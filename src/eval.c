#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>

#include <check.h>

#include "bit.h"
#include "pos.h"
#include "move.h"
#include "movegen.h"
#include "eval.h"
#include "rng.h"

/*
 * The set of random numbers in the Zobrist array map to each possible variation
 * in the state of the position. 12 * 64 random numbers for each piece on each
 * square, 16 permutations of castling rights, 8 possible en passant files and
 * finally 1 possible variation of color when it is black instead of white.
 */
#define NUM_PIECES 12
#define NUM_SQUARES 64
#define NUM_CASTLING_RIGHTS 16
#define NUM_EN_PASSANT_FILES 8
#define NUM_COLOR_VARIATION 1
#define ZOBRIST_ARRAY_SIZE (NUM_PIECES * NUM_SQUARES + NUM_CASTLING_RIGHTS +\
                            NUM_EN_PASSANT_FILES + NUM_COLOR_VARIATION)

enum piece_values {
	PIECE_VALUE_PAWN = 100,
	PIECE_VALUE_KNIGHT = 300,
	PIECE_VALUE_BISHOP = 350,
	PIECE_VALUE_ROOK = 500,
	PIECE_VALUE_QUEEN = 1000,
	PIECE_VALUE_KING = 10000,
};

struct transposition_table {
	NodeData *ptr;
	size_t capacity;
} transposition_table = {.ptr = NULL, .capacity = 0};

static u64 zobrist_numbers[ZOBRIST_ARRAY_SIZE];

/*
 * Generate a set of unique random numbers for Zobrist hashing.
 */
static void init_hash(void)
{
	for (size_t i = 0; i < ZOBRIST_ARRAY_SIZE; ++i) {
		zobrist_numbers[i] = rng_next();
		for (size_t j = 0; j < i; ++j) {
			if (zobrist_numbers[i] == zobrist_numbers[j])
				--i;
		}
	}
}

static u64 hash(const Position *pos)
{
	u64 key = 0;
	u64 *ptr = zobrist_numbers;

	for (Square sq = 0; sq < NUM_SQUARES; ++sq) {
		const Piece piece = pos_get_piece_at(pos, sq);
		if (piece != PIECE_NONE)
			key ^= ptr[NUM_SQUARES * piece + sq];
	}
	ptr += NUM_PIECES * NUM_SQUARES;

	u8 rights = 0;
	for (Color color = COLOR_WHITE; color <= COLOR_BLACK; ++color) {
		const bool has_king_right = pos_has_castling_right(pos, color, CASTLING_SIDE_KING);
		const bool has_queen_right = pos_has_castling_right(pos, color, CASTLING_SIDE_QUEEN);
		rights = ((has_king_right << 1) | has_queen_right) << (2 * color);
	}
	key ^= ptr[rights];
	ptr += NUM_CASTLING_RIGHTS;

	if (pos_enpassant_possible(pos)) {
		const Square sq = pos_get_enpassant(pos);
		const File file = pos_get_file_of_square(sq);
		key ^= ptr[file];
	}
	ptr += NUM_EN_PASSANT_FILES;

	if (pos_get_side_to_move(pos) == COLOR_BLACK)
		key ^= ptr[0];

	return key;
}

static void init_position_table(void)
{
	transposition_table.capacity = 2 << 20;
	transposition_table.ptr = calloc(transposition_table.capacity, sizeof(NodeData));
	if (!transposition_table.ptr) {
		fprintf(stderr, "Could not allocate memory");
		exit(1);
	}
}

static void finish_position_table(void)
{
	transposition_table.capacity = 0;
	free(transposition_table.ptr);
	transposition_table.ptr = NULL;
}

static int compute_mobility(const Position *pos)
{
	const int c = pos_get_side_to_move(pos);
	const int mobility = movegen_get_number_of_pseudo_legal_moves(pos, c) -
	                     movegen_get_number_of_pseudo_legal_moves(pos, !c);

	return mobility;
}

