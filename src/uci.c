#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <errno.h>
#include <stdarg.h>

#include <check.h>

#include "bit.h"
#include "pos.h"
#include "move.h"
#include "movegen.h"
#include "search.h"
#include "uci.h"

bool newgame_has_been_run = false;
Position *current_position = NULL;

static const size_t max_lan_len = 5;

#define OPTION_UCI_ANALYSISMODE_TYPE boolean
#define OPTION_HASH_TYPE integer
#define OPTION_PONDER_TYPE boolean
#define OPTION_VALUE_TYPE(name) OPTION_##name##_TYPE

enum option_type {
	OPTION_TYPE_BOOLEAN,
	OPTION_TYPE_INTEGER,
	OPTION_TYPE_STRING,
};

union option_value {
	bool boolean;
	int integer;
	char *string;
};

struct option {
	char *name;
	enum option_type type;
	union option_value default_value;
	union option_value value;
	int min;
	int max;
} options[] = {
	{.name = "UCI_AnalyseMode", .type = OPTION_TYPE_BOOLEAN, .default_value.boolean = false, .value.boolean = false},
	{.name = "Hash", .type = OPTION_TYPE_INTEGER, .default_value.integer = 64, .value.integer = 64, .min = 64, .max = 32768},
	{.name = "Ponder", .type = OPTION_TYPE_BOOLEAN, .default_value.boolean = false, .value.boolean = false},
};

/*
 * I used the same promotion_to_char table for both promotions and promotions
 * with captures because the number of promotions with captures are the same
 * as the number of promotions and they are 4 numbers apart.
 */
static void move_to_lan(char *lan, Move move)
{
	const char file_to_char[] = {
		[FILE_A] = 'a', [FILE_B] = 'b', [FILE_C] = 'c', [FILE_D] = 'd',
		[FILE_E] = 'e', [FILE_F] = 'f', [FILE_G] = 'g', [FILE_H] = 'h',
	};
	const char rank_to_char[] = {
		[RANK_1] = '1', [RANK_2] = '2', [RANK_3] = '3', [RANK_4] = '4',
		[RANK_5] = '5', [RANK_6] = '6', [RANK_7] = '7', [RANK_8] = '8',
	};
	const char promotion_to_char[] = {
		[MOVE_KNIGHT_PROMOTION] = 'n', [MOVE_ROOK_PROMOTION ] = 'r',
		[MOVE_BISHOP_PROMOTION] = 'b', [MOVE_QUEEN_PROMOTION] = 'q',
	};

	if (!move) {
		memset(lan, '0', max_lan_len - 1);
		lan[max_lan_len - 1] = '\0';
		return;
	}

	const Square sq1 = move_get_origin(move);
	const Square sq2 = move_get_target(move);
	const MoveType type = move_get_type(move);
	const File file1 = pos_get_file_of_square(sq1);
	const File file2 = pos_get_file_of_square(sq2);
	const Rank rank1 = pos_get_rank_of_square(sq1);
	const Rank rank2 = pos_get_rank_of_square(sq2);

	lan[0] = file_to_char[file1];
	lan[1] = rank_to_char[rank1];
	lan[2] = file_to_char[file2];
	lan[3] = rank_to_char[rank2];
	if (type >= MOVE_KNIGHT_PROMOTION && type <= MOVE_QUEEN_PROMOTION)
		lan[4] = promotion_to_char[type];
	else if (type >= MOVE_KNIGHT_PROMOTION_CAPTURE && type <= MOVE_QUEEN_PROMOTION_CAPTURE)
		lan[4] = promotion_to_char[type - 4];
	else
		lan[4] = '\0';
	lan[5] = '\0';
}

/*
 * error is set to 1 if the LAN is invalid and anything may be returned.
 */
static Move lan_to_move(const char *lan, const Position *pos, bool *success)
{
	char test_lan[max_lan_len + 1];
	size_t num_moves;
	Move *moves = movegen_get_pseudo_legal_moves(pos, &num_moves);
	for (size_t i = 0; i < num_moves; ++i) {
		Move move = moves[i];
		move_to_lan(test_lan, move);
		if (!strcmp(test_lan, lan)) {
			free(moves);
			*success = true;
			return move;
		}
	}
	free(moves);

	*success = false;
	return 0xffff;
}

