-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION pgstrtranslate" to load this file. \quit

CREATE OR REPLACE FUNCTION pgstrtranslate(fullsearch boolean, t text, search text[], replacement text[])
RETURNS text
AS 'MODULE_PATHNAME' LANGUAGE C IMMUTABLE STRICT;
