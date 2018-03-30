#include "access/cmapi.h"
#include "access/tuptoaster.h"
#include "catalog/pg_type.h"
#include "commands/defrem.h"
#include "fmgr.h"
#include "nodes/parsenodes.h"
#include "postgres.h"
#include "utils/builtins.h"
#include "utils/datum.h"

PG_MODULE_MAGIC;
PG_FUNCTION_INFO_V1( dict_compression_handler );
PG_FUNCTION_INFO_V1( dict_compression_test );

#define DELIM (" \xFF,")

/* borh tree for aho-corasick algorithm */
typedef struct bnode bnode;
struct bnode
{
	int		next[255];	/* indexes for each char in dict_state.nodes or -1 */
	int		level;
	uint8	code;		/* char of this node */
	int		idx;		/* index of token in tokens */
	bnode  *parent;		/* parent of this node */
	bnode  *link;		/* suffix link */
};

/* compression options state */
typedef struct
{
	int		ntokens;
	char  **tokens;
	int	   *lengths;
	bnode  *nodes;
	int		nnodes;
	int		nallocated;
} dict_state;

bnode *get_suffix_node(dict_state *state, bnode *node);
int get_goto_link(dict_state *state, bnode *node, uint8 c);

bnode *
get_suffix_node(dict_state *state, bnode *node)
{
	if (node->link == NULL)
	{
		if (node->parent == NULL || node->parent->parent == NULL)
			node->link = &state->nodes[0];	/* first two levels to root */
		else
		{
			bnode  *psuffix = get_suffix_node(state, node->parent);
			int		g = psuffix->next[node->code];
			if (g != -1)
				node->link = &state->nodes[g];
			else
				node->link = &state->nodes[0];
		}
	}

	return node->link;
}

static int
make_bnode(dict_state *state, uint8 code, int level)
{
	int		loc;
	bnode  *curnode;

	/* Extend nodes array if needed */
	if (state->nnodes == state->nallocated)
	{
		state->nallocated += 10;
		if (state->nodes == NULL)
			state->nodes = palloc(sizeof(bnode) * state->nallocated);
		else
			state->nodes = repalloc(state->nodes,
									sizeof(bnode) * state->nallocated);
	}

	/* Init basic state for the node, set indexes to -1 */
	loc = state->nnodes++;
	curnode = &state->nodes[loc];
	curnode->idx = -1;
	curnode->code = code;
	curnode->link = NULL;
	curnode->parent = NULL;
	curnode->level = level;

	/* initialize by -1 values */
	memset(curnode->next, 0xFF, sizeof(curnode->next));
	return loc;
}

static void
dict_check(Form_pg_attribute att, List *options)
{
	int			ntokens = 0;
	char	   *val,
			   *tok;
	DefElem    *def;

	/* TODO: check binary compatible types too */
	if (att->attlen != -1 || att->attstorage == 'p')
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 (errmsg("dictionary compression is support for only variable length attributes"))));

	if (list_length(options) != 1)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 (errmsg("incorrect number of options"))));

	def = (DefElem *) linitial(options);

	if (strcmp(def->defname, "dict") != 0)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 (errmsg("unknown option '%s'", def->defname))));

	val = pstrdup(defGetString(def));
	while ((tok = strtok(val, DELIM)) != NULL)
	{
		if (strlen(tok) < 2)
			ereport(ERROR,
					(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
					 (errmsg("dictionary token length should more than 1"))));

		ntokens++;
		val = NULL;
	}

	if (ntokens < 1 || ntokens > 254)
		ereport(ERROR,
				(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
				 (errmsg("correct number of tokens should be specified (between 1 and 254)"))));
}

/*
 * Make borh tree from specified dictionary
 */
static void
dict_append_tokens(dict_state *state)
{
	int		i;
	int		k;

	for (i = 0; i < state->ntokens; i++)
	{
		char	*token = state->tokens[i];
		bnode	*curnode = &state->nodes[0];

		for (k = 0; k < strlen(token); k++) {
			uint8 c = (uint8) token[k];

			/* 0xFF is reserved for mark of compressed token */
			Assert(c < 255);
			if (curnode->next[c] == -1)
			{
				/* Create child node for this symbol */
				curnode->next[c] = make_bnode(state, c, curnode->level + 1);
				state->nodes[curnode->next[c]].parent = curnode;
			}

			curnode = &state->nodes[curnode->next[c]];
		}

		curnode->idx = i;
		Assert(curnode->level == strlen(token));
	}
}

static void *
dict_initstate(Oid acoid, List *options)
{
	dict_state *state = (dict_state *) palloc0(sizeof(dict_state));
	DefElem	   *def = (DefElem *) linitial(options);
	char	   *val,
			   *tok;
	int			i = 0;
	List	   *tokens = NIL;
	ListCell   *lc;

	/* Parse dict words and save them in the state */
	val = pstrdup(defGetString(def));
	tokens = lappend(tokens, strtok(val, DELIM));

	while ((tok = strtok(NULL, DELIM)) != NULL)
		tokens = lappend(tokens, tok);

	Assert(list_length(tokens) < 255);
	state->ntokens = (uint8) list_length(tokens);
	state->tokens = palloc(sizeof(char *) * state->ntokens);
	state->lengths = palloc(sizeof(int) * state->ntokens);

	foreach(lc, tokens)
	{
		state->tokens[i] = (char *) lfirst(lc);
		state->lengths[i] = strlen(state->tokens[i]);
		i++;
	}
	list_free(tokens);

	/* Create root of borh tree */
	i = make_bnode(state, 0, 0);
	Assert(i == 0);

	/* Append tokens from dictionary to tree */
	dict_append_tokens(state);

	return (void *) state;
}