/*
 * Convert a string to a value for an option and return 0 on success, 1 if the
 * option was not found and 2 if str is not a valid value for the option.
 */
static int str_to_option_value(union option_value *value, const char *name, const char *str)
{
	const struct option *op;

	for (size_t i = 0; i < sizeof(options) / sizeof(options[0]); ++i) {
		op = &options[i];
		if (!strcmp(name, op->name)) {
			switch (op->type) {
			case OPTION_TYPE_BOOLEAN:
				goto boolean;
			case OPTION_TYPE_INTEGER:
				goto integer;
			case OPTION_TYPE_STRING:
				goto string;
			default:
				abort();
			}
		}
	}
	return 1;

boolean:
	if (!strcmp(str, "true"))
		value->boolean = true;
	else if (!strcmp(str, "false"))
		value->boolean = false;
	else
		return 2;
	return 0;
integer:
	errno = 0;
	char *endptr;
	long n = strtol(str, &endptr, 10);
	if (errno == ERANGE || endptr == str || *endptr != '\0' || n < op->min || n > op->max)
		return 2;
	value->integer = n;
	return 0;
string:
	value->string = malloc(strlen(str) + 1);
	strcpy(value->string, str);
	return 0;
}

static void bestmove(Move move)
{
	char lan[max_lan_len + 1];

	move_to_lan(lan, move);
	uci_send("bestmove %s", lan);
}

static void readyok(void)
{
	uci_send("readyok");
}

static void uciok(void)
{
	uci_send("uciok");
}

static void option(void)
{
	for (size_t i = 0; i < sizeof(options) / sizeof(options[0]); ++i) {
		const struct option *const op = &options[i];
		switch (op->type) {
		case OPTION_TYPE_BOOLEAN:
			if (op->default_value.boolean == false)
				uci_send("option name %s type check default %s",
				         op->name, "false");
			else
				uci_send("option name %s type check default %s",
				         op->name, "true");
			break;
		case OPTION_TYPE_INTEGER:
			uci_send("option name %s type spin default %d min %d "
			         "max %d", op->name, op->default_value.integer,
			         op->min, op->max);
			break;
		case OPTION_TYPE_STRING:
			uci_send("option name %s type string default %s",
			          op->name, op->default_value.string);
			break;
		}
	}
}

static void id(void)
{
	uci_send("id name Athena");
	uci_send("id author Aiya");
}

static void quit(void)
{
	if (current_position)
		pos_destroy(current_position);
	search_finish();
	for (size_t i = 0; i < sizeof(options) / sizeof(options[0]); ++i) {
		struct option *const op = &options[i];
		if (op->type == OPTION_TYPE_STRING)
			free(op->value.string);
	}
}

static void go(void)
{
	if (!current_position) {
		fprintf(stderr, "go command sent before position command.\n");
		return;
	}

	int depth = 0;

	char *str = strtok(NULL, " ");
	while (str) {
		if (!strcmp(str, "depth")) {
			str = strtok(NULL, " ");
			if (!str) {
				fprintf(stderr, "Invalid UCI command.\n");
				return;
			}
			char *endptr = NULL;
			errno = 0;
			depth = strtol(str, &endptr, 10);
			if (errno == ERANGE || endptr == str || *endptr != '\0')
				depth = 0;
		} else {
			break;
		}
		str = strtok(NULL, " ");
	}

	Move move = search_search(current_position, depth);
	bestmove(move);
}

static void ucinewgame(void)
{
	if (current_position)
		pos_destroy(current_position);
	search_finish();
	movegen_init();
	search_init();
	newgame_has_been_run = true;
}

