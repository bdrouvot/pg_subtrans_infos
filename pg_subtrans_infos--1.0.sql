CREATE FUNCTION pg_subtrans_infos(
	IN bigint,
	OUT xid integer,
	OUT status text,
	OUT parent_xid integer,
	OUT top_parent_xid integer,
	OUT commit_timestamp timestamptz)
RETURNS SETOF RECORD
AS 'MODULE_PATHNAME','pg_subtrans_infos'
LANGUAGE C STRICT VOLATILE;
