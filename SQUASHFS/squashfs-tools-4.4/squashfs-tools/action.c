/*
 * Create a squashfs filesystem.  This is a highly compressed read only
 * filesystem.
 *
 * Copyright (c) 2011, 2012, 2013, 2014
 * Phillip Lougher <phillip@squashfs.org.uk>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * action.c
 */

#include <fcntl.h>
#include <dirent.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <sys/wait.h>
#include <regex.h>
#include <limits.h>
#include <errno.h>

#include "squashfs_fs.h"
#include "mksquashfs.h"
#include "action.h"
#include "error.h"
#include "fnmatch_compat.h"

/*
 * code to parse actions
 */

static char *cur_ptr, *source;
static struct action *fragment_spec = NULL;
static struct action *exclude_spec = NULL;
static struct action *empty_spec = NULL;
static struct action *move_spec = NULL;
static struct action *prune_spec = NULL;
static struct action *other_spec = NULL;
static int fragment_count = 0;
static int exclude_count = 0;
static int empty_count = 0;
static int move_count = 0;
static int prune_count = 0;
static int other_count = 0;
static struct action_entry *parsing_action;

static struct file_buffer *def_fragment = NULL;

static struct token_entry token_table[] = {
	{ "(", TOK_OPEN_BRACKET, 1, },
	{ ")", TOK_CLOSE_BRACKET, 1 },
	{ "&&", TOK_AND, 2 },
	{ "||", TOK_OR, 2 },
	{ "!", TOK_NOT, 1 },
	{ ",", TOK_COMMA, 1 },
	{ "@", TOK_AT, 1},
	{ " ", 	TOK_WHITE_SPACE, 1 },
	{ "\t ", TOK_WHITE_SPACE, 1 },
	{ "", -1, 0 }
};


static struct test_entry test_table[];

static struct action_entry action_table[];

static struct expr *parse_expr(int subexp);

extern char *pathname(struct dir_ent *);

extern char *subpathname(struct dir_ent *);

extern int read_file(char *filename, char *type, int (parse_line)(char *));

/*
 * Lexical analyser
 */
#define STR_SIZE 256

static int get_token(char **string)
{
	/* string buffer */
	static char *str = NULL;
	static int size = 0;

	char *str_ptr;
	int cur_size, i, quoted;

	while (1) {
		if (*cur_ptr == '\0')
			return TOK_EOF;
		for (i = 0; token_table[i].token != -1; i++)
			if (strncmp(cur_ptr, token_table[i].string,
						token_table[i].size) == 0)
				break;
		if (token_table[i].token != TOK_WHITE_SPACE)
			break;
		cur_ptr ++;
	}

	if (token_table[i].token != -1) {
		cur_ptr += token_table[i].size;
		return token_table[i].token;
	}

	/* string */
	if(str == NULL) {
		str = malloc(STR_SIZE);
		if(str == NULL)
			MEM_ERROR();
		size = STR_SIZE;
	}

	/* Initialise string being read */
	str_ptr = str;
	cur_size = 0;
	quoted = 0;

	while(1) {
		while(*cur_ptr == '"') {
			cur_ptr ++;
			quoted = !quoted;
		}

		if(*cur_ptr == '\0') {
			/* inside quoted string EOF, otherwise end of string */
			if(quoted)
				return TOK_EOF;
			else
				break;
		}

		if(!quoted) {
			for(i = 0; token_table[i].token != -1; i++)
				if (strncmp(cur_ptr, token_table[i].string,
						token_table[i].size) == 0)
					break;
			if (token_table[i].token != -1)
				break;
		}

		if(*cur_ptr == '\\') {
			cur_ptr ++;
			if(*cur_ptr == '\0')
				return TOK_EOF;
		}

		if(cur_size + 2 > size) {
			char *tmp;

			size = (cur_size + 1  + STR_SIZE) & ~(STR_SIZE - 1);

			tmp = realloc(str, size);
			if(tmp == NULL)
				MEM_ERROR();

			str_ptr = str_ptr - str + tmp;
			str = tmp;
		}

		*str_ptr ++ = *cur_ptr ++;
		cur_size ++;
	}

	*str_ptr = '\0';
	*string = str;
	return TOK_STRING;
}


static int peek_token(char **string)
{
	char *saved = cur_ptr;
	int token = get_token(string);

	cur_ptr = saved;

	return token;
}


/*
 * Expression parser
 */
static void free_parse_tree(struct expr *expr)
{
	if(expr->type == ATOM_TYPE) {
		int i;

		for(i = 0; i < expr->atom.test->args; i++)
			free(expr->atom.argv[i]);

		free(expr->atom.argv);
	} else if (expr->type == UNARY_TYPE)
		free_parse_tree(expr->unary_op.expr);
	else {
		free_parse_tree(expr->expr_op.lhs);
		free_parse_tree(expr->expr_op.rhs);
	}

	free(expr);
}


static struct expr *create_expr(struct expr *lhs, int op, struct expr *rhs)
{
	struct expr *expr;

	if (rhs == NULL) {
		free_parse_tree(lhs);
		return NULL;
	}

	expr = malloc(sizeof(*expr));
	if (expr == NULL)
		MEM_ERROR();

	expr->type = OP_TYPE;
	expr->expr_op.lhs = lhs;
	expr->expr_op.rhs = rhs;
	expr->expr_op.op = op;

	return expr;
}


static struct expr *create_unary_op(struct expr *lhs, int op)
{
	struct expr *expr;

	if (lhs == NULL)
		return NULL;

	expr = malloc(sizeof(*expr));
	if (expr == NULL)
		MEM_ERROR();

	expr->type = UNARY_TYPE;
	expr->unary_op.expr = lhs;
	expr->unary_op.op = op;

	return expr;
}


static struct expr *parse_test(char *name)
{
	char *string, **argv = NULL;
	int token, args = 0;
	int i;
	struct test_entry *test;
	struct expr *expr;

	for (i = 0; test_table[i].args != -1; i++)
		if (strcmp(name, test_table[i].name) == 0)
			break;

	test = &test_table[i];

	if (test->args == -1) {
		SYNTAX_ERROR("Non-existent test \"%s\"\n", name);
		return NULL;
	}

	if(parsing_action->type == EXCLUDE_ACTION && !test->exclude_ok) {
		fprintf(stderr, "Failed to parse action \"%s\"\n", source);
		fprintf(stderr, "Test \"%s\" cannot be used in exclude "
							"actions\n", name);
		fprintf(stderr, "Use prune action instead ...\n");
		return NULL;
	}

	expr = malloc(sizeof(*expr));
	if (expr == NULL)
		MEM_ERROR();

	expr->type = ATOM_TYPE;

	expr->atom.test = test;
	expr->atom.data = NULL;

	/*
	 * If the test has no arguments, then go straight to checking if there's
	 * enough arguments
	 */
	token = peek_token(&string);

	if (token != TOK_OPEN_BRACKET)
			goto skip_args;

	get_token(&string);

	/*
	 * speculatively read all the arguments, and then see if the
	 * number of arguments read is the number expected, this handles
	 * tests with a variable number of arguments
	 */
	token = get_token(&string);
	if (token == TOK_CLOSE_BRACKET)
		goto skip_args;

	while(1) {
		if (token != TOK_STRING) {
			SYNTAX_ERROR("Unexpected token \"%s\", expected "
				"argument\n", TOK_TO_STR(token, string));
			goto failed;
		}

		argv = realloc(argv, (args + 1) * sizeof(char *));
		if (argv == NULL)
			MEM_ERROR();

		argv[args ++ ] = strdup(string);

		token = get_token(&string);

		if (token == TOK_CLOSE_BRACKET)
			break;

		if (token != TOK_COMMA) {
			SYNTAX_ERROR("Unexpected token \"%s\", expected "
				"\",\" or \")\"\n", TOK_TO_STR(token, string));
			goto failed;
		}
		token = get_token(&string);
	}

skip_args:
	/*
	 * expected number of arguments?
	 */
	if(test->args != -2 && args != test->args) {
		SYNTAX_ERROR("Unexpected number of arguments, expected %d, "
			"got %d\n", test->args, args);
		goto failed;
	}

	expr->atom.args = args;
	expr->atom.argv = argv;

	if (test->parse_args) {
		int res = test->parse_args(test, &expr->atom);

		if (res == 0)		
			goto failed;
	}

	return expr;

failed:
	free(argv);
	free(expr);
	return NULL;
}


static struct expr *get_atom()
{
	char *string;
	int token = get_token(&string);

	switch(token) {
	case TOK_NOT:
		return create_unary_op(get_atom(), token);
	case TOK_OPEN_BRACKET:
		return parse_expr(1);
	case TOK_STRING:
		return parse_test(string);
	default:
		SYNTAX_ERROR("Unexpected token \"%s\", expected test "
					"operation, \"!\", or \"(\"\n",
					TOK_TO_STR(token, string));
		return NULL;
	}
}


static struct expr *parse_expr(int subexp)
{
	struct expr *expr = get_atom();

	while (expr) {
		char *string;
		int op = get_token(&string);

		if (op == TOK_EOF) {
			if (subexp) {
				free_parse_tree(expr);
				SYNTAX_ERROR("Expected \"&&\", \"||\" or "
						"\")\", got EOF\n");
				return NULL;
			}
			break;
		}

		if (op == TOK_CLOSE_BRACKET) {
			if (!subexp) {
				free_parse_tree(expr);
				SYNTAX_ERROR("Unexpected \")\", expected "
						"\"&&\", \"!!\" or EOF\n");
				return NULL;
			}
			break;
		}
		
		if (op != TOK_AND && op != TOK_OR) {
			free_parse_tree(expr);
			SYNTAX_ERROR("Unexpected token \"%s\", expected "
				"\"&&\" or \"||\"\n", TOK_TO_STR(op, string));
			return NULL;
		}

		expr = create_expr(expr, op, get_atom());
	}

	return expr;
}


/*
 * Action parser
 */