static void position(char *split_str)
{
	if (!newgame_has_been_run)
		ucinewgame();
	char *startpos = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

	const char *token = strtok(NULL, " ");
	if (token && !strcmp(token, "startpos")) {
		if (current_position)
			pos_destroy(current_position);
		current_position = pos_create(startpos);
	} else if (token && !strcmp(token, "fen")) {
		char *fen = NULL;
		const size_t num_fen_parts = 6;
		char *parts[num_fen_parts];
		for (size_t i = 0; i < num_fen_parts; ++i) {
			parts[i] = strtok(NULL, " ");
			if (i == 0 && !strcmp(parts[i], "startpos")) {
				fen = startpos;
				break;
			}
			if (!parts[i])
				fprintf(stderr, "Invalid UCI command.\n");
		}
		size_t fen_len = 0;
		for (size_t i = 0; i < num_fen_parts; ++i) {
			size_t part_len = strlen(parts[i]);
			fen_len += part_len + 1; /* + 1 for space or '\0'. */
			char *tmp = realloc(fen, fen_len);
			if (!tmp) {
				fprintf(stderr, "Could not allocate memory.\n");
				free(fen);
				return;
			}
			fen = tmp;
			char *part_ptr = fen + fen_len - part_len - 1;
			strcpy(part_ptr, parts[i]);
			if (i < num_fen_parts - 1) {
				fen[fen_len - 1] = ' ';
			} else {
				fen[fen_len - 1] = '\0';
				/* Remove the extra 1 that was added in the
				 * beginning when it is for '\0'. */
				--fen_len;
			}
		}
		if (current_position)
			pos_destroy(current_position);
		current_position = pos_create(fen);
		if (!current_position) {
			fprintf(stderr, "Invalid UCI command.\n");
			free(fen);
			return;
		}
		free(fen);
	} else {
		fprintf(stderr, "Invalid UCI command.\n");
		return;
	}

	token = strtok(NULL, " ");
	if (!token)
		return;
	if (strcmp(token, "moves")) {
		fprintf(stderr, "Invalid UCI command\n");
		pos_destroy(current_position);
		current_position = NULL;
		return;
	}

	size_t num_moves = 0;
	size_t capacity_moves = 0;
	Move *moves = NULL;
	for (char *move = strtok(NULL, " "); move; move = strtok(NULL, " ")) {
		const size_t move_len = strlen(move);
		if (move_len > max_lan_len) {
			fprintf(stderr, "Invalid UCI command.\n");
			free(split_str);
			pos_destroy(current_position);
			current_position = NULL;
			return;
		}
		++num_moves;
		if (num_moves > capacity_moves) {
			capacity_moves += 128;
			Move *tmp = realloc(moves, capacity_moves);
			if (!tmp) {
				fprintf(stderr, "Could not allocate memory.\n");
				free(moves);
				pos_destroy(current_position);
				current_position = NULL;
				return ;
			}
			moves = tmp;
		}
		bool success;
		moves[num_moves - 1] = lan_to_move(move, current_position, &success);
		if (!success) {
			fprintf(stderr, "Invalid UCI command.\n");
			free(moves);
			pos_destroy(current_position);
			current_position = NULL;
			return;
		}
		move_do(current_position, moves[num_moves - 1]);
	}
	free(moves);
}

static void isready(void)
{
	readyok();
}

/*
 * Read all the words until str is found or the end of the string has been
 * reached, and return the full sentence or NULL if there is nothing before str.
 * This function may receive an extra argument of type (bool *) and if str is
 * found at the end of the string it will be set to true and false if not found.
 * If str is an empty string then the function will read until the end and the
 * extra argument is ignored.
 */
static char *read_words_until_equal(char *str, bool *allocation_error, ...)
{
	va_list ap;
	bool requires_extra = !!strlen(str);
	if (requires_extra)
		va_start(ap, allocation_error);

	size_t name_len = 0;
	*allocation_error = false;
	char *joined = NULL, *word = NULL;
	for (word = strtok(NULL, " "); word && strcmp(word, str); word = strtok(NULL, " ")) {
		const size_t word_len = strlen(word);
		name_len += word_len + 1;
		char *tmp = realloc(joined, name_len);
		if (!tmp) {
			*allocation_error = true;
			free(joined);
			return NULL;
		}
		joined = tmp;
		strcpy(joined + name_len - word_len - 1, word);
		joined[name_len - 1] = ' ';
	}
	if (!joined)
		return NULL;
	--name_len;
	joined[name_len] = '\0';
	if (requires_extra && word)
		*va_arg(ap, bool *) = true;
	return joined;
}

