#include "postgres.h"
#include "fmgr.h"
#include "access/cmapi.h"
#include "commands/defrem.h"
#include "nodes/parsenodes.h"
#include "utils/builtins.h"
#include "utils/datum.h"

PG_MODULE_MAGIC;

/* borh tree */
typedef struct bnode bnode;
struct bnode
{
	char	code;
	int		nnodes;
	bnode **nodes;
	bool	leaf;
};

/* compression options state */
typedef struct
{
	Size	nwords;
	char  **words;
} dict_state;

PG_FUNCTION_INFO_V1( dict_compression_handler );

static void
dict_check(Form_pg_attribute att, List *options)
{
	Size		nwords = 0;
	char	   *val;
	DefElem    *def;

	if (list_length(options) != 1)
		elog(ERROR, "incorrect number of options");

	def = (DefElem *) linitial(options);

	if (strcmp(def->defname, "dict") != 0)
		elog(ERROR, "unknown option '%s'", def->defname);

	val = defGetString(def);
	while (strtok(val, " ") != NULL)
		nwords++;

	if (nwords < 1 || nwords > 254)
		elog(ERROR, "correct number of words should be specified (between 1 and 254)");
}

static void *
dict_initstate(Oid acoid, List *options)
{
	dict_state *state = (dict_state *) palloc0(sizeof(dict_state));
	DefElem	   *def = (DefElem *) linitial(options);
	char	   *val,
			   *tok;
	int			i = 0;

	val = defGetString(def);
	while (strtok(val, " ") != NULL)
		state->nwords++;

	state->words = palloc(sizeof(char *) * state->nwords);
	while ((tok = strtok(val, " ")) != NULL)
		state->words[i++] = tok;

	return (void *) state;
}

static bytea *
dict_compress(CompressionAmOptions *cmoptions, const bytea *value)
{
	return NULL;
}

static bytea *
dict_decompress(CompressionAmOptions *cmoptions, const bytea *value)
{
	return NULL;
}

Datum
dict_compression_handler(PG_FUNCTION_ARGS)
{
	CompressionAmRoutine *routine = makeNode(CompressionAmRoutine);

	routine->cmcheck = dict_check;
	routine->cmdrop = NULL;
	routine->cminitstate = dict_initstate;
	routine->cmcompress = dict_compress;
	routine->cmdecompress = dict_decompress;

	PG_RETURN_POINTER(routine);
}