int parse_action(char *s, int verbose)
{
	char *string, **argv = NULL;
	int i, token, args = 0;
	struct expr *expr;
	struct action_entry *action;
	void *data = NULL;
	struct action **spec_list;
	int spec_count;

	cur_ptr = source = s;
	token = get_token(&string);

	if (token != TOK_STRING) {
		SYNTAX_ERROR("Unexpected token \"%s\", expected name\n",
						TOK_TO_STR(token, string));
		return 0;
	}

	for (i = 0; action_table[i].args != -1; i++)
		if (strcmp(string, action_table[i].name) == 0)
			break;

	if (action_table[i].args == -1) {
		SYNTAX_ERROR("Non-existent action \"%s\"\n", string);
		return 0;
	}

	action = &action_table[i];

	token = get_token(&string);

	if (token == TOK_AT)
		goto skip_args;

	if (token != TOK_OPEN_BRACKET) {
		SYNTAX_ERROR("Unexpected token \"%s\", expected \"(\"\n",
						TOK_TO_STR(token, string));
		goto failed;
	}

	/*
	 * speculatively read all the arguments, and then see if the
	 * number of arguments read is the number expected, this handles
	 * actions with a variable number of arguments
	 */
	token = get_token(&string);
	if (token == TOK_CLOSE_BRACKET)
		goto skip_args;

	while (1) {
		if (token != TOK_STRING) {
			SYNTAX_ERROR("Unexpected token \"%s\", expected "
				"argument\n", TOK_TO_STR(token, string));
			goto failed;
		}

		argv = realloc(argv, (args + 1) * sizeof(char *));
		if (argv == NULL)
			MEM_ERROR();

		argv[args ++] = strdup(string);

		token = get_token(&string);

		if (token == TOK_CLOSE_BRACKET)
			break;

		if (token != TOK_COMMA) {
			SYNTAX_ERROR("Unexpected token \"%s\", expected "
				"\",\" or \")\"\n", TOK_TO_STR(token, string));
			goto failed;
		}
		token = get_token(&string);
	}

skip_args:
	/*
	 * expected number of arguments?
	 */
	if(action->args != -2 && args != action->args) {
		SYNTAX_ERROR("Unexpected number of arguments, expected %d, "
			"got %d\n", action->args, args);
		goto failed;
	}

	if (action->parse_args) {
		int res = action->parse_args(action, args, argv, &data);

		if (res == 0)
			goto failed;
	}

	if (token == TOK_CLOSE_BRACKET)
		token = get_token(&string);

	if (token != TOK_AT) {
		SYNTAX_ERROR("Unexpected token \"%s\", expected \"@\"\n",
						TOK_TO_STR(token, string));
		goto failed;
	}
	
	parsing_action = action;
	expr = parse_expr(0);

	if (expr == NULL)
		goto failed;

	/*
	 * choose action list and increment action counter
	 */
	switch(action->type) {
	case FRAGMENT_ACTION:
		spec_count = fragment_count ++;
		spec_list = &fragment_spec;
		break;
	case EXCLUDE_ACTION:
		spec_count = exclude_count ++;
		spec_list = &exclude_spec;
		break;
	case EMPTY_ACTION:
		spec_count = empty_count ++;
		spec_list = &empty_spec;
		break;
	case MOVE_ACTION:
		spec_count = move_count ++;
		spec_list = &move_spec;
		break;
	case PRUNE_ACTION:
		spec_count = prune_count ++;
		spec_list = &prune_spec;
		break;
	default:
		spec_count = other_count ++;
		spec_list = &other_spec;
	}
	
	*spec_list = realloc(*spec_list, (spec_count + 1) *
					sizeof(struct action));
	if (*spec_list == NULL)
		MEM_ERROR();

	(*spec_list)[spec_count].type = action->type;
	(*spec_list)[spec_count].action = action;
	(*spec_list)[spec_count].args = args;
	(*spec_list)[spec_count].argv = argv;
	(*spec_list)[spec_count].expr = expr;
	(*spec_list)[spec_count].data = data;
	(*spec_list)[spec_count].verbose = verbose;

	return 1;

failed:
	free(argv);
	return 0;
}


/*
 * Evaluate expressions
 */

#define ALLOC_SZ 128

#define LOG_ENABLE	0
#define LOG_DISABLE	1
#define LOG_PRINT	2
#define LOG_ENABLED	3

char *_expr_log(char *string, int cmnd)
{
	static char *expr_msg = NULL;
	static int cur_size = 0, alloc_size = 0;
	int size;

	switch(cmnd) {
	case LOG_ENABLE:
		expr_msg = malloc(ALLOC_SZ);
		alloc_size = ALLOC_SZ;
		cur_size = 0;
		return expr_msg;
	case LOG_DISABLE:
		free(expr_msg);
		alloc_size = cur_size = 0;
		return expr_msg = NULL;
	case LOG_ENABLED:
		return expr_msg;
	default:
		if(expr_msg == NULL)
			return NULL;
		break;
	}

	/* if string is empty append '\0' */
	size = strlen(string) ? : 1; 

	if(alloc_size - cur_size < size) {
		/* buffer too small, expand */
		alloc_size = (cur_size + size + ALLOC_SZ - 1) & ~(ALLOC_SZ - 1);

		expr_msg = realloc(expr_msg, alloc_size);
		if(expr_msg == NULL)
			MEM_ERROR();
	}

	memcpy(expr_msg + cur_size, string, size);
	cur_size += size; 

	return expr_msg;
}


char *expr_log_cmnd(int cmnd)
{
	return _expr_log(NULL, cmnd);
}


char *expr_log(char *string)
{
	return _expr_log(string, LOG_PRINT);
}


void expr_log_atom(struct atom *atom)
{
	int i;

	if(atom->test->handle_logging)
		return;

	expr_log(atom->test->name);

	if(atom->args) {
		expr_log("(");
		for(i = 0; i < atom->args; i++) {
			expr_log(atom->argv[i]);
			if (i + 1 < atom->args)
				expr_log(",");
		}
		expr_log(")");
	}
}


void expr_log_match(int match)
{
	if(match)
		expr_log("=True");
	else
		expr_log("=False");
}


static int eval_expr_log(struct expr *expr, struct action_data *action_data)
{
	int match;

	switch (expr->type) {
	case ATOM_TYPE:
		expr_log_atom(&expr->atom);
		match = expr->atom.test->fn(&expr->atom, action_data);
		expr_log_match(match);
		break;
	case UNARY_TYPE:
		expr_log("!");
		match = !eval_expr_log(expr->unary_op.expr, action_data);
		break;
	default:
		expr_log("(");
		match = eval_expr_log(expr->expr_op.lhs, action_data);

		if ((expr->expr_op.op == TOK_AND && match) ||
				(expr->expr_op.op == TOK_OR && !match)) {
			expr_log(token_table[expr->expr_op.op].string);
			match = eval_expr_log(expr->expr_op.rhs, action_data);
		}
		expr_log(")");
		break;
	}

	return match;
}


static int eval_expr(struct expr *expr, struct action_data *action_data)
{
	int match;

	switch (expr->type) {
	case ATOM_TYPE:
		match = expr->atom.test->fn(&expr->atom, action_data);
		break;
	case UNARY_TYPE:
		match = !eval_expr(expr->unary_op.expr, action_data);
		break;
	default:
		match = eval_expr(expr->expr_op.lhs, action_data);

		if ((expr->expr_op.op == TOK_AND && match) ||
					(expr->expr_op.op == TOK_OR && !match))
			match = eval_expr(expr->expr_op.rhs, action_data);
		break;
	}

	return match;
}


static int eval_expr_top(struct action *action, struct action_data *action_data)
{
	if(action->verbose) {
		int match, n;

		expr_log_cmnd(LOG_ENABLE);

		if(action_data->subpath)
			expr_log(action_data->subpath);

		expr_log("=");
		expr_log(action->action->name);

		if(action->args) {
			expr_log("(");
			for (n = 0; n < action->args; n++) {
				expr_log(action->argv[n]);
				if(n + 1 < action->args)
					expr_log(",");
			}
			expr_log(")");
		}

		expr_log("@");

		match = eval_expr_log(action->expr, action_data);

		/*
		 * Print the evaluated expression log, if the
		 * result matches the logging specified
		 */
		if((match && (action->verbose & ACTION_LOG_TRUE)) || (!match
				&& (action->verbose & ACTION_LOG_FALSE)))
			progressbar_info("%s\n", expr_log(""));

		expr_log_cmnd(LOG_DISABLE);

		return match;
	} else
		return eval_expr(action->expr, action_data);
}


/*
 * Read action file, passing each line to parse_action() for
 * parsing.
 *
 * One action per line, of the form
 *	action(arg1,arg2)@expr(arg1,arg2)....
 *
 * Actions can be split across multiple lines using "\".
 * 
 * Blank lines and comment lines indicated by # are supported.
 */
int parse_action_true(char *s)
{
	return parse_action(s, ACTION_LOG_TRUE);
}


int parse_action_false(char *s)
{
	return parse_action(s, ACTION_LOG_FALSE);
}


int parse_action_verbose(char *s)
{
	return parse_action(s, ACTION_LOG_VERBOSE);
}


int parse_action_nonverbose(char *s)
{
	return parse_action(s, ACTION_LOG_NONE);
}


int read_action_file(char *filename, int verbose)
{
	switch(verbose) {
	case ACTION_LOG_TRUE:
		return read_file(filename, "action", parse_action_true);
	case ACTION_LOG_FALSE:
		return read_file(filename, "action", parse_action_false);
	case ACTION_LOG_VERBOSE:
		return read_file(filename, "action", parse_action_verbose);
	default:
		return read_file(filename, "action", parse_action_nonverbose);
	}
}


/*
 * helper to evaluate whether action/test acts on this file type
 */
static int file_type_match(int st_mode, int type)
{
	switch(type) {
	case ACTION_DIR:
		return S_ISDIR(st_mode);
	case ACTION_REG:
		return S_ISREG(st_mode);
	case ACTION_ALL:
		return S_ISREG(st_mode) || S_ISDIR(st_mode) ||
			S_ISCHR(st_mode) || S_ISBLK(st_mode) ||
			S_ISFIFO(st_mode) || S_ISSOCK(st_mode);
	case ACTION_LNK:
		return S_ISLNK(st_mode);
	case ACTION_ALL_LNK:
	default:
		return 1;
	}
}


/*
 * General action evaluation code
 */
int actions()
{
	return other_count;
}


void eval_actions(struct dir_info *root, struct dir_ent *dir_ent)
{
	int i, match;
	struct action_data action_data;
	int st_mode = dir_ent->inode->buf.st_mode;

	action_data.name = dir_ent->name;
	action_data.pathname = strdup(pathname(dir_ent));
	action_data.subpath = strdup(subpathname(dir_ent));
	action_data.buf = &dir_ent->inode->buf;
	action_data.depth = dir_ent->our_dir->depth;
	action_data.dir_ent = dir_ent;
	action_data.root = root;

	for (i = 0; i < other_count; i++) {
		struct action *action = &other_spec[i];

		if (!file_type_match(st_mode, action->action->file_types))
			/* action does not operate on this file type */
			continue;

		match = eval_expr_top(action, &action_data);

		if (match)
			action->action->run_action(action, dir_ent);
	}

	free(action_data.pathname);
	free(action_data.subpath);
}


/*
 * Fragment specific action code
 */
void *eval_frag_actions(struct dir_info *root, struct dir_ent *dir_ent)
{
	int i, match;
	struct action_data action_data;

	action_data.name = dir_ent->name;
	action_data.pathname = strdup(pathname(dir_ent));
	action_data.subpath = strdup(subpathname(dir_ent));
	action_data.buf = &dir_ent->inode->buf;
	action_data.depth = dir_ent->our_dir->depth;
	action_data.dir_ent = dir_ent;
	action_data.root = root;

	for (i = 0; i < fragment_count; i++) {
		match = eval_expr_top(&fragment_spec[i], &action_data);
		if (match) {
			free(action_data.pathname);
			free(action_data.subpath);
			return &fragment_spec[i].data;
		}
	}

	free(action_data.pathname);
	free(action_data.subpath);
	return &def_fragment;
}


