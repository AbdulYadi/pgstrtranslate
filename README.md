# postgresql extended string translation
PostgreSQL provides a built-in function for character wise string replacement:
~~~
select translate('abcdefghijkl', 'ace', '123');
  translate   
--------------
 1b2d3fghijkl
~~~

<b>pgstrtranslate</b> extends it with multi-character replacement. It takes 4 arguments and returning text.
~~~
CREATE OR REPLACE FUNCTION public.pgstrtranslate(
    recursive boolean,
    t text,
    search text[],
    replacement text[])
  RETURNS text AS
'$libdir/pgstrtranslate', 'pgstrtranslate'
  LANGUAGE c IMMUTABLE STRICT;
~~~

How it works:

### Non-recursive replacement:
~~~
select pgstrtranslate(false, 'abcdefghijkl', array['ab', 'efg', '2cd']::text[], array['012', '3', '78']::text[]);
  translate   
--------------
 012cd3hijkl
~~~
Note that '2cd' does not match original string.

### Recursive replacement:
~~~
select pgstrtranslate(true, 'abcdefghijkl', array['ab', 'efg', '2cd']::text[], array['012', '3', '78']::text[]);
  translate   
--------------
 01783hijkl
~~~
Replace 'ab' with '012': 'abcdefghijkl' -> '012cdefghijkl'<br />
Replace 'efg' with '3': '012cdefghijkl' -> '012cd3hijkl'<br />
Replace '2cd' with '78': '012cd3hijkl' -> '01783hijkl'<br />

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
