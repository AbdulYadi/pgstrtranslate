# postgresql extended string translation
PostgreSQL provides a built-in function for character wise string replacement:
~~~
select translate('abcdefghijkl', 'ace', '123');
  translate   
--------------
 1b2d3fghijkl
~~~

<b>pgstrtranslate</b> extends it with multi-character replacement. It takes 4 arguments and returning a text.
~~~
CREATE OR REPLACE FUNCTION public.pgstrtranslate(
    fullsearch boolean,
    t text,
    search text[],
    replacement text[])
  RETURNS text AS
'$libdir/pgstrtranslate', 'pgstrtranslate'
  LANGUAGE c IMMUTABLE STRICT;
~~~

## How it works:

### Non-fullsearch replacement:
~~~
select pgstrtranslate(false, --non-fullsearch
	'abcdefghijkl', --original string
	array['ab', 'efg', '2cd']::text[], --array of searchs
	array['012', '3', '78']::text[]); --array of replacement
  translate   
--------------
 012cd3hijkl
~~~
'<b>ab</b>cd<b>efg</b>hijkl' -> '<b>012</b>cd<b>3</b>hijkl'<br />
Note that '2cd' does not match original string.

### Fullsearch replacement:
~~~
select pgstrtranslate(true, --fullsearch
	'abcdefghijkl', --original string
	array['ab', 'efg', '2cd']::text[], --array of searchs
	array['012', '3', '78']::text[]); --array of replacement
  translate   
--------------
 01783hijkl
~~~
Replace 'ab' with '012': '<b>ab</b>cdefghijkl' -> '<b>012</b>cdefghijkl'<br />
Replace 'efg' with '3': '012cd<b>efg</b>hijkl' -> '012cd<b>3</b>hijkl'<br />
Replace '2cd' with '78': '01<b>2cd</b>3hijkl' -> '01<b>78</b>3hijkl'<br />

## How to install
1. Clone or download source code from https://github.com/AbdulYadi/pgstrtranslate.git. Extract it.
2. If necessary, modify PG_CONFIG path according to your specific PostgreSQL installation location.
3. Build as usual:
~~~
$ make
$ make install
~~~
4. On successful compilation, install this extension in PostgreSQL environment
~~~
$ create extension pgstrtranslate;
~~~
