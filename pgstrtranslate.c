#include <postgres.h>
#include <fmgr.h>
#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif
#include <lib/stringinfo.h>
#include <utils/array.h>
#include <utils/builtins.h>
#include <catalog/pg_type_d.h>

PG_FUNCTION_INFO_V1(pgstrtranslate);
Datum pgstrtranslate(PG_FUNCTION_ARGS);

PG_FUNCTION_INFO_V1(pgstrarrayremove);
Datum pgstrarrayremove(PG_FUNCTION_ARGS);

static void pgstrtranslate_fullsearch(char **t, int search_count,
	const bool *search_nulls, const bool *replacement_nulls,
	const Datum *search_datums, const Datum *replacement_datums);

static void pgstrtranslate_distinct(char **t, int search_count,
	const bool *search_nulls, const bool *replacement_nulls,
	const Datum *search_datums, const Datum *replacement_datums);

#define PGTRANSLATETOKENCHUNK 5

typedef struct _pgstrtranslate_token {
	bool solved;
	char *s;
	int tokencount;
	int chunksize;
	struct _pgstrtranslate_token* tokenarr;	
} pgstrtranslate_token;

static void pgstrtranslate_token_crunch(pgstrtranslate_token *ptoken, const char *search, const char *replace);	
static char* pgstrtranslate_token_compose(pgstrtranslate_token *ptoken);
static void pgstrtranslate_token_init(pgstrtranslate_token *ptoken, char* s);
static void pgstrtranslate_token_enlarge(pgstrtranslate_token *ptoken);

Datum
pgstrtranslate(PG_FUNCTION_ARGS)
{
	bool fullsearch;
	ArrayType *search_arr, *replacement_arr;
	int sdims, rdims;
	Datum *search_datums, *replacement_datums;
	bool *search_nulls, *replacement_nulls;
	int search_count, replacement_count;
	text *t;	
	text *result;	
	char *c;

	t = PG_GETARG_TEXT_PP(1);
	c = text_to_cstring(t);
	
	search_arr = PG_GETARG_ARRAYTYPE_P(2);
	sdims = ARR_NDIM(search_arr);

	replacement_arr = PG_GETARG_ARRAYTYPE_P(3);
	rdims = ARR_NDIM(replacement_arr);

	if(strlen(c)>0 && sdims>0 && rdims>0) {
		fullsearch = PG_GETARG_BOOL(0);		
	
		if(sdims>1 || sdims!=rdims)
			ereport(ERROR,
					(errcode(ERRCODE_ARRAY_SUBSCRIPT_ERROR),
					 errmsg("/*both search and replacement should be one dimension array*/")));
	
		deconstruct_array(search_arr,
			TEXTOID, -1, false, 'i',
			&search_datums, &search_nulls, &search_count);

		deconstruct_array(replacement_arr,
			TEXTOID, -1, false, 'i',
			&replacement_datums, &replacement_nulls, &replacement_count);

		if (search_count != replacement_count)
			ereport(ERROR,
					(errcode(ERRCODE_ARRAY_SUBSCRIPT_ERROR),
					 errmsg("/*mismatched array dimensions*/")));	
		if( fullsearch )
			pgstrtranslate_fullsearch(&c, search_count, search_nulls, replacement_nulls, search_datums, replacement_datums);
		else
			pgstrtranslate_distinct(&c, search_count, search_nulls, replacement_nulls, search_datums, replacement_datums);
	} //if(strlen(c)>0)	
	result = cstring_to_text(c);
	PG_RETURN_TEXT_P(result);
}

static void pgstrtranslate_fullsearch(char **t, int search_count,
	const bool *search_nulls, const bool *replacement_nulls,
	const Datum *search_datums, const Datum *replacement_datums) {
	
	StringInfoData buff;
	int i;
	char *s, *r, *run, *p;

	initStringInfo( &buff );		
	for(i=0; i<search_count; i++) {
		if( !search_nulls[i] && !replacement_nulls[i] ) {
			s = TextDatumGetCString(search_datums[i]);
			if(strlen(s)>0) {
				r = TextDatumGetCString(replacement_datums[i]);				
				run = *t;
				do {
					p = strstr(run, s);
					if( p!=NULL ) {
						appendBinaryStringInfo( &buff, run, p - run);
						appendBinaryStringInfo( &buff, r, strlen(r));
						run = p + strlen( s );
					} else
						appendStringInfoString( &buff, run );
				} while( p!=NULL );		
				if( buff.len>0 ) {
					pfree( *t );
					*t = pstrdup( buff.data );
					resetStringInfo( &buff );	
				}
			}
		}
	}
	pfree(buff.data);
	return;	
}

static void pgstrtranslate_distinct(char **t, int search_count,
	const bool *search_nulls, const bool *replacement_nulls,
	const Datum *search_datums, const Datum *replacement_datums) {
	
	int i;
	pgstrtranslate_token token;
	char *s, *r;
	
	pgstrtranslate_token_init(&token, pstrdup(*t));		
	for(i=0; i<search_count; i++) {
		if( !search_nulls[i] && !replacement_nulls[i] ) {
			s = TextDatumGetCString(search_datums[i]);
			if( strlen(s)>0 ) {
				r = TextDatumGetCString(replacement_datums[i]);				
				pgstrtranslate_token_crunch(&token, s, r);
			}
		}
	}
	
	*t = pgstrtranslate_token_compose(&token);	
	return;	
}