void *get_frag_action(void *fragment)
{
	struct action *spec_list_end = &fragment_spec[fragment_count];
	struct action *action;

	if (fragment == NULL)
		return &def_fragment;

	if (fragment_count == 0)
		return NULL;

	if (fragment == &def_fragment)
		action = &fragment_spec[0] - 1;
	else 
		action = fragment - offsetof(struct action, data);

	if (++action == spec_list_end)
		return NULL;

	return &action->data;
}


/*
 * Exclude specific action code
 */
int exclude_actions()
{
	return exclude_count;
}


int eval_exclude_actions(char *name, char *pathname, char *subpath,
	struct stat *buf, int depth, struct dir_ent *dir_ent)
{
	int i, match = 0;
	struct action_data action_data;

	action_data.name = name;
	action_data.pathname = pathname;
	action_data.subpath = subpath;
	action_data.buf = buf;
	action_data.depth = depth;
	action_data.dir_ent = dir_ent;

	for (i = 0; i < exclude_count && !match; i++)
		match = eval_expr_top(&exclude_spec[i], &action_data);

	return match;
}


/*
 * Fragment specific action code
 */
static void frag_action(struct action *action, struct dir_ent *dir_ent)
{
	struct inode_info *inode = dir_ent->inode;

	inode->no_fragments = 0;
}

static void no_frag_action(struct action *action, struct dir_ent *dir_ent)
{
	struct inode_info *inode = dir_ent->inode;

	inode->no_fragments = 1;
}

static void always_frag_action(struct action *action, struct dir_ent *dir_ent)
{
	struct inode_info *inode = dir_ent->inode;

	inode->always_use_fragments = 1;
}

static void no_always_frag_action(struct action *action, struct dir_ent *dir_ent)
{
	struct inode_info *inode = dir_ent->inode;

	inode->always_use_fragments = 0;
}


/*
 * Compression specific action code
 */
static void comp_action(struct action *action, struct dir_ent *dir_ent)
{
	struct inode_info *inode = dir_ent->inode;

	inode->noD = inode->noF = 0;
}

static void uncomp_action(struct action *action, struct dir_ent *dir_ent)
{
	struct inode_info *inode = dir_ent->inode;

	inode->noD = inode->noF = 1;
}


/*
 * Uid/gid specific action code
 */
static long long parse_uid(char *arg) {
	char *b;
	long long uid = strtoll(arg, &b, 10);

	if (*b == '\0') {
		if (uid < 0 || uid >= (1LL << 32)) {
			SYNTAX_ERROR("Uid out of range\n");
			return -1;
		}
	} else {
		struct passwd *passwd = getpwnam(arg);

		if (passwd)
			uid = passwd->pw_uid;
		else {
			SYNTAX_ERROR("Invalid uid or unknown user\n");
			return -1;
		}
	}

	return uid;
}


static long long parse_gid(char *arg) {
	char *b;
	long long gid = strtoll(arg, &b, 10);

	if (*b == '\0') {
		if (gid < 0 || gid >= (1LL << 32)) {
			SYNTAX_ERROR("Gid out of range\n");
			return -1;
		}
	} else {
		struct group *group = getgrnam(arg);

		if (group)
			gid = group->gr_gid;
		else {
			SYNTAX_ERROR("Invalid gid or unknown group\n");
			return -1;
		}
	}

	return gid;
}


static int parse_uid_args(struct action_entry *action, int args, char **argv,
								void **data)
{
	long long uid;
	struct uid_info *uid_info;

	uid = parse_uid(argv[0]);
	if (uid == -1)
		return 0;

	uid_info = malloc(sizeof(struct uid_info));
	if (uid_info == NULL)
		MEM_ERROR();

	uid_info->uid = uid;
	*data = uid_info;

	return 1;
}


static int parse_gid_args(struct action_entry *action, int args, char **argv,
								void **data)
{
	long long gid;
	struct gid_info *gid_info;

	gid = parse_gid(argv[0]);
	if (gid == -1)
		return 0;

	gid_info = malloc(sizeof(struct gid_info));
	if (gid_info == NULL)
		MEM_ERROR();

	gid_info->gid = gid;
	*data = gid_info;

	return 1;
}


static int parse_guid_args(struct action_entry *action, int args, char **argv,
								void **data)
{
	long long uid, gid;
	struct guid_info *guid_info;

	uid = parse_uid(argv[0]);
	if (uid == -1)
		return 0;

	gid = parse_gid(argv[1]);
	if (gid == -1)
		return 0;

	guid_info = malloc(sizeof(struct guid_info));
	if (guid_info == NULL)
		MEM_ERROR();

	guid_info->uid = uid;
	guid_info->gid = gid;
	*data = guid_info;

	return 1;
}


static void uid_action(struct action *action, struct dir_ent *dir_ent)
{
	struct inode_info *inode = dir_ent->inode;
	struct uid_info *uid_info = action->data;

	inode->buf.st_uid = uid_info->uid;
}

static void gid_action(struct action *action, struct dir_ent *dir_ent)
{
	struct inode_info *inode = dir_ent->inode;
	struct gid_info *gid_info = action->data;

	inode->buf.st_gid = gid_info->gid;
}

static void guid_action(struct action *action, struct dir_ent *dir_ent)
{
	struct inode_info *inode = dir_ent->inode;
	struct guid_info *guid_info = action->data;

	inode->buf.st_uid = guid_info->uid;
	inode->buf.st_gid = guid_info->gid;

}


/*
 * Mode specific action code
 */
static int parse_octal_mode_args(int args, char **argv,
			void **data)
{
	int n, bytes;
	unsigned int mode;
	struct mode_data *mode_data;

	/* octal mode number? */
	n = sscanf(argv[0], "%o%n", &mode, &bytes);
	if (n == 0)
		return -1; /* not an octal number arg */


	/* check there's no trailing junk */
	if (argv[0][bytes] != '\0') {
		SYNTAX_ERROR("Unexpected trailing bytes after octal "
			"mode number\n");
		return 0; /* bad octal number arg */
	}

	/* check there's only one argument */
	if (args > 1) {
		SYNTAX_ERROR("Octal mode number is first argument, "
			"expected one argument, got %d\n", args);
		return 0; /* bad octal number arg */
	}

	/*  check mode is within range */
	if (mode > 07777) {
		SYNTAX_ERROR("Octal mode %o is out of range\n", mode);
		return 0; /* bad octal number arg */
	}

	mode_data = malloc(sizeof(struct mode_data));
	if (mode_data == NULL)
		MEM_ERROR();

	mode_data->operation = ACTION_MODE_OCT;
	mode_data->mode = mode;
	mode_data->next = NULL;
	*data = mode_data;

	return 1;
}


/*
 * Parse symbolic mode of format [ugoa]*[[+-=]PERMS]+
 * PERMS = [rwxXst]+ or [ugo]
 */
static int parse_sym_mode_arg(char *arg, struct mode_data **head,
	struct mode_data **cur)
{
	struct mode_data *mode_data;
	int mode;
	int mask = 0;
	int op;
	char X;

	if (arg[0] != 'u' && arg[0] != 'g' && arg[0] != 'o' && arg[0] != 'a') {
		/* no ownership specifiers, default to a */
		mask = 0777;
		goto parse_operation;
	}

	/* parse ownership specifiers */
	while(1) {
		switch(*arg) {
		case 'u':
			mask |= 04700;
			break;
		case 'g':
			mask |= 02070;
			break;
		case 'o':
			mask |= 01007;
			break;
		case 'a':
			mask = 07777;
			break;
		default:
			goto parse_operation;
		}
		arg ++;
	}

parse_operation:
	/* trap a symbolic mode with just an ownership specification */
	if(*arg == '\0') {
		SYNTAX_ERROR("Expected one of '+', '-' or '=', got EOF\n");
		goto failed;
	}

	while(*arg != '\0') {
		mode = 0;
		X = 0;

		switch(*arg) {
		case '+':
			op = ACTION_MODE_ADD;
			break;
		case '-':
			op = ACTION_MODE_REM;
			break;
		case '=':
			op = ACTION_MODE_SET;
			break;
		default:
			SYNTAX_ERROR("Expected one of '+', '-' or '=', got "
				"'%c'\n", *arg);
			goto failed;
		}
	
		arg ++;
	
		/* Parse PERMS */
		if (*arg == 'u' || *arg == 'g' || *arg == 'o') {
	 		/* PERMS = [ugo] */
			mode = - *arg;
			arg ++;
		} else {
	 		/* PERMS = [rwxXst]* */
			while(1) {
				switch(*arg) {
				case 'r':
					mode |= 0444;
					break;
				case 'w':
					mode |= 0222;
					break;
				case 'x':
					mode |= 0111;
					break;
				case 's':
					mode |= 06000;
					break;
				case 't':
					mode |= 01000;
					break;
				case 'X':
					X = 1;
					break;
				case '+':
				case '-':
				case '=':
				case '\0':
					mode &= mask;
					goto perms_parsed;
				default:
					SYNTAX_ERROR("Unrecognised permission "
								"'%c'\n", *arg);
					goto failed;
				}
	
				arg ++;
			}
		}
	
perms_parsed:
		mode_data = malloc(sizeof(*mode_data));
		if (mode_data == NULL)
			MEM_ERROR();

		mode_data->operation = op;
		mode_data->mode = mode;
		mode_data->mask = mask;
		mode_data->X = X;
		mode_data->next = NULL;

		if (*cur) {
			(*cur)->next = mode_data;
			*cur = mode_data;
		} else
			*head = *cur = mode_data;
	}

	return 1;

failed:
	return 0;
}


static int parse_sym_mode_args(struct action_entry *action, int args,
					char **argv, void **data)
{
	int i, res = 1;
	struct mode_data *head = NULL, *cur = NULL;

	for (i = 0; i < args && res; i++)
		res = parse_sym_mode_arg(argv[i], &head, &cur);

	*data = head;

	return res;
}


static int parse_mode_args(struct action_entry *action, int args,
					char **argv, void **data)
{
	int res;

	if (args == 0) {
		SYNTAX_ERROR("Mode action expects one or more arguments\n");
		return 0;
	}

	res = parse_octal_mode_args(args, argv, data);
	if(res >= 0)
		/* Got an octal mode argument */
		return res;
	else  /* not an octal mode argument */
		return parse_sym_mode_args(action, args, argv, data);
}