static bytea *
dict_compress(CompressionAmOptions *cmoptions, const bytea *value)
{
#define DEST_ADD(c) \
	if (dpos >= srclen - 1) { pfree(res); return NULL; } \
	dest[dpos++] = 0xFF; \
	dest[dpos++] = (uint8) c;

	int			pos = 0,
				initpos = 0,
				dpos = 0;
	int			srclen = (Size) VARSIZE_ANY_EXHDR(value);
	dict_state *state = (dict_state *) cmoptions->acstate;
	uint8	   *src = (uint8 *) VARDATA(value);
	bytea	   *res = palloc0(srclen + VARHDRSZ_CUSTOM_COMPRESSED);
	uint8	   *dest = (uint8 *) res + VARHDRSZ_CUSTOM_COMPRESSED;
	bnode	   *curnode = &state->nodes[0];

	while (initpos < srclen)
	{
		int		level,
				copylen;
		uint8	code;

		if (curnode->idx != -1)
		{
			/* we found a match */
			DEST_ADD(curnode->idx);
			curnode = &state->nodes[0];	/* start from the root */
			initpos = pos;
			continue;
		}

		if (pos >= srclen)
			goto rem;

		/* escape 0xFF */
		if (code = src[pos], code == 0xFF)
		{
			DEST_ADD(0xFF);
			pos++;
			initpos = pos;
			continue;
		}


		if (curnode->next[code] != -1)
		{
			/* we found a child, change the current state to it */
			curnode = &state->nodes[curnode->next[code]];
			pos++;
			continue;
		}

		level = curnode->level;
		curnode = get_suffix_node(state, curnode);
		if (curnode->level)
		{
			int diff;
			diff = level - curnode->level;

			/* we found a suffix node, copy beginning of previous prefix */
			Assert(diff > 0);
			memcpy(&dest[dpos], &src[initpos], diff);
			dpos += diff;
			initpos += diff;
			continue;
		}

		/* prefix didn't match */
		pos++;
rem:
		copylen = (pos - initpos);
		if (dpos + copylen > srclen)
		{
			pfree(res);
			return NULL;
		}

		memcpy(&dest[dpos], &src[initpos], copylen);
		dpos += copylen;
		initpos = pos;
	}

	SET_VARSIZE_COMPRESSED(res, dpos + VARHDRSZ_CUSTOM_COMPRESSED);
	return res;
}

static bytea *
dict_decompress(CompressionAmOptions *cmoptions, const bytea *value)
{
	dict_state *state = (dict_state *) cmoptions->acstate;

	int		srclen = VARSIZE_ANY(value) - VARHDRSZ_CUSTOM_COMPRESSED;
	int		dstlen = VARRAWSIZE_4B_C(value);
	int		pos = 0,
			dpos = 0;
	bytea  *res;
	uint8  *src = (uint8 *) value + VARHDRSZ_CUSTOM_COMPRESSED;
	uint8  *dst;

	res = (bytea *) palloc(dstlen + VARHDRSZ);
	dst = (uint8 *) VARDATA(res);

	SET_VARSIZE(res, dstlen + VARHDRSZ);
	while (pos < srclen)
	{
		if (src[pos] == 0xFF)
		{
			pos++;

			/* is it escaped 0xFF? */
			if (src[pos] == 0xFF)
				dst[dpos++] = 0xFF;
			else
			{
				char *token = state->tokens[src[pos]];
				int len = state->lengths[src[pos]];

				Assert(token != NULL);
				memcpy(&dst[dpos], token, len);
				dpos += len;
			}
			pos++;
		}
		else
			dst[dpos++] = src[pos++];
	}

	/* check correctness */
	Assert(pos == srclen && dpos == dstlen);
	return res;
}

Datum
dict_compression_test(PG_FUNCTION_ARGS)
{
	Oid		 acoid = PG_GETARG_OID(0);
	text	*t = PG_GETARG_TEXT_PP(1);
	CompressionAmOptions	*amoptions = lookup_compression_am_options(acoid);
	bytea	*res = dict_compress(amoptions, t);
	ereport(NOTICE,
			(errcode(ERRCODE_INVALID_PARAMETER_VALUE),
			 (errmsg("len: %lu", VARSIZE(res) - VARHDRSZ_CUSTOM_COMPRESSED))));

	toast_set_compressed_datum_info(res, amoptions->acoid,
								VARSIZE_ANY_EXHDR(DatumGetPointer(t)));
	PG_RETURN_TEXT_P(res);
}

Datum
dict_compression_handler(PG_FUNCTION_ARGS)
{
	CompressionAmRoutine *routine = makeNode(CompressionAmRoutine);

	routine->cmcheck = dict_check;
	routine->cminitstate = dict_initstate;
	routine->cmcompress = dict_compress;
	routine->cmdecompress = dict_decompress;

	PG_RETURN_POINTER(routine);
}