static void pgstrtranslate_token_crunch(pgstrtranslate_token* ptoken, const char *search, const char *replace) {
	int i, idx;
	char *p, *run;
	pgstrtranslate_token *tkn;

	if( ptoken->solved )
		return;

	if( ptoken->tokencount > 0 ) {
		for( i=0; i<ptoken->tokencount; i++ )
			pgstrtranslate_token_crunch(&ptoken->tokenarr[i], search, replace);//recursive
		return;
	}
	
	run = ptoken->s;
	do {
		if((p = strstr(run, search))!=NULL) {
			idx = ptoken->tokencount++;
			if(run < p) {
				ptoken->tokencount++;
				pgstrtranslate_token_enlarge(ptoken);
				tkn = &ptoken->tokenarr[idx];	
				pgstrtranslate_token_init(tkn, pnstrdup(run, p-run));
				idx++;
			}
			pgstrtranslate_token_enlarge(ptoken);
			tkn = &ptoken->tokenarr[idx];
			pgstrtranslate_token_init(tkn, pstrdup(replace));
			tkn->solved = true;
			run = p + strlen(search);
		} else {
			if( run > ptoken->s ) {	
				idx = ptoken->tokencount++;
				pgstrtranslate_token_enlarge(ptoken);
				tkn = &ptoken->tokenarr[idx];
				pgstrtranslate_token_init(tkn, pstrdup(run));
			}
		}
	} while( p!=NULL );
	
	if( run > ptoken->s ) {	
		pfree(ptoken->s);
		ptoken->s = NULL;
	}
	
	return;
}

static char* pgstrtranslate_token_compose(pgstrtranslate_token *ptoken) {
	StringInfoData buff;
	int i;
	pgstrtranslate_token *tkn;
	char *s;	

	if( ptoken->tokencount==0 )
		return ptoken->s;

	initStringInfo(&buff);	
	for(i=0; i < ptoken->tokencount; i++) {
		tkn = &ptoken->tokenarr[i];
		s = pgstrtranslate_token_compose(tkn);
		appendStringInfoString(&buff, s);
		pfree(s);
	}
	
	pfree(ptoken->tokenarr);

	return buff.data;
}

static void pgstrtranslate_token_init(pgstrtranslate_token *ptoken, char* s) {
	ptoken->solved = false;
	ptoken->s = s;
	ptoken->tokencount = 0;
	ptoken->chunksize = 0;	
	ptoken->tokenarr = NULL;
	return;
}

static void pgstrtranslate_token_enlarge(pgstrtranslate_token *ptoken) {
	if(ptoken->chunksize < ptoken->tokencount) {
		ptoken->chunksize += PGTRANSLATETOKENCHUNK;
		ptoken->tokenarr = ptoken->tokenarr==NULL ? (pgstrtranslate_token*)palloc(ptoken->chunksize * sizeof(pgstrtranslate_token))
			: (pgstrtranslate_token*)repalloc((void*)ptoken->tokenarr, ptoken->chunksize * sizeof(pgstrtranslate_token));
	}
	return;
}

Datum
pgstrarrayremove(PG_FUNCTION_ARGS)
{
	ArrayType *search_arr, *remove_arr;
	int sdims, rdims;
	Datum *search_datums, *remove_datums, *rsltdatums;
	bool *search_nulls, *remove_nulls, *rsltnulls;
	ArrayType  *rslt;
	int search_count, remove_count, i, j, count;
    int dims[1];
    int lbs[1];
	char *s, **removes;
	bool hit;
			
	search_arr = PG_GETARG_ARRAYTYPE_P(0);
	sdims = ARR_NDIM(search_arr);

	remove_arr = PG_GETARG_ARRAYTYPE_P(1);
	rdims = ARR_NDIM(remove_arr);
	
	rslt = search_arr;
	
	if(sdims>0 && rdims>0) {
		if(sdims>1 || sdims!=rdims)
			ereport(ERROR,
				(errcode(ERRCODE_ARRAY_SUBSCRIPT_ERROR),
				 errmsg("/*both search and replacement should be one dimension array*/")));		
	
		deconstruct_array(search_arr,
			TEXTOID, -1, false, 'i',
			&search_datums, &search_nulls, &search_count);

		deconstruct_array(remove_arr,
			TEXTOID, -1, false, 'i',
			&remove_datums, &remove_nulls, &remove_count);
			
		if( search_count>0 && remove_count>0 ) {
			rsltdatums = (Datum *) palloc(sizeof(Datum) * search_count);
			rsltnulls = (bool *) palloc(sizeof(bool) * search_count);
			count = 0;
			removes = NULL;
			for(i=0;i<search_count;i++) {	
				if(search_nulls[i]){
					rsltnulls[count] = true;	
					rsltdatums[count] = search_datums[i];
					count++;
				} else {					
					if( removes==NULL ) {
						removes = (char**)palloc(remove_count*sizeof(char*));
						for(j=0;j<remove_count;j++) {
							if(remove_nulls[i])
								removes[j]=NULL;
							else
								removes[j]=TextDatumGetCString(remove_datums[j]);
						}
					}
					hit = false;	
					s = TextDatumGetCString(search_datums[i]);				
					for(j=0;j<remove_count;j++) {
						if(removes[j]!=NULL) {
							hit = strcmp(s, removes[j])==0;
							if(hit)
								break;
						}
					}
					if(!hit) {
						rsltnulls[count] = false;	
						rsltdatums[count] = search_datums[i];
						count++;
					}
				}//if!(search_nulls[i]){
			}//for(i=0;i<search_count;i++) {		

			dims[0] = count;
			lbs[0] = 1;			
			rslt = construct_md_array(rsltdatums, rsltnulls, 1, dims, lbs, TEXTOID, -1, false, 'i');
		}//if( search_count>0 && remove_count>0 ) {
	} //if( sdims>0 )		
	
	PG_RETURN_POINTER(rslt);
}