static int mode_execute(struct mode_data *mode_data, int st_mode)
{
	int mode = 0;

	for (;mode_data; mode_data = mode_data->next) {
		if (mode_data->mode < 0) {
			/* 'u', 'g' or 'o' */
			switch(-mode_data->mode) {
			case 'u':
				mode = (st_mode >> 6) & 07;
				break;
			case 'g':
				mode = (st_mode >> 3) & 07;
				break;
			case 'o':
				mode = st_mode & 07;
				break;
			}
			mode = ((mode << 6) | (mode << 3) | mode) &
				mode_data->mask;
		} else if (mode_data->X &&
				((st_mode & S_IFMT) == S_IFDIR ||
				(st_mode & 0111)))
			/* X permission, only takes effect if inode is a
			 * directory or x is set for some owner */
			mode = mode_data->mode | (0111 & mode_data->mask);
		else
			mode = mode_data->mode;

		switch(mode_data->operation) {
		case ACTION_MODE_OCT:
			st_mode = (st_mode & S_IFMT) | mode;
			break;
		case ACTION_MODE_SET:
			st_mode = (st_mode & ~mode_data->mask) | mode;
			break;
		case ACTION_MODE_ADD:
			st_mode |= mode;
			break;
		case ACTION_MODE_REM:
			st_mode &= ~mode;
		}
	}

	return st_mode;
}


static void mode_action(struct action *action, struct dir_ent *dir_ent)
{
	dir_ent->inode->buf.st_mode = mode_execute(action->data,
					dir_ent->inode->buf.st_mode);
}


/*
 *  Empty specific action code
 */
int empty_actions()
{
	return empty_count;
}


static int parse_empty_args(struct action_entry *action, int args,
					char **argv, void **data)
{
	struct empty_data *empty_data;
	int val;

	if (args >= 2) {
		SYNTAX_ERROR("Empty action expects zero or one argument\n");
		return 0;
	}

	if (args == 0 || strcmp(argv[0], "all") == 0)
		val = EMPTY_ALL;
	else if (strcmp(argv[0], "source") == 0)
		val = EMPTY_SOURCE;
	else if (strcmp(argv[0], "excluded") == 0)
		val = EMPTY_EXCLUDED;
	else {
		SYNTAX_ERROR("Empty action expects zero arguments, or one"
			"argument containing \"all\", \"source\", or \"excluded\""
			"\n");
		return 0;
	}

	empty_data = malloc(sizeof(*empty_data));
	if (empty_data == NULL)
		MEM_ERROR();

	empty_data->val = val;
	*data = empty_data;

	return 1;
}


int eval_empty_actions(struct dir_info *root, struct dir_ent *dir_ent)
{
	int i, match = 0;
	struct action_data action_data;
	struct empty_data *data;
	struct dir_info *dir = dir_ent->dir;

	/*
	 * Empty action only works on empty directories
	 */
	if (dir->count != 0)
		return 0;

	action_data.name = dir_ent->name;
	action_data.pathname = strdup(pathname(dir_ent));
	action_data.subpath = strdup(subpathname(dir_ent));
	action_data.buf = &dir_ent->inode->buf;
	action_data.depth = dir_ent->our_dir->depth;
	action_data.dir_ent = dir_ent;
	action_data.root = root;

	for (i = 0; i < empty_count && !match; i++) {
		data = empty_spec[i].data;

		/*
		 * determine the cause of the empty directory and evaluate
		 * the empty action specified.  Three empty actions:
		 * - EMPTY_SOURCE: empty action triggers only if the directory
		 *	was originally empty, i.e directories that are empty
		 *	only due to excluding are ignored.
		 * - EMPTY_EXCLUDED: empty action triggers only if the directory
		 *	is empty because of excluding, i.e. directories that
		 *	were originally empty are ignored.
		 * - EMPTY_ALL (the default): empty action triggers if the
		 *	directory is empty, irrespective of the reason, i.e.
		 *	the directory could have been originally empty or could
		 *	be empty due to excluding.
		 */
		if ((data->val == EMPTY_EXCLUDED && !dir->excluded) ||
				(data->val == EMPTY_SOURCE && dir->excluded))
			continue;
		
		match = eval_expr_top(&empty_spec[i], &action_data);
	}

	free(action_data.pathname);
	free(action_data.subpath);

	return match;
}


/*
 *  Move specific action code
 */
static struct move_ent *move_list = NULL;


int move_actions()
{
	return move_count;
}


static char *move_pathname(struct move_ent *move)
{
	struct dir_info *dest;
	char *name, *pathname;
	int res;

	dest = (move->ops & ACTION_MOVE_MOVE) ?
		move->dest : move->dir_ent->our_dir;
	name = (move->ops & ACTION_MOVE_RENAME) ?
		move->name : move->dir_ent->name;

	if(dest->subpath[0] != '\0')
		res = asprintf(&pathname, "%s/%s", dest->subpath, name);
	else
		res = asprintf(&pathname, "/%s", name);

	if(res == -1)
		BAD_ERROR("asprintf failed in move_pathname\n");

	return pathname;
}


static char *get_comp(char **pathname)
{
	char *path = *pathname, *start;

	while(*path == '/')
		path ++;

	if(*path == '\0')
		return NULL;

	start = path;
	while(*path != '/' && *path != '\0')
		path ++;

	*pathname = path;
	return strndup(start, path - start);
}


static struct dir_ent *lookup_comp(char *comp, struct dir_info *dest)
{
	struct dir_ent *dir_ent;

	for(dir_ent = dest->list; dir_ent; dir_ent = dir_ent->next)
		if(strcmp(comp, dir_ent->name) == 0)
			break;

	return dir_ent;
}


void eval_move(struct action_data *action_data, struct move_ent *move,
		struct dir_info *root, struct dir_ent *dir_ent, char *pathname)
{
	struct dir_info *dest, *source = dir_ent->our_dir;
	struct dir_ent *comp_ent;
	char *comp, *path = pathname;

	/*
	 * Walk pathname to get the destination directory
	 *
	 * Like the mv command, if the last component exists and it
	 * is a directory, then move the file into that directory,
	 * otherwise, move the file into parent directory of the last
	 * component and rename to the last component.
	 */
	if (pathname[0] == '/')
		/* absolute pathname, walk from root directory */
		dest = root;
	else
		/* relative pathname, walk from current directory */
		dest = source;

	for(comp = get_comp(&pathname); comp; free(comp),
						comp = get_comp(&pathname)) {

		if (strcmp(comp, ".") == 0)
			continue;

		if (strcmp(comp, "..") == 0) {
			/* if we're in the root directory then ignore */
			if(dest->depth > 1)
				dest = dest->dir_ent->our_dir;
			continue;
		}

		/*
		 * Look up comp in current directory, if it exists and it is a
		 * directory continue walking the pathname, otherwise exit,
		 * we've walked as far as we can go, normally this is because
		 * we've arrived at the leaf component which we are going to
		 * rename source to
		 */
		comp_ent = lookup_comp(comp, dest);
		if (comp_ent == NULL || (comp_ent->inode->buf.st_mode & S_IFMT)
							!= S_IFDIR)
			break;

		dest = comp_ent->dir;
	}

	if(comp) {
		/* Leaf component? If so we're renaming to this  */
		char *remainder = get_comp(&pathname);
		free(remainder);

		if(remainder) {
			/*
			 * trying to move source to a subdirectory of
			 * comp, but comp either doesn't exist, or it isn't
			 * a directory, which is impossible
			 */
			if (comp_ent == NULL)
				ERROR("Move action: cannot move %s to %s, no "
					"such directory %s\n",
					action_data->subpath, path, comp);
			else
				ERROR("Move action: cannot move %s to %s, %s "
					"is not a directory\n",
					action_data->subpath, path, comp);
			free(comp);
			return;
		}

		/*
		 * Multiple move actions triggering on one file can be merged
		 * if one is a RENAME and the other is a MOVE.  Multiple RENAMEs
		 * can only merge if they're doing the same thing
	 	 */
		if(move->ops & ACTION_MOVE_RENAME) {
			if(strcmp(comp, move->name) != 0) {
				char *conf_path = move_pathname(move);
				ERROR("Move action: Cannot move %s to %s, "
					"conflicting move, already moving "
					"to %s via another move action!\n",
					action_data->subpath, path, conf_path);
				free(conf_path);
				free(comp);
				return;
			}
			free(comp);
		} else {
			move->name = comp;
			move->ops |= ACTION_MOVE_RENAME;
		}
	}

	if(dest != source) {
		/*
		 * Multiple move actions triggering on one file can be merged
		 * if one is a RENAME and the other is a MOVE.  Multiple MOVEs
		 * can only merge if they're doing the same thing
	 	 */
		if(move->ops & ACTION_MOVE_MOVE) {
			if(dest != move->dest) {
				char *conf_path = move_pathname(move);
				ERROR("Move action: Cannot move %s to %s, "
					"conflicting move, already moving "
					"to %s via another move action!\n",
					action_data->subpath, path, conf_path);
				free(conf_path);
				return;
			}
		} else {
			move->dest = dest;
			move->ops |= ACTION_MOVE_MOVE;
		}
	}
}


static int subdirectory(struct dir_info *source, struct dir_info *dest)
{
	if(source == NULL)
		return 0;

	return strlen(source->subpath) <= strlen(dest->subpath) &&
		(dest->subpath[strlen(source->subpath)] == '/' ||
		dest->subpath[strlen(source->subpath)] == '\0') &&
		strncmp(source->subpath, dest->subpath,
		strlen(source->subpath)) == 0;
}


void eval_move_actions(struct dir_info *root, struct dir_ent *dir_ent)
{
	int i;
	struct action_data action_data;
	struct move_ent *move = NULL;

	action_data.name = dir_ent->name;
	action_data.pathname = strdup(pathname(dir_ent));
	action_data.subpath = strdup(subpathname(dir_ent));
	action_data.buf = &dir_ent->inode->buf;
	action_data.depth = dir_ent->our_dir->depth;
	action_data.dir_ent = dir_ent;
	action_data.root = root;

	/*
	 * Evaluate each move action against the current file.  For any
	 * move actions that match don't actually perform the move now, but,
	 * store it, and execute all the stored move actions together once the
	 * directory scan is complete.  This is done to ensure each separate
	 * move action does not nondeterministically interfere with other move
	 * actions.  Each move action is considered to act independently, and
	 * each move action sees the directory tree in the same state.
	 */
	for (i = 0; i < move_count; i++) {
		struct action *action = &move_spec[i];
		int match = eval_expr_top(action, &action_data);

		if(match) {
			if(move == NULL) {
				move = malloc(sizeof(*move));
				if(move == NULL)
					MEM_ERROR();

				move->ops = 0;
				move->dir_ent = dir_ent;
			}
			eval_move(&action_data, move, root, dir_ent,
				action->argv[0]);
		}
	}

	if(move) {
		struct dir_ent *comp_ent;
		struct dir_info *dest;
		char *name;

		/*
		 * Move contains the result of all triggered move actions.
		 * Check the destination doesn't already exist
		 */
		if(move->ops == 0) {
			free(move);
			goto finish;
		}

		dest = (move->ops & ACTION_MOVE_MOVE) ?
			move->dest : dir_ent->our_dir;
		name = (move->ops & ACTION_MOVE_RENAME) ?
			move->name : dir_ent->name;
		comp_ent = lookup_comp(name, dest);
		if(comp_ent) {
			char *conf_path = move_pathname(move);
			ERROR("Move action: Cannot move %s to %s, "
				"destination already exists\n",
				action_data.subpath, conf_path);
			free(conf_path);
			free(move);
			goto finish;
		}

		/*
		 * If we're moving a directory, check we're not moving it to a
		 * subdirectory of itself
		 */
		if(subdirectory(dir_ent->dir, dest)) {
			char *conf_path = move_pathname(move);
			ERROR("Move action: Cannot move %s to %s, this is a "
				"subdirectory of itself\n",
				action_data.subpath, conf_path);
			free(conf_path);
			free(move);
			goto finish;
		}
		move->next = move_list;
		move_list = move;
	}

finish:
	free(action_data.pathname);
	free(action_data.subpath);
}


