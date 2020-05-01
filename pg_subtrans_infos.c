#include "postgres.h"
#include "miscadmin.h"
#include "access/twophase.h"
#include "access/hash.h"
#include "funcapi.h"
#include "utils/builtins.h"
#include "access/subtrans.h"
#include "access/commit_ts.h"
#include "access/transam.h"
#if PG_VERSION_NUM < 110000
#include "access/xact.h"
#endif

PG_MODULE_MAGIC;
Datum pg_subtrans_infos(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1(pg_subtrans_infos);

/*
 * See txid.c
 */
static bool
TransactionIdInRecentPast(uint64 xid_with_epoch, TransactionId *extracted_xid)
{
	uint32		xid_epoch = (uint32) (xid_with_epoch >> 32);
	TransactionId xid = (TransactionId) xid_with_epoch;
	uint32		now_epoch;
	TransactionId now_epoch_next_xid;

#if PG_VERSION_NUM >= 120000
	FullTransactionId now_fullxid;
	now_fullxid = ReadNextFullTransactionId();
	now_epoch_next_xid = XidFromFullTransactionId(now_fullxid);
	now_epoch = EpochFromFullTransactionId(now_fullxid);
#else
	GetNextXidAndEpoch(&now_epoch_next_xid, &now_epoch);
#endif

	if (extracted_xid != NULL)
		*extracted_xid = xid;

	if (!TransactionIdIsValid(xid))
		return false;

	/* For non-normal transaction IDs, we can ignore the epoch. */
	if (!TransactionIdIsNormal(xid))
		return true;

	/* If the transaction ID is in the future, throw an error. */
#if PG_VERSION_NUM >= 120000
	if (xid_with_epoch >= U64FromFullTransactionId(now_fullxid))
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 errmsg("transaction ID %s is in the future",
						psprintf(UINT64_FORMAT, xid_with_epoch))));
#else
	if (xid_epoch > now_epoch
		|| (xid_epoch == now_epoch && xid >= now_epoch_next_xid))
		ereport(ERROR,
			(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				errmsg("transaction ID %s is in the future",
					psprintf(UINT64_FORMAT, xid_with_epoch))));
#endif

	/*
	 * ShmemVariableCache->oldestClogXid is protected by CLogTruncationLock,
	 * but we don't acquire that lock here.  Instead, we require the caller to
	 * acquire it, because the caller is presumably going to look up the
	 * returned XID.  If we took and released the lock within this function, a
	 * CLOG truncation could occur before the caller finished with the XID.
	 */
	Assert(LWLockHeldByMe(CLogTruncationLock));

	/*
	 * If the transaction ID has wrapped around, it's definitely too old to
	 * determine the commit status.  Otherwise, we can compare it to
	 * ShmemVariableCache->oldestClogXid to determine whether the relevant
	 * CLOG entry is guaranteed to still exist.
	 */
	if (xid_epoch + 1 < now_epoch
		|| (xid_epoch + 1 == now_epoch && xid < now_epoch_next_xid)
		|| TransactionIdPrecedes(xid, ShmemVariableCache->oldestClogXid))
		return false;

	return true;
}

Datum 
pg_subtrans_infos(PG_FUNCTION_ARGS)
{
	uint64          xid_with_epoch = PG_GETARG_INT64(0);
	TransactionId xid;
	TransactionId parentxid;
	TransactionId topparentxid;
	ReturnSetInfo   *rsinfo = (ReturnSetInfo *) fcinfo->resultinfo;
	char *status;
	TupleDesc           tupdesc;
	Tuplestorestate *tupstore;
	MemoryContext   per_query_ctx;
	MemoryContext   oldcontext;
	Datum           values[5];
	bool            nulls[5] = {0};
	TimestampTz ts;
	bool            found = false;

	/*
	 * for commit_timestamp
	 */
	nulls[4]=true;

	per_query_ctx = rsinfo->econtext->ecxt_per_query_memory;
	oldcontext = MemoryContextSwitchTo(per_query_ctx);

	/*
	 * Build a tuple descriptor for our result type
	 */
	if (get_call_result_type(fcinfo, NULL, &tupdesc) != TYPEFUNC_COMPOSITE)
		elog(ERROR, "return type must be a row type");
	tupstore = tuplestore_begin_heap(true, false, work_mem);
	rsinfo->returnMode = SFRM_Materialize;
	rsinfo->setResult = tupstore;
	rsinfo->setDesc = tupdesc;
	MemoryContextSwitchTo(oldcontext);

	LWLockAcquire(CLogTruncationLock, LW_SHARED);
	if (TransactionIdInRecentPast(xid_with_epoch, &xid))
	{
		Assert(TransactionIdIsValid(xid));

		parentxid = SubTransGetParent(xid);
		topparentxid = SubTransGetTopmostTransaction(xid);

		if (!TransactionIdIsValid(parentxid))
			nulls[2]=true;
		else
			values[2]=parentxid;

		if (!TransactionIdIsValid(topparentxid) || topparentxid == xid)
			nulls[3]=true;
		else
			values[3]=topparentxid;

		if (TransactionIdIsCurrentTransactionId(xid))
			status = "in progress";
		else if (TransactionIdDidCommit(xid)) {
			status = "committed";
			if (track_commit_timestamp)
				found = TransactionIdGetCommitTsData(xid, &ts, NULL);
			if (found) {
				values[4]=TimestampTzGetDatum(ts);
				nulls[4]=false;
			}
		}
		else if (TransactionIdDidAbort(xid))
			status = "aborted";
		else
		{
			/*
			 * The xact is not marked as either committed or aborted in clog.
			 *
			 * It could be a transaction that ended without updating clog or
			 * writing an abort record due to a crash. We can safely assume
			 * it's aborted if it isn't committed and is older than our
			 * snapshot xmin.
			 *
			 * Otherwise it must be in-progress (or have been at the time we
			 * checked commit/abort status).
			 */
			if (TransactionIdPrecedes(xid, GetActiveSnapshot()->xmin))
				status = "aborted";
			else
				status = "in progress";
		}
	}
	else
	{
		status = NULL;
	}

	LWLockRelease(CLogTruncationLock);

	if (status == NULL) {
		nulls[1]=true;
		nulls[2]=true;
		nulls[3]=true;
		nulls[4]=true;
	}
	else
		values[1]=CStringGetTextDatum(status);

	values[0]=xid;
	tuplestore_putvalues(tupstore, tupdesc, values, nulls);
	tuplestore_donestoring(tupstore);
	return (Datum) 0;
}
