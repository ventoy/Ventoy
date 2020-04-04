#ifndef ACTION_H
#define ACTION_H
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
 * action.h
 */

/*
 * Lexical analyser definitions
 */
#define TOK_OPEN_BRACKET	0
#define TOK_CLOSE_BRACKET	1
#define TOK_AND			2
#define TOK_OR			3
#define TOK_NOT			4
#define TOK_COMMA		5
#define TOK_AT			6
#define TOK_WHITE_SPACE		7
#define TOK_STRING		8
#define TOK_EOF			9

#define TOK_TO_STR(OP, S) ({ \
	char *s; \
	switch(OP) { \
	case TOK_EOF: \
		s = "EOF"; \
		break; \
	case TOK_STRING: \
		s = S; \
		break; \
	default: \
		s = token_table[OP].string; \
		break; \
	} \
	s; \
})


struct token_entry {
	char *string;
	int token;
	int size;
};

/*
 * Expression parser definitions
 */
#define OP_TYPE			0
#define ATOM_TYPE		1
#define UNARY_TYPE		2

#define SYNTAX_ERROR(S, ARGS...) { \
	char *src = strdup(source); \
	src[cur_ptr - source] = '\0'; \
	fprintf(stderr, "Failed to parse action \"%s\"\n", source); \
	fprintf(stderr, "Syntax error: "S, ##ARGS); \
	fprintf(stderr, "Got here \"%s\"\n", src); \
	free(src); \
}

#define TEST_SYNTAX_ERROR(TEST, ARG, S, ARGS...) { \
	char *src = strdup(source); \
	src[cur_ptr - source] = '\0'; \
	fprintf(stderr, "Failed to parse action \"%s\"\n", source); \
	fprintf(stderr, "Syntax error in \"%s()\", arg %d: "S, TEST->name, \
			 ARG, ##ARGS); \
	fprintf(stderr, "Got here \"%s\"\n", src); \
	free(src); \
}

struct expr;

struct expr_op {
	struct expr *lhs;
	struct expr *rhs;
	int op;
};


struct atom {
	struct test_entry *test;
	int args;
	char **argv;
	void *data;
};


struct unary_op {
	struct expr *expr;
	int op;
};


struct expr {
	int type;
	union {
		struct atom atom;
		struct expr_op expr_op;
		struct unary_op unary_op;
	};
};

/*
 * Test operation definitions
 */
#define NUM_EQ		1
#define NUM_LESS	2
#define NUM_GREATER	3

struct test_number_arg {
	long long size;
	int range;
};

struct test_range_args {
	long long start;
	long long end;
};

struct action;
struct action_data;

struct test_entry {
	char *name;
	int args;
	int (*fn)(struct atom *, struct action_data *);
	int (*parse_args)(struct test_entry *, struct atom *);
	int exclude_ok;
	int handle_logging;
};


/*
 * Type test specific definitions
 */
struct type_entry {
	int value;
	char type;
};


/*
 * Action definitions
 */
#define FRAGMENT_ACTION 0
#define EXCLUDE_ACTION 1
#define FRAGMENTS_ACTION 2
#define NO_FRAGMENTS_ACTION 3
#define ALWAYS_FRAGS_ACTION 4
#define NO_ALWAYS_FRAGS_ACTION 5
#define COMPRESSED_ACTION 6
#define UNCOMPRESSED_ACTION 7
#define UID_ACTION 8
#define GID_ACTION 9
#define GUID_ACTION 10
#define MODE_ACTION 11
#define EMPTY_ACTION 12
#define MOVE_ACTION 13
#define PRUNE_ACTION 14
#define NOOP_ACTION 15

/*
 * Define what file types each action operates over
 */
#define ACTION_DIR 1
#define ACTION_REG 2
#define ACTION_ALL_LNK 3
#define ACTION_ALL 4
#define ACTION_LNK 5


/*
 * Action logging requested, specified by the various
 * -action, -true-action, -false-action and -verbose-action
 * options
 */
#define ACTION_LOG_NONE	0
#define ACTION_LOG_TRUE 1
#define ACTION_LOG_FALSE 2
#define ACTION_LOG_VERBOSE ACTION_LOG_TRUE | ACTION_LOG_FALSE

struct action_entry {
	char *name;
	int type;
	int args;
	int file_types;
	int (*parse_args)(struct action_entry *, int, char **, void **);
	void (*run_action)(struct action *, struct dir_ent *);
};


struct action_data {
	int depth;
	char *name;
	char *pathname;
	char *subpath;
	struct stat *buf;
	struct dir_ent *dir_ent;
	struct dir_info *root;
};


struct action {
	int type;
	struct action_entry *action;
	int args;
	char **argv;
	struct expr *expr;
	void *data;
	int verbose;
};


/*
 * Uid/gid action specific definitions
 */
struct uid_info {
	uid_t uid;
};

struct gid_info {
	gid_t gid;
};

struct guid_info {
	uid_t uid;
	gid_t gid;
};


/*
 * Mode action specific definitions
 */
#define ACTION_MODE_SET 0
#define ACTION_MODE_ADD 1
#define ACTION_MODE_REM 2
#define ACTION_MODE_OCT 3

struct mode_data {
	struct mode_data *next;
	int operation;
	int mode;
	unsigned int mask;
	char X;
};


/*
 * Empty action specific definitions
 */
#define EMPTY_ALL 0
#define EMPTY_SOURCE 1
#define EMPTY_EXCLUDED 2

struct empty_data {
	int val;
};


/*
 * Move action specific definitions
 */
#define ACTION_MOVE_RENAME 1
#define ACTION_MOVE_MOVE 2

struct move_ent {
	int ops;
	struct dir_ent *dir_ent;
	char *name;
	struct dir_info *dest;
	struct move_ent *next;
};


/*
 * Perm test function specific definitions
 */
#define PERM_ALL 1
#define PERM_ANY 2
#define PERM_EXACT 3

struct perm_data {
	int op;
	int mode;
};


/*
 * External function definitions
 */
extern int parse_action(char *, int verbose);
extern void dump_actions();
extern void *eval_frag_actions(struct dir_info *, struct dir_ent *);
extern void *get_frag_action(void *);
extern int eval_exclude_actions(char *, char *, char *, struct stat *, int,
							struct dir_ent *);
extern void eval_actions(struct dir_info *, struct dir_ent *);
extern int eval_empty_actions(struct dir_info *, struct dir_ent *dir_ent);
extern void eval_move_actions(struct dir_info *, struct dir_ent *);
extern int eval_prune_actions(struct dir_info *, struct dir_ent *);
extern void do_move_actions();
extern int read_bytes(int, void *, int);
extern int actions();
extern int move_actions();
extern int empty_actions();
extern int read_action_file(char *, int);
extern int exclude_actions();
extern int prune_actions();
#endif