static void move_dir(struct dir_ent *dir_ent)
{
	struct dir_info *dir = dir_ent->dir;
	struct dir_ent *comp_ent;

	/* update our directory's subpath name */
	free(dir->subpath);
	dir->subpath = strdup(subpathname(dir_ent));

	/* recursively update the subpaths of any sub-directories */
	for(comp_ent = dir->list; comp_ent; comp_ent = comp_ent->next)
		if(comp_ent->dir)
			move_dir(comp_ent);
}


static void move_file(struct move_ent *move_ent)
{
	struct dir_ent *dir_ent = move_ent->dir_ent;

	if(move_ent->ops & ACTION_MOVE_MOVE) {
		struct dir_ent *comp_ent, *prev = NULL;
		struct dir_info *source = dir_ent->our_dir,
							*dest = move_ent->dest;
		char *filename = pathname(dir_ent);

		/*
		 * If we're moving a directory, check we're not moving it to a
		 * subdirectory of itself
		 */
		if(subdirectory(dir_ent->dir, dest)) {
			char *conf_path = move_pathname(move_ent);
			ERROR("Move action: Cannot move %s to %s, this is a "
				"subdirectory of itself\n",
				subpathname(dir_ent), conf_path);
			free(conf_path);
			return;
		}

		/* Remove the file from source directory */
		for(comp_ent = source->list; comp_ent != dir_ent;
				prev = comp_ent, comp_ent = comp_ent->next);

		if(prev)
			prev->next = comp_ent->next;
		else
			source->list = comp_ent->next;

		source->count --;
		if((comp_ent->inode->buf.st_mode & S_IFMT) == S_IFDIR)
			source->directory_count --;

		/* Add the file to dest directory */
		comp_ent->next = dest->list;
		dest->list = comp_ent;
		comp_ent->our_dir = dest;

		dest->count ++;
		if((comp_ent->inode->buf.st_mode & S_IFMT) == S_IFDIR)
			dest->directory_count ++;

		/*
		 * We've moved the file, and so we can't now use the
		 * parent directory's pathname to calculate the pathname
		 */
		if(dir_ent->nonstandard_pathname == NULL) {
			dir_ent->nonstandard_pathname = strdup(filename);
			if(dir_ent->source_name) {
				free(dir_ent->source_name);
				dir_ent->source_name = NULL;
			}
		}
	}

	if(move_ent->ops & ACTION_MOVE_RENAME) {
		/*
		 * If we're using name in conjunction with the parent
		 * directory's pathname to calculate the pathname, we need
		 * to use source_name to override.  Otherwise it's already being
		 * over-ridden
		 */
		if(dir_ent->nonstandard_pathname == NULL &&
						dir_ent->source_name == NULL)
			dir_ent->source_name = dir_ent->name;
		else
			free(dir_ent->name);

		dir_ent->name = move_ent->name;
	}

	if(dir_ent->dir)
		/*
		 * dir_ent is a directory, and we have to recursively fix-up
		 * its subpath, and the subpaths of all of its sub-directories
		 */
		move_dir(dir_ent);
}


void do_move_actions()
{
	while(move_list) {
		struct move_ent *temp = move_list;
		struct dir_info *dest = (move_list->ops & ACTION_MOVE_MOVE) ?
			move_list->dest : move_list->dir_ent->our_dir;
		char *name = (move_list->ops & ACTION_MOVE_RENAME) ?
			move_list->name : move_list->dir_ent->name;
		struct dir_ent *comp_ent = lookup_comp(name, dest);
		if(comp_ent) {
			char *conf_path = move_pathname(move_list);
			ERROR("Move action: Cannot move %s to %s, "
				"destination already exists\n",
				subpathname(move_list->dir_ent), conf_path);
			free(conf_path);
		} else
			move_file(move_list);

		move_list = move_list->next;
		free(temp);
	}
}


/*
 * Prune specific action code
 */
int prune_actions()
{
	return prune_count;
}


int eval_prune_actions(struct dir_info *root, struct dir_ent *dir_ent)
{
	int i, match = 0;
	struct action_data action_data;

	action_data.name = dir_ent->name;
	action_data.pathname = strdup(pathname(dir_ent));
	action_data.subpath = strdup(subpathname(dir_ent));
	action_data.buf = &dir_ent->inode->buf;
	action_data.depth = dir_ent->our_dir->depth;
	action_data.dir_ent = dir_ent;
	action_data.root = root;

	for (i = 0; i < prune_count && !match; i++)
		match = eval_expr_top(&prune_spec[i], &action_data);

	free(action_data.pathname);
	free(action_data.subpath);

	return match;
}


/*
 * Noop specific action code
 */
static void noop_action(struct action *action, struct dir_ent *dir_ent)
{
}


/*
 * General test evaluation code
 */

/*
 * A number can be of the form [range]number[size]
 * [range] is either:
 *	'<' or '-', match on less than number
 *	'>' or '+', match on greater than number
 *	'' (nothing), match on exactly number
 * [size] is either:
 *	'' (nothing), number
 *	'k' or 'K', number * 2^10
 * 	'm' or 'M', number * 2^20
 *	'g' or 'G', number * 2^30
 */
static int parse_number(char *start, long long *size, int *range, char **error)
{
	char *end;
	long long number;

	if (*start == '>' || *start == '+') {
		*range = NUM_GREATER;
		start ++;
	} else if (*start == '<' || *start == '-') {
		*range = NUM_LESS;
		start ++;
	} else
		*range = NUM_EQ;

	errno = 0; /* To enable failure after call to be determined */
	number = strtoll(start, &end, 10);

	if((errno == ERANGE && (number == LLONG_MAX || number == LLONG_MIN))
				|| (errno != 0 && number == 0)) {
		/* long long underflow or overflow in conversion, or other
		 * conversion error.
		 * Note: we don't check for LLONG_MIN and LLONG_MAX only
		 * because strtoll can validly return that if the
		 * user used these values
		 */
		*error = "Long long underflow, overflow or other conversion "
								"error";
		return 0;
	}

	if (end == start) {
		/* Couldn't read any number  */
		*error = "Number expected";
		return 0;
	}

	switch (end[0]) {
	case 'g':
	case 'G':
		number *= 1024;
	case 'm':
	case 'M':
		number *= 1024;
	case 'k':
	case 'K':
		number *= 1024;

		if (end[1] != '\0') {
			*error = "Trailing junk after size specifier";
			return 0;
		}

		break;
	case '\0':
		break;
	default:
		*error = "Trailing junk after number";
		return 0;
	}

	*size = number;

	return 1;
}


static int parse_number_arg(struct test_entry *test, struct atom *atom)
{
	struct test_number_arg *number;
	long long size;
	int range;
	char *error;
	int res = parse_number(atom->argv[0], &size, &range, &error);

	if (res == 0) {
		TEST_SYNTAX_ERROR(test, 0, "%s\n", error);
		return 0;
	}

	number = malloc(sizeof(*number));
	if (number == NULL)
		MEM_ERROR();

	number->range = range;
	number->size = size;

	atom->data = number;

	return 1;
}


static int parse_range_args(struct test_entry *test, struct atom *atom)
{
	struct test_range_args *range;
	long long start, end;
	int type;
	int res;
	char *error;

	res = parse_number(atom->argv[0], &start, &type, &error);
	if (res == 0) {
		TEST_SYNTAX_ERROR(test, 0, "%s\n", error);
		return 0;
	}

	if (type != NUM_EQ) {
		TEST_SYNTAX_ERROR(test, 0, "Range specifier (<, >, -, +) not "
			"expected\n");
		return 0;
	}
 
	res = parse_number(atom->argv[1], &end, &type, &error);
	if (res == 0) {
		TEST_SYNTAX_ERROR(test, 1, "%s\n", error);
		return 0;
	}

	if (type != NUM_EQ) {
		TEST_SYNTAX_ERROR(test, 1, "Range specifier (<, >, -, +) not "
			"expected\n");
		return 0;
	}
 
	range = malloc(sizeof(*range));
	if (range == NULL)
		MEM_ERROR();

	range->start = start;
	range->end = end;

	atom->data = range;

	return 1;
}


/*
 * Generic test code macro
 */
#define TEST_FN(NAME, MATCH, CODE) \
static int NAME##_fn(struct atom *atom, struct action_data *action_data) \
{ \
	/* test operates on MATCH file types only */ \
	if (!file_type_match(action_data->buf->st_mode, MATCH)) \
		return 0; \
 \
	CODE \
}

/*
 * Generic test code macro testing VAR for size (eq, less than, greater than)
 */
#define TEST_VAR_FN(NAME, MATCH, VAR) TEST_FN(NAME, MATCH, \
	{ \
	int match = 0; \
	struct test_number_arg *number = atom->data; \
	\
	switch (number->range) { \
	case NUM_EQ: \
		match = VAR == number->size; \
		break; \
	case NUM_LESS: \
		match = VAR < number->size; \
		break; \
	case NUM_GREATER: \
		match = VAR > number->size; \
		break; \
	} \
	\
	return match; \
	})	


/*
 * Generic test code macro testing VAR for range [x, y] (value between x and y
 * inclusive).
 */	
#define TEST_VAR_RANGE_FN(NAME, MATCH, VAR) TEST_FN(NAME##_range, MATCH, \
	{ \
	struct test_range_args *range = atom->data; \
	\
	return range->start <= VAR && VAR <= range->end; \
	})	


/*
 * Name, Pathname and Subpathname test specific code
 */

/*
 * Add a leading "/" if subpathname and pathname lacks it
 */
static int check_pathname(struct test_entry *test, struct atom *atom)
{
	int res;
	char *name;

	if(atom->argv[0][0] != '/') {
		res = asprintf(&name, "/%s", atom->argv[0]);
		if(res == -1)
			BAD_ERROR("asprintf failed in check_pathname\n");

		free(atom->argv[0]);
		atom->argv[0] = name;
	}

	return 1;
}