static int compute_material(const Position *pos)
{
	const Color c = pos_get_side_to_move(pos);
	const int P = pos_make_piece(PIECE_TYPE_PAWN  , c),
	          N = pos_make_piece(PIECE_TYPE_KNIGHT, c),
	          R = pos_make_piece(PIECE_TYPE_ROOK  , c),
	          B = pos_make_piece(PIECE_TYPE_BISHOP, c),
	          Q = pos_make_piece(PIECE_TYPE_QUEEN , c),
	          K = pos_make_piece(PIECE_TYPE_KING  , c);
	const int p = pos_make_piece(PIECE_TYPE_PAWN  , !c),
	          n = pos_make_piece(PIECE_TYPE_KNIGHT, !c),
	          r = pos_make_piece(PIECE_TYPE_ROOK  , !c),
	          b = pos_make_piece(PIECE_TYPE_BISHOP, !c),
	          q = pos_make_piece(PIECE_TYPE_QUEEN , !c),
	          k = pos_make_piece(PIECE_TYPE_KING  , !c);
	const int num_P = pos_get_number_of_pieces(pos, P),
	          num_N = pos_get_number_of_pieces(pos, N),
	          num_R = pos_get_number_of_pieces(pos, R),
	          num_B = pos_get_number_of_pieces(pos, B),
	          num_Q = pos_get_number_of_pieces(pos, Q),
	          num_K = pos_get_number_of_pieces(pos, K);

	const int num_p = pos_get_number_of_pieces(pos, p),
	          num_n = pos_get_number_of_pieces(pos, n),
	          num_r = pos_get_number_of_pieces(pos, r),
	          num_b = pos_get_number_of_pieces(pos, b),
	          num_q = pos_get_number_of_pieces(pos, q),
	          num_k = pos_get_number_of_pieces(pos, k);
	const int material = PIECE_VALUE_PAWN   * (num_P - num_p)
	                   + PIECE_VALUE_KNIGHT * (num_N - num_n)
	                   + PIECE_VALUE_ROOK   * (num_R - num_r)
	                   + PIECE_VALUE_BISHOP * (num_B - num_b)
                           + PIECE_VALUE_QUEEN  * (num_Q - num_q)
	                   + PIECE_VALUE_KING   * (num_K - num_k);

	return material;
}

int eval_evaluate(const Position *pos)
{
	const int material_weight = 3;
	const int material = compute_material(pos);
	const int mobility = compute_mobility(pos);

	return material_weight * material + mobility;
}

int eval_evaluate_move(Move move, Position *pos)
{
	const int target_table[6] = {
		[PIECE_TYPE_PAWN  ] = PIECE_VALUE_PAWN,
		[PIECE_TYPE_KNIGHT] = PIECE_VALUE_KNIGHT,
		[PIECE_TYPE_BISHOP] = PIECE_VALUE_BISHOP,
		[PIECE_TYPE_ROOK  ] = PIECE_VALUE_ROOK,
		[PIECE_TYPE_QUEEN ] = PIECE_VALUE_QUEEN,
		[PIECE_TYPE_KING  ] = PIECE_VALUE_KING,
	};
	const int attacker_table[6] = {
		[PIECE_TYPE_PAWN  ] = PIECE_VALUE_KING,
		[PIECE_TYPE_KNIGHT] = PIECE_VALUE_QUEEN,
		[PIECE_TYPE_BISHOP] = PIECE_VALUE_ROOK,
		[PIECE_TYPE_ROOK  ] = PIECE_VALUE_BISHOP,
		[PIECE_TYPE_QUEEN ] = PIECE_VALUE_KNIGHT,
		[PIECE_TYPE_KING  ] = PIECE_VALUE_PAWN,
	};

	if (move_get_type(move) == MOVE_CAPTURE) {
		Square sq = move_get_target(move);
		Piece piece = pos_get_piece_at(pos, sq);
		PieceType target = pos_get_piece_type(piece);

		sq = move_get_origin(move);
		piece = pos_get_piece_at(pos, sq);
		PieceType attacker = pos_get_piece_type(piece);

		int value = target_table[target] + attacker_table[attacker];
		return value;
	} else {
		return 0;
	}
}

/*
 * It will return true if the node data is in the transposition table table and
 * false otherwise.
 */
bool eval_get_node_data(NodeData *data, const Position *pos)
{
	const u64 node_hash = hash(pos);
	const size_t key = node_hash % transposition_table.capacity;
	struct node_data tt_data = transposition_table.ptr[key];
	if (node_hash == tt_data.hash) {
		*data = tt_data;
		return true;
	}
	return false;
}

void eval_store_node_data(const NodeData *data)
{
	const size_t key = data->hash % transposition_table.capacity;
	transposition_table.ptr[key] = *data;
}

void eval_node_data_init(NodeData *data, int score, int depth, NodeType type, const Position *pos)
{
	data->score = score;
	data->depth = depth;
	data->type = type;
	data->hash = hash(pos);
}

void eval_init(void)
{
	init_position_table();
	init_hash();
}

void eval_finish(void)
{
	finish_position_table();
}