static void setoption(void)
{
	char *token = strtok(NULL, " ");
	if (!token || strcmp(token, "name")) {
		fprintf(stderr, "Invalid UCI command.\n");
		return;
	}

	bool has_value = false, allocation_error = false;
	char *name = read_words_until_equal("value", &allocation_error, &has_value);
	if (allocation_error) {
		fprintf(stderr, "Could not allocate memory.\n");
		return;
	}
	if (!name) {
		fprintf(stderr, "Invalid UCI command.\n");
		return;
	}
	/* I might change this when I implement an option that is a button. */
	if (!has_value) {
		fprintf(stderr, "Invalid UCI command.\n");
		free(name);
		return;
	}

	char *const value_str = read_words_until_equal("", &allocation_error);
	if (allocation_error) {
		fprintf(stderr, "Could not allocate memory.\n");
		free(name);
		return;
	}
	if (!value_str) {
		fprintf(stderr, "Invalid UCI command.\n");
		free(name);
		return;
	}

	union option_value value;
	switch (str_to_option_value(&value, name, value_str)) {
	case 0:
		break;
	case 1:
		fprintf(stderr, "Option %s not recognized.\n", name);
		free(name);
		free(value_str);
		return;
	case 2:
		fprintf(stderr, "%s is not a valid value for %s\n", value_str, name);
		free(name);
		free(value_str);
		return;
	default:
		abort();
	}

	for (size_t i = 0; i < sizeof(options) / sizeof(options[0]); ++i) {
		struct option *const op = &options[i];
		if (!strcmp(name, op->name)) {
			if (op->type == OPTION_TYPE_STRING)
				free(op->value.string);
			op->value = value;
			break;
		}
	}
	free(name);
	free(value_str);
}

static void uci(void)
{
	id();
	option();
	uciok();
}

/*
 * Return true normally and false when the "quit" command is used.
 */
bool uci_interpret(const char *str)
{
	bool ret = true;
	const size_t len = strlen(str);
	char *const split_str = malloc(len + 1);

	strcpy(split_str, str);
	char *const cmd = strtok(split_str, " ");

	if (!strcmp(cmd, "uci")) {
		uci();
	} else if (!strcmp(cmd, "isready")) {
		isready();
	} else if (!strcmp(cmd, "setoption")) {
		setoption();
	} else if (!strcmp(cmd, "ucinewgame")) {
		ucinewgame();
	} else if (!strcmp(cmd, "position")) {
		position(split_str);
	} else if (!strcmp(cmd, "go")) {
		go();
	} else if (!strcmp(cmd, "quit")) {
		quit();
		ret = false;
	}

	free(split_str);
	return ret;
}

void uci_send(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vprintf(fmt, args);
	putchar('\n');
	fflush(stdout);
}

/*
 * Read a UCI message from stdin and return it, or return NULL if the message is
 * invalid or an error occurred.
 */
char *uci_receive(void)
{
	size_t max_len = BUFSIZ;
	char *str = malloc(max_len + 1);

	size_t i = 0;
	char ch = '\0';
	for (ch = fgetc(stdin); ch != EOF; ch = fgetc(stdin)) {
		if (ch == '\n')
			break;
		if (i == max_len) {
			max_len += BUFSIZ;
			char *const tmp = realloc(str, max_len + 1);
			if (!tmp) {
				fprintf(stderr, "Could not allocate memory.\n");
				free(str);
				return NULL;
			}
			str = tmp;
		}
		str[i] = ch;
		++i;
	}
	if (ch != '\n') {
		fprintf(stderr, "Invalid UCI string.\n");
		free(str);
		return NULL;
	}
	str[i] = '\0';

	return str;
}