TEST_FN(name, ACTION_ALL_LNK, \
	return fnmatch(atom->argv[0], action_data->name,
				FNM_PATHNAME|FNM_PERIOD|FNM_EXTMATCH) == 0;)

TEST_FN(pathname, ACTION_ALL_LNK, \
	return fnmatch(atom->argv[0], action_data->subpath,
				FNM_PATHNAME|FNM_PERIOD|FNM_EXTMATCH) == 0;)


static int count_components(char *path)
{
	int count;

	for (count = 0; *path != '\0'; count ++) {
		while (*path == '/')
			path ++;

		while (*path != '\0' && *path != '/')
			path ++;
	}

	return count;
}


static char *get_start(char *s, int n)
{
	int count;
	char *path = s;

	for (count = 0; *path != '\0' && count < n; count ++) {
		while (*path == '/')
			path ++;

		while (*path != '\0' && *path != '/')
			path ++;
	}

	if (count == n)
		*path = '\0';

	return s;
}
	

static int subpathname_fn(struct atom *atom, struct action_data *action_data)
{
	return fnmatch(atom->argv[0], get_start(strdupa(action_data->subpath),
		count_components(atom->argv[0])),
		FNM_PATHNAME|FNM_PERIOD|FNM_EXTMATCH) == 0;
}

/*
 * Inode attribute test operations using generic
 * TEST_VAR_FN(test name, file scope, attribute name) macro.
 * This is for tests that do not need to be specially handled in any way.
 * They just take a variable and compare it against a number.
 */
TEST_VAR_FN(filesize, ACTION_REG, action_data->buf->st_size)

TEST_VAR_FN(dirsize, ACTION_DIR, action_data->buf->st_size)

TEST_VAR_FN(size, ACTION_ALL_LNK, action_data->buf->st_size)

TEST_VAR_FN(inode, ACTION_ALL_LNK, action_data->buf->st_ino)

TEST_VAR_FN(nlink, ACTION_ALL_LNK, action_data->buf->st_nlink)

TEST_VAR_FN(fileblocks, ACTION_REG, action_data->buf->st_blocks)

TEST_VAR_FN(dirblocks, ACTION_DIR, action_data->buf->st_blocks)

TEST_VAR_FN(blocks, ACTION_ALL_LNK, action_data->buf->st_blocks)

TEST_VAR_FN(dircount, ACTION_DIR, action_data->dir_ent->dir->count)

TEST_VAR_FN(depth, ACTION_ALL_LNK, action_data->depth)

TEST_VAR_RANGE_FN(filesize, ACTION_REG, action_data->buf->st_size)

TEST_VAR_RANGE_FN(dirsize, ACTION_DIR, action_data->buf->st_size)

TEST_VAR_RANGE_FN(size, ACTION_ALL_LNK, action_data->buf->st_size)

TEST_VAR_RANGE_FN(inode, ACTION_ALL_LNK, action_data->buf->st_ino)

TEST_VAR_RANGE_FN(nlink, ACTION_ALL_LNK, action_data->buf->st_nlink)

TEST_VAR_RANGE_FN(fileblocks, ACTION_REG, action_data->buf->st_blocks)

TEST_VAR_RANGE_FN(dirblocks, ACTION_DIR, action_data->buf->st_blocks)

TEST_VAR_RANGE_FN(blocks, ACTION_ALL_LNK, action_data->buf->st_blocks)

TEST_VAR_RANGE_FN(gid, ACTION_ALL_LNK, action_data->buf->st_gid)

TEST_VAR_RANGE_FN(uid, ACTION_ALL_LNK, action_data->buf->st_uid)

TEST_VAR_RANGE_FN(depth, ACTION_ALL_LNK, action_data->depth)

TEST_VAR_RANGE_FN(dircount, ACTION_DIR, action_data->dir_ent->dir->count)

/*
 * uid specific test code
 */
TEST_VAR_FN(uid, ACTION_ALL_LNK, action_data->buf->st_uid)

static int parse_uid_arg(struct test_entry *test, struct atom *atom)
{
	struct test_number_arg *number;
	long long size;
	int range;
	char *error;

	if(parse_number(atom->argv[0], &size, &range, &error)) {
		/* managed to fully parse argument as a number */
		if(size < 0 || size > (((long long) 1 << 32) - 1)) {
			TEST_SYNTAX_ERROR(test, 1, "Numeric uid out of "
								"range\n");
			return 0;
		}
	} else {
		/* couldn't parse (fully) as a number, is it a user name? */
		struct passwd *uid = getpwnam(atom->argv[0]);
		if(uid) {
			size = uid->pw_uid;
			range = NUM_EQ;
		} else {
			TEST_SYNTAX_ERROR(test, 1, "Invalid uid or unknown "
								"user\n");
			return 0;
		}
	}

	number = malloc(sizeof(*number));
	if(number == NULL)
		MEM_ERROR();

	number->range = range;
	number->size= size;

	atom->data = number;

	return 1;
}


/*
 * gid specific test code
 */
TEST_VAR_FN(gid, ACTION_ALL_LNK, action_data->buf->st_gid)

static int parse_gid_arg(struct test_entry *test, struct atom *atom)
{
	struct test_number_arg *number;
	long long size;
	int range;
	char *error;

	if(parse_number(atom->argv[0], &size, &range, &error)) {
		/* managed to fully parse argument as a number */
		if(size < 0 || size > (((long long) 1 << 32) - 1)) {
			TEST_SYNTAX_ERROR(test, 1, "Numeric gid out of "
								"range\n");
			return 0;
		}
	} else {
		/* couldn't parse (fully) as a number, is it a group name? */
		struct group *gid = getgrnam(atom->argv[0]);
		if(gid) {
			size = gid->gr_gid;
			range = NUM_EQ;
		} else {
			TEST_SYNTAX_ERROR(test, 1, "Invalid gid or unknown "
								"group\n");
			return 0;
		}
	}

	number = malloc(sizeof(*number));
	if(number == NULL)
		MEM_ERROR();

	number->range = range;
	number->size= size;

	atom->data = number;

	return 1;
}


/*
 * Type test specific code
 */
struct type_entry type_table[] = {
	{ S_IFSOCK, 's' },
	{ S_IFLNK, 'l' },
	{ S_IFREG, 'f' },
	{ S_IFBLK, 'b' },
	{ S_IFDIR, 'd' },
	{ S_IFCHR, 'c' },
	{ S_IFIFO, 'p' },
	{ 0, 0 },
};


static int parse_type_arg(struct test_entry *test, struct atom *atom)
{
	int i;

	if (strlen(atom->argv[0]) != 1)
		goto failed;

	for(i = 0; type_table[i].type != 0; i++)
		if (type_table[i].type == atom->argv[0][0])
			break;

	atom->data = &type_table[i];

	if(type_table[i].type != 0)
		return 1;

failed:
	TEST_SYNTAX_ERROR(test, 0, "Unexpected file type, expected 'f', 'd', "
		"'c', 'b', 'l', 's' or 'p'\n");
	return 0;
}
	

static int type_fn(struct atom *atom, struct action_data *action_data)
{
	struct type_entry *type = atom->data;

	return (action_data->buf->st_mode & S_IFMT) == type->value;
}


/*
 * True test specific code
 */
static int true_fn(struct atom *atom, struct action_data *action_data)
{
	return 1;
}


/*
 *  False test specific code
 */
static int false_fn(struct atom *atom, struct action_data *action_data)
{
	return 0;
}


/*
 *  File test specific code
 */
static int parse_file_arg(struct test_entry *test, struct atom *atom)
{
	int res;
	regex_t *preg = malloc(sizeof(regex_t));

	if (preg == NULL)
		MEM_ERROR();

	res = regcomp(preg, atom->argv[0], REG_EXTENDED);
	if (res) {
		char str[1024]; /* overflow safe */

		regerror(res, preg, str, 1024);
		free(preg);
		TEST_SYNTAX_ERROR(test, 0, "invalid regex \"%s\" because "
			"\"%s\"\n", atom->argv[0], str);
		return 0;
	}

	atom->data = preg;

	return 1;
}


static int file_fn(struct atom *atom, struct action_data *action_data)
{
	int child, res, size = 0, status;
	int pipefd[2];
	char *buffer = NULL;
	regex_t *preg = atom->data;

	res = pipe(pipefd);
	if (res == -1)
		BAD_ERROR("file_fn pipe failed\n");

	child = fork();
	if (child == -1)
		BAD_ERROR("file_fn fork_failed\n");

	if (child == 0) {
		/*
		 * Child process
		 * Connect stdout to pipefd[1] and execute file command
		 */
		close(STDOUT_FILENO);
		res = dup(pipefd[1]);
		if (res == -1)
			exit(EXIT_FAILURE);

		execlp("file", "file", "-b", action_data->pathname,
			(char *) NULL);
		exit(EXIT_FAILURE);
	}

	/*
	 * Parent process.  Read stdout from file command
 	 */
	close(pipefd[1]);

	do {
		buffer = realloc(buffer, size + 512);
		if (buffer == NULL)
			MEM_ERROR();

		res = read_bytes(pipefd[0], buffer + size, 512);

		if (res == -1)
			BAD_ERROR("file_fn pipe read error\n");

		size += 512;

	} while (res == 512);

	size = size + res - 512;

	buffer[size] = '\0';

	res = waitpid(child,  &status, 0);

	if (res == -1)
		BAD_ERROR("file_fn waitpid failed\n");
 
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
		BAD_ERROR("file_fn file returned error\n");

	close(pipefd[0]);

	res = regexec(preg, buffer, (size_t) 0, NULL, 0);

	free(buffer);

	return res == 0;
}


/*
 *  Exec test specific code
 */
static int exec_fn(struct atom *atom, struct action_data *action_data)
{
	int child, i, res, status;

	child = fork();
	if (child == -1)
		BAD_ERROR("exec_fn fork_failed\n");

	if (child == 0) {
		/*
		 * Child process
		 * redirect stdin, stdout & stderr to /dev/null and
		 * execute atom->argv[0]
		 */
		int fd = open("/dev/null", O_RDWR);
		if(fd == -1)
			exit(EXIT_FAILURE);

		close(STDIN_FILENO);
		close(STDOUT_FILENO);
		close(STDERR_FILENO);
		for(i = 0; i < 3; i++) {
			res = dup(fd);
			if (res == -1)
				exit(EXIT_FAILURE);
		}
		close(fd);

		/*
		 * Create environment variables
		 * NAME: name of file
		 * PATHNAME: pathname of file relative to squashfs root
		 * SOURCE_PATHNAME: the pathname of the file in the source
		 *                  directory
		 */
		res = setenv("NAME", action_data->name, 1);
		if(res == -1)
			exit(EXIT_FAILURE);

		res = setenv("PATHNAME", action_data->subpath, 1);
		if(res == -1)
			exit(EXIT_FAILURE);

		res = setenv("SOURCE_PATHNAME", action_data->pathname, 1);
		if(res == -1)
			exit(EXIT_FAILURE);

		execl("/bin/sh", "sh", "-c", atom->argv[0], (char *) NULL);
		exit(EXIT_FAILURE);
	}

	/*
	 * Parent process. 
 	 */

	res = waitpid(child,  &status, 0);

	if (res == -1)
		BAD_ERROR("exec_fn waitpid failed\n");
 
	return WIFEXITED(status) ? WEXITSTATUS(status) == 0 : 0;
}


/*
 * Symbolic link specific test code
 */

/*
 * Walk the supplied pathname and return the directory entry corresponding
 * to the pathname.  If any symlinks are encountered whilst walking the
 * pathname, then recursively walk these, to obtain the fully
 * dereferenced canonicalised directory entry.
 *
 * If follow_path fails to walk a pathname either because a component
 * doesn't exist, it is a non directory component when a directory
 * component is expected, a symlink with an absolute path is encountered,
 * or a symlink is encountered which cannot be recursively walked due to
 * the above failures, then return NULL.
 */
static struct dir_ent *follow_path(struct dir_info *dir, char *pathname)
{
	char *comp, *path = pathname;
	struct dir_ent *dir_ent = NULL;

	/* We cannot follow absolute paths */
	if(pathname[0] == '/')
		return NULL;

	for(comp = get_comp(&path); comp; free(comp), comp = get_comp(&path)) {
		if(strcmp(comp, ".") == 0)
			continue;

		if(strcmp(comp, "..") == 0) {
			/* Move to parent if we're not in the root directory */
			if(dir->depth > 1) {
				dir = dir->dir_ent->our_dir;
				dir_ent = NULL; /* lazily eval at loop exit */
				continue;
			} else
				/* Failed to walk pathname */
				return NULL;
		}

		/* Lookup comp in current directory */
		dir_ent = lookup_comp(comp, dir);
		if(dir_ent == NULL)
			/* Doesn't exist, failed to walk pathname */
			return NULL;

		if((dir_ent->inode->buf.st_mode & S_IFMT) == S_IFLNK) {
			/* Symbolic link, try to walk it */
			dir_ent = follow_path(dir, dir_ent->inode->symlink);
			if(dir_ent == NULL)
				/* Failed to follow symlink */
				return NULL;
		}

		if((dir_ent->inode->buf.st_mode & S_IFMT) != S_IFDIR)
			/* Cannot walk further */
			break;

		dir = dir_ent->dir;
	}

	/* We will have exited the loop either because we've processed
	 * all the components, which means we've successfully walked the
	 * pathname, or because we've hit a non-directory, in which case
	 * it's success if this is the leaf component */
	if(comp) {
		free(comp);
		comp = get_comp(&path);
		free(comp);
		if(comp != NULL)
			/* Not a leaf component */
			return NULL;
	} else {
		/* Fully walked pathname, dir_ent contains correct value unless
		 * we've walked to the parent ("..") in which case we need
		 * to resolve it here */
		if(!dir_ent)
			dir_ent = dir->dir_ent;
	}

	return dir_ent;
}


static int exists_fn(struct atom *atom, struct action_data *action_data)
{
	/*
	 * Test if a symlink exists within the output filesystem, that is,
	 * the symlink has a relative path, and the relative path refers
	 * to an entry within the output filesystem.
	 *
	 * This test function evaluates the path for symlinks - that is it
	 * follows any symlinks in the path (and any symlinks that it contains
 	 * etc.), to discover the fully dereferenced canonicalised relative
	 * path.
	 *
	 * If any symlinks within the path do not exist or are absolute
	 * then the symlink is considered to not exist, as it cannot be
	 * fully dereferenced.
	 *
	 * exists operates on symlinks only, other files by definition
	 * exist
	 */
	if (!file_type_match(action_data->buf->st_mode, ACTION_LNK))
		return 1;

	/* dereference the symlink, and return TRUE if it exists */
	return follow_path(action_data->dir_ent->our_dir,
			action_data->dir_ent->inode->symlink) ? 1 : 0;
}


static int absolute_fn(struct atom *atom, struct action_data *action_data)
{
	/*
	 * Test if a symlink has an absolute path, which by definition
	 * means the symbolic link may be broken (even if the absolute path
	 * does point into the filesystem being squashed, because the resultant
	 * filesystem can be mounted/unsquashed anywhere, it is unlikely the
	 * absolute path will still point to the right place).  If you know that
	 * an absolute symlink will point to the right place then you don't need
	 * to use this function, and/or these symlinks can be excluded by
	 * use of other test operators.
	 *
	 * absolute operates on symlinks only, other files by definition
	 * don't have problems
	 */
	if (!file_type_match(action_data->buf->st_mode, ACTION_LNK))
		return 0;

	return action_data->dir_ent->inode->symlink[0] == '/';
}


static int parse_expr_argX(struct test_entry *test, struct atom *atom,
	int argno)
{
	/* Call parse_expr to parse argument, which should be an expression */

	 /* save the current parser state */
	char *save_cur_ptr = cur_ptr;
	char *save_source = source;

	cur_ptr = source = atom->argv[argno];
	atom->data = parse_expr(0);

	cur_ptr = save_cur_ptr;
	source = save_source;

	if(atom->data == NULL) {
		/* parse_expr(0) will have reported the exact syntax error,
		 * but, because we recursively evaluated the expression, it
		 * will have been reported without the context of the stat
		 * test().  So here additionally report our failure to parse
		 * the expression in the stat() test to give context */
		TEST_SYNTAX_ERROR(test, 0, "Failed to parse expression\n");
		return 0;
	}

	return 1;
}


static int parse_expr_arg0(struct test_entry *test, struct atom *atom)
{
	return parse_expr_argX(test, atom, 0);
}


static int parse_expr_arg1(struct test_entry *test, struct atom *atom)
{
	return parse_expr_argX(test, atom, 1);
}


static int stat_fn(struct atom *atom, struct action_data *action_data)
{
	struct stat buf;
	struct action_data eval_action;
	int match, res;

	/* evaluate the expression using the context of the inode
	 * pointed to by the symlink.  This allows the inode attributes
	 * of the file pointed to by the symlink to be evaluated, rather
	 * than the symlink itself.
	 *
	 * Note, stat() deliberately does not evaluate the pathname, name or
	 * depth of the symlink, these are left with the symlink values.
	 * This allows stat() to be used on any symlink, rather than
	 * just symlinks which are contained (if the symlink is *not*
	 * contained then pathname, name and depth are meaningless as they
	 * are relative to the filesystem being squashed). */

	/* if this isn't a symlink then stat will just return the current
	 * information, i.e. stat(expr) == expr.  This is harmless and
	 * is better than returning TRUE or FALSE in a non symlink case */
	res = stat(action_data->pathname, &buf);
	if(res == -1) {
		if(expr_log_cmnd(LOG_ENABLED)) {
			expr_log(atom->test->name);
			expr_log("(");
			expr_log_match(0);
			expr_log(")");
		}
		return 0;
	}

	/* fill in the inode values of the file pointed to by the
	 * symlink, but, leave everything else the same */
	memcpy(&eval_action, action_data, sizeof(struct action_data));
	eval_action.buf = &buf;

	if(expr_log_cmnd(LOG_ENABLED)) {
		expr_log(atom->test->name);
		expr_log("(");
		match = eval_expr_log(atom->data, &eval_action);
		expr_log(")");
	} else
		match = eval_expr(atom->data, &eval_action);

	return match;
}


static int readlink_fn(struct atom *atom, struct action_data *action_data)
{
	int match = 0;
	struct dir_ent *dir_ent;
	struct action_data eval_action;

	/* Dereference the symlink and evaluate the expression in the
	 * context of the file pointed to by the symlink.
	 * All attributes are updated to refer to the file that is pointed to.
	 * Thus the inode attributes, pathname, name and depth all refer to
	 * the dereferenced file, and not the symlink.
	 *
	 * If the symlink cannot be dereferenced because it doesn't exist in
	 * the output filesystem, or due to some other failure to
	 * walk the pathname (see follow_path above), then FALSE is returned.
	 *
	 * If you wish to evaluate the inode attributes of symlinks which
	 * exist in the source filestem (but not in the output filesystem then
	 * use stat instead (see above).
	 *
	 * readlink operates on symlinks only */
	if (!file_type_match(action_data->buf->st_mode, ACTION_LNK))
		goto finish;

	/* dereference the symlink, and get the directory entry it points to */
	dir_ent = follow_path(action_data->dir_ent->our_dir,
			action_data->dir_ent->inode->symlink);
	if(dir_ent == NULL)
		goto finish;

	eval_action.name = dir_ent->name;
	eval_action.pathname = strdup(pathname(dir_ent));
	eval_action.subpath = strdup(subpathname(dir_ent));
	eval_action.buf = &dir_ent->inode->buf;
	eval_action.depth = dir_ent->our_dir->depth;
	eval_action.dir_ent = dir_ent;
	eval_action.root = action_data->root;

	if(expr_log_cmnd(LOG_ENABLED)) {
		expr_log(atom->test->name);
		expr_log("(");
		match = eval_expr_log(atom->data, &eval_action);
		expr_log(")");
	} else
		match = eval_expr(atom->data, &eval_action);

	free(eval_action.pathname);
	free(eval_action.subpath);

	return match;

finish:
	if(expr_log_cmnd(LOG_ENABLED)) {
		expr_log(atom->test->name);
		expr_log("(");
		expr_log_match(0);
		expr_log(")");
	}

	return 0;
}


static int eval_fn(struct atom *atom, struct action_data *action_data)
{
	int match;
	char *path = atom->argv[0];
	struct dir_ent *dir_ent = action_data->dir_ent;
	struct stat *buf = action_data->buf;
	struct action_data eval_action;

	/* Follow path (arg1) and evaluate the expression (arg2)
	 * in the context of the file discovered.  All attributes are updated
	 * to refer to the file that is pointed to.
	 *
	 * This test operation allows you to add additional context to the
	 * evaluation of the file being scanned, such as "if current file is
	 * XXX and the parent is YYY, then ..."  Often times you need or
	 * want to test a combination of file status
	 *
	 * If the file referenced by the path does not exist in
	 * the output filesystem, or some other failure is experienced in
	 * walking the path (see follow_path above), then FALSE is returned.
	 *
	 * If you wish to evaluate the inode attributes of files which
	 * exist in the source filestem (but not in the output filesystem then
	 * use stat instead (see above). */

	/* try to follow path, and get the directory entry it points to */
	if(path[0] == '/') {
		/* absolute, walk from root - first skip the leading / */
		while(path[0] == '/')
			path ++;
		if(path[0] == '\0')
			dir_ent = action_data->root->dir_ent;
		else
			dir_ent = follow_path(action_data->root, path);
	} else {
		/* relative, if first component is ".." walk from parent,
		 * otherwise walk from dir_ent.
		 * Note: this has to be handled here because follow_path
		 * will quite correctly refuse to execute ".." on anything
		 * which isn't a directory */
		if(strncmp(path, "..", 2) == 0 && (path[2] == '\0' ||
							path[2] == '/')) {
			/* walk from parent */
			path += 2;
			while(path[0] == '/')
				path ++;
			if(path[0] == '\0')
				dir_ent = dir_ent->our_dir->dir_ent;
			else 
				dir_ent = follow_path(dir_ent->our_dir, path);
		} else if(!file_type_match(buf->st_mode, ACTION_DIR))
			dir_ent = NULL;
		else
			dir_ent = follow_path(dir_ent->dir, path);
	}

	if(dir_ent == NULL) {
		if(expr_log_cmnd(LOG_ENABLED)) {
			expr_log(atom->test->name);
			expr_log("(");
			expr_log(atom->argv[0]);
			expr_log(",");
			expr_log_match(0);
			expr_log(")");
		}

		return 0;
	}

	eval_action.name = dir_ent->name;
	eval_action.pathname = strdup(pathname(dir_ent));
	eval_action.subpath = strdup(subpathname(dir_ent));
	eval_action.buf = &dir_ent->inode->buf;
	eval_action.depth = dir_ent->our_dir->depth;
	eval_action.dir_ent = dir_ent;
	eval_action.root = action_data->root;

	if(expr_log_cmnd(LOG_ENABLED)) {
		expr_log(atom->test->name);
		expr_log("(");
		expr_log(eval_action.subpath);
		expr_log(",");
		match = eval_expr_log(atom->data, &eval_action);
		expr_log(")");
	} else
		match = eval_expr(atom->data, &eval_action);

	free(eval_action.pathname);
	free(eval_action.subpath);

	return match;
}


/*
 * Perm specific test code
 */
static int parse_perm_args(struct test_entry *test, struct atom *atom)
{
	int res = 1, mode, op, i;
	char *arg;
	struct mode_data *head = NULL, *cur = NULL;
	struct perm_data *perm_data;

	if(atom->args == 0) {
		TEST_SYNTAX_ERROR(test, 0, "One or more arguments expected\n");
		return 0;
	}

	switch(atom->argv[0][0]) {
	case '-':
		op = PERM_ALL;
		arg = atom->argv[0] + 1;
		break;
	case '/':
		op = PERM_ANY;
		arg = atom->argv[0] + 1;
		break;
	default:
		op = PERM_EXACT;
		arg = atom->argv[0];
		break;
	}

	/* try to parse as an octal number */
	res = parse_octal_mode_args(atom->args, atom->argv, (void **) &head);
	if(res == -1) {
		/* parse as sym mode argument */
		for(i = 0; i < atom->args && res; i++, arg = atom->argv[i])
			res = parse_sym_mode_arg(arg, &head, &cur);
	}

	if (res == 0)
		goto finish;

	/*
	 * Evaluate the symbolic mode against a permission of 0000 octal
	 */
	mode = mode_execute(head, 0);

	perm_data = malloc(sizeof(struct perm_data));
	if (perm_data == NULL)
		MEM_ERROR();

	perm_data->op = op;
	perm_data->mode = mode;

	atom->data = perm_data;
	
finish:
	while(head) {
		struct mode_data *tmp = head;
		head = head->next;
		free(tmp);
	}

	return res;
}


static int perm_fn(struct atom *atom, struct action_data *action_data)
{
	struct perm_data *perm_data = atom->data;
	struct stat *buf = action_data->buf;

	switch(perm_data->op) {
	case PERM_EXACT:
		return (buf->st_mode & ~S_IFMT) == perm_data->mode;
	case PERM_ALL:
		return (buf->st_mode & perm_data->mode) == perm_data->mode;
	case PERM_ANY:
	default:
		/*
		 * if no permission bits are set in perm_data->mode match
		 * on any file, this is to be consistent with find, which
		 * does this to be consistent with the behaviour of
		 * -perm -000
		 */
		return perm_data->mode == 0 || (buf->st_mode & perm_data->mode);
	}
}


#ifdef SQUASHFS_TRACE
static void dump_parse_tree(struct expr *expr)
{
	int i;

	if(expr->type == ATOM_TYPE) {
		printf("%s", expr->atom.test->name);
		if(expr->atom.args) {
			printf("(");
			for(i = 0; i < expr->atom.args; i++) {
				printf("%s", expr->atom.argv[i]);
				if (i + 1 < expr->atom.args)
					printf(",");
			}
			printf(")");
		}
	} else if (expr->type == UNARY_TYPE) {
		printf("%s", token_table[expr->unary_op.op].string);
		dump_parse_tree(expr->unary_op.expr);
	} else {
		printf("(");
		dump_parse_tree(expr->expr_op.lhs);
		printf("%s", token_table[expr->expr_op.op].string);
		dump_parse_tree(expr->expr_op.rhs);
		printf(")");
	}
}


void dump_action_list(struct action *spec_list, int spec_count)
{
	int i;

	for (i = 0; i < spec_count; i++) {
		printf("%s", spec_list[i].action->name);
		if (spec_list[i].args) {
			int n;

			printf("(");
			for (n = 0; n < spec_list[i].args; n++) {
				printf("%s", spec_list[i].argv[n]);
				if (n + 1 < spec_list[i].args)
					printf(",");
			}
			printf(")");
		}
		printf("=");
		dump_parse_tree(spec_list[i].expr);
		printf("\n");
	}
}


void dump_actions()
{
	dump_action_list(exclude_spec, exclude_count);
	dump_action_list(fragment_spec, fragment_count);
	dump_action_list(other_spec, other_count);
	dump_action_list(move_spec, move_count);
	dump_action_list(empty_spec, empty_count);
}
#else
void dump_actions()
{
}
#endif


static struct test_entry test_table[] = {
	{ "name", 1, name_fn, NULL, 1},
	{ "pathname", 1, pathname_fn, check_pathname, 1, 0},
	{ "subpathname", 1, subpathname_fn, check_pathname, 1, 0},
	{ "filesize", 1, filesize_fn, parse_number_arg, 1, 0},
	{ "dirsize", 1, dirsize_fn, parse_number_arg, 1, 0},
	{ "size", 1, size_fn, parse_number_arg, 1, 0},
	{ "inode", 1, inode_fn, parse_number_arg, 1, 0},
	{ "nlink", 1, nlink_fn, parse_number_arg, 1, 0},
	{ "fileblocks", 1, fileblocks_fn, parse_number_arg, 1, 0},
	{ "dirblocks", 1, dirblocks_fn, parse_number_arg, 1, 0},
	{ "blocks", 1, blocks_fn, parse_number_arg, 1, 0},
	{ "gid", 1, gid_fn, parse_gid_arg, 1, 0},
	{ "uid", 1, uid_fn, parse_uid_arg, 1, 0},
	{ "depth", 1, depth_fn, parse_number_arg, 1, 0},
	{ "dircount", 1, dircount_fn, parse_number_arg, 0, 0},
	{ "filesize_range", 2, filesize_range_fn, parse_range_args, 1, 0},
	{ "dirsize_range", 2, dirsize_range_fn, parse_range_args, 1, 0},
	{ "size_range", 2, size_range_fn, parse_range_args, 1, 0},
	{ "inode_range", 2, inode_range_fn, parse_range_args, 1, 0},
	{ "nlink_range", 2, nlink_range_fn, parse_range_args, 1, 0},
	{ "fileblocks_range", 2, fileblocks_range_fn, parse_range_args, 1, 0},
	{ "dirblocks_range", 2, dirblocks_range_fn, parse_range_args, 1, 0},
	{ "blocks_range", 2, blocks_range_fn, parse_range_args, 1, 0},
	{ "gid_range", 2, gid_range_fn, parse_range_args, 1, 0},
	{ "uid_range", 2, uid_range_fn, parse_range_args, 1, 0},
	{ "depth_range", 2, depth_range_fn, parse_range_args, 1, 0},
	{ "dircount_range", 2, dircount_range_fn, parse_range_args, 0, 0},
	{ "type", 1, type_fn, parse_type_arg, 1, 0},
	{ "true", 0, true_fn, NULL, 1, 0},
	{ "false", 0, false_fn, NULL, 1, 0},
	{ "file", 1, file_fn, parse_file_arg, 1, 0},
	{ "exec", 1, exec_fn, NULL, 1, 0},
	{ "exists", 0, exists_fn, NULL, 0, 0},
	{ "absolute", 0, absolute_fn, NULL, 0, 0},
	{ "stat", 1, stat_fn, parse_expr_arg0, 1, 1},
	{ "readlink", 1, readlink_fn, parse_expr_arg0, 0, 1},
	{ "eval", 2, eval_fn, parse_expr_arg1, 0, 1},
	{ "perm", -2, perm_fn, parse_perm_args, 1, 0},
	{ "", -1 }
};


static struct action_entry action_table[] = {
	{ "fragment", FRAGMENT_ACTION, 1, ACTION_REG, NULL, NULL},
	{ "exclude", EXCLUDE_ACTION, 0, ACTION_ALL_LNK, NULL, NULL},
	{ "fragments", FRAGMENTS_ACTION, 0, ACTION_REG, NULL, frag_action},
	{ "no-fragments", NO_FRAGMENTS_ACTION, 0, ACTION_REG, NULL,
						no_frag_action},
	{ "always-use-fragments", ALWAYS_FRAGS_ACTION, 0, ACTION_REG, NULL,
						always_frag_action},
	{ "dont-always-use-fragments", NO_ALWAYS_FRAGS_ACTION, 0, ACTION_REG,	
						NULL, no_always_frag_action},
	{ "compressed", COMPRESSED_ACTION, 0, ACTION_REG, NULL, comp_action},
	{ "uncompressed", UNCOMPRESSED_ACTION, 0, ACTION_REG, NULL,
						uncomp_action},
	{ "uid", UID_ACTION, 1, ACTION_ALL_LNK, parse_uid_args, uid_action},
	{ "gid", GID_ACTION, 1, ACTION_ALL_LNK, parse_gid_args, gid_action},
	{ "guid", GUID_ACTION, 2, ACTION_ALL_LNK, parse_guid_args, guid_action},
	{ "mode", MODE_ACTION, -2, ACTION_ALL, parse_mode_args, mode_action },
	{ "empty", EMPTY_ACTION, -2, ACTION_DIR, parse_empty_args, NULL},
	{ "move", MOVE_ACTION, 1, ACTION_ALL_LNK, NULL, NULL},
	{ "prune", PRUNE_ACTION, 0, ACTION_ALL_LNK, NULL, NULL},
	{ "chmod", MODE_ACTION, -2, ACTION_ALL, parse_mode_args, mode_action },
	{ "noop", NOOP_ACTION, 0, ACTION_ALL, NULL, noop_action },
	{ "", 0, -1, 0, NULL, NULL}
};
