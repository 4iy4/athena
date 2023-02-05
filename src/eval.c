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

enum piece_values {
	PIECE_VALUE_PAWN = 100,
	PIECE_VALUE_KNIGHT = 320,
	PIECE_VALUE_BISHOP = 350,
	PIECE_VALUE_ROOK = 500,
	PIECE_VALUE_QUEEN = 1000,
	PIECE_VALUE_KING = 10000,
};

/*
 * These tables store the number of possible moves for a piece when the board
 * contains only that piece, so no occupancy for sliding pieces.
 */
static i8 white_pawn_number_of_possible_moves[64];
static i8 black_pawn_number_of_possible_moves[64];
static i8 knight_number_of_possible_moves[64];
static i8 rook_number_of_possible_moves[64];
static i8 bishop_number_of_possible_moves[64];
static i8 queen_number_of_possible_moves[64];
static i8 king_number_of_possible_moves[64];

/*
 * The square tables are indexed by the square number so even though the code
 * looks like a chess board the top row is actually the rank 1.
 */

static i8 white_pawn_sq_table[64] = {
	 0,  0,   0,   0,   0,   0,  0,  0,
	 5, 10,  10, -20, -20,  10, 10,  5,
	 5, -5, -10,   0,   0, -10, -5,  5,
	 0,  0,   0,  20,  20,   0,  0,  0,
	 5,  5,  10,  25,  25,  10,  5,  5,
	10, 10,  20,  30,  30,  20, 10, 10,
	50, 50,  50,  50,  50,  50, 50, 50,
	 0,  0,   0,   0,   0,   0,  0,  0,
};

static i8 white_knight_sq_table[64] = {
	-50, -40, -30, -30, -30, -30, -40, -50,
	-40, -20,   0,   5,   5,   0, -20, -40,
	-30,   5,  10,  15,  15,  10,   5, -30,
	-30,   0,  15,  20,  20,  15,   0, -30,
	-30,   5,  15,  20,  20,  15,   5, -30,
	-30,   0,  10,  15,  15,  10,   0, -30,
	-40, -20,   0,   0,   0,   0, -20, -40,
	-50, -40, -30, -30, -30, -30, -40, -50,
};

static i8 white_bishop_sq_table[64] = {
	-20, -10, -10, -10, -10, -10, -10, -20,
	-10,   5,   0,   0,   0,   0,   5, -10,
	-10,  10,  10,  10,  10,  10,  10, -10,
	-10,   0,  10,  10,  10,  10,   0, -10,
	-10,   5,   5,  10,  10,   5,   5, -10,
	-10,   0,   5,  10,  10,   5,   0, -10,
	-10,   0,   0,   0,   0,   0,   0, -10,
	-20, -10, -10, -10, -10, -10, -10, -20,
};

static i8 white_rook_sq_table[64] = {
	 0,  0,  0,  5,  5,  0,  0,  0,
	-5,  0,  0,  0,  0,  0,  0, -5,
	-5,  0,  0,  0,  0,  0,  0, -5,
	-5,  0,  0,  0,  0,  0,  0, -5,
	-5,  0,  0,  0,  0,  0,  0, -5,
	-5,  0,  0,  0,  0,  0,  0, -5,
	 5, 10, 10, 10, 10, 10, 10,  5,
	 0,  0,  0,  0,  0,  0,  0,  0,
};

static i8 white_queen_sq_table[64] = {
	-20, -10, -10, -5, -5, -10, -10, -20,
	-10,   0,   5,  0,  0,   0,   0, -10,
	-10,   5,   5,  5,  5,   5,   0, -10,
	  0,   0,   5,  5,  5,   5,   0,  -5,
	 -5,   0,   5,  5,  5,   5,   0,  -5,
	-10,   0,   5,  5,  5,   5,   0, -10,
	-10,   0,   0,  0,  0,   0,   0, -10,
	-20, -10, -10, -5, -5, -10, -10, -20,
};

static i8 white_king_middle_game_sq_table[64] = {
	 20,  30,  10,   0,   0,  10,  30,  20,
	 20,  20,   0,   0,   0,   0,  20,  20,
	-10, -20, -20, -20, -20, -20, -20, -10,
	-20, -30, -30, -40, -40, -30, -30, -20,
	-30, -40, -40, -50, -50, -40, -40, -30,
	-30, -40, -40, -50, -50, -40, -40, -30,
	-30, -40, -40, -50, -50, -40, -40, -30,
	-30, -40, -40, -50, -50, -40, -40, -30,
};

static i8 white_king_end_game_sq_table[64] = {
	-50, -30, -30, -30, -30, -30, -30, -50,
	-30, -30,   0,   0,   0,   0, -30, -30,
	-30, -10,  20,  30,  30,  20, -10, -30,
	-30, -10,  30,  40,  40,  30, -10, -30,
	-30, -10,  30,  40,  40,  30, -10, -30,
	-30, -10,  20,  30,  30,  20, -10, -30,
	-30, -20, -10,   0,   0, -10, -20, -30,
	-50, -40, -30, -20, -20, -30, -40, -50,
};

static i8 black_pawn_sq_table[64];
static i8 black_knight_sq_table[64];
static i8 black_bishop_sq_table[64];
static i8 black_rook_sq_table[64];
static i8 black_queen_sq_table[64];
static i8 black_king_middle_game_sq_table[64];
static i8 black_king_end_game_sq_table[64];

static void init_possible_moves_table(void)
{
	for (Square sq = A1; sq <= H8; ++sq) {
		white_pawn_number_of_possible_moves[sq] = movegen_get_number_of_possible_moves(PIECE_WHITE_PAWN, sq);
		black_pawn_number_of_possible_moves[sq] = movegen_get_number_of_possible_moves(PIECE_BLACK_PAWN, sq);
		knight_number_of_possible_moves[sq] = movegen_get_number_of_possible_moves(PIECE_WHITE_KNIGHT, sq);
		rook_number_of_possible_moves[sq] = movegen_get_number_of_possible_moves(PIECE_WHITE_ROOK, sq);
		bishop_number_of_possible_moves[sq] = movegen_get_number_of_possible_moves(PIECE_WHITE_BISHOP, sq);
		queen_number_of_possible_moves[sq] = movegen_get_number_of_possible_moves(PIECE_WHITE_QUEEN, sq);
		king_number_of_possible_moves[sq] = movegen_get_number_of_possible_moves(PIECE_WHITE_KING, sq);
	}
}

/*
 * The square tables for black pieces has the same values as the ones for white
 * pieces but the board is flipped, so this function uses the square tables of
 * white pieces to initialize the square tables of black pieces.
 */
static void init_square_tables(void)
{
	i8 *const table_pairs[7][2] = {
		{white_pawn_sq_table, black_pawn_sq_table},
		{white_knight_sq_table, black_knight_sq_table},
		{white_bishop_sq_table, black_bishop_sq_table},
		{white_rook_sq_table, black_rook_sq_table},
		{white_queen_sq_table, black_queen_sq_table},
		{white_king_middle_game_sq_table, black_king_middle_game_sq_table},
		{white_king_end_game_sq_table, black_king_end_game_sq_table},
	};

	for (size_t i = 0; i < sizeof(table_pairs) / sizeof(table_pairs[0]); ++i) {
		const i8 *white_table = table_pairs[i][0];
		i8 *black_table = table_pairs[i][1];
		for (size_t rank = RANK_1; rank <= RANK_8; ++rank) {
			for (size_t file = FILE_A; file <= FILE_H; ++file) {
				const Square sq = pos_file_rank_to_square(file, rank);
				const Rank opposite_rank = RANK_8 - rank;
				const Square opposite_sq = pos_file_rank_to_square(file, opposite_rank);
				black_table[opposite_sq] = white_table[sq];
			}
		}
	}
}

static int compute_positioning(const Position *pos)
{
	const Color color = pos_get_side_to_move(pos);
	const i8 *square_tables[] = {
		[PIECE_WHITE_PAWN  ] = white_pawn_sq_table,   [PIECE_BLACK_PAWN  ] = black_pawn_sq_table,
		[PIECE_WHITE_KNIGHT] = white_knight_sq_table, [PIECE_BLACK_KNIGHT] = black_knight_sq_table,
		[PIECE_WHITE_BISHOP] = white_bishop_sq_table, [PIECE_BLACK_BISHOP] = black_bishop_sq_table,
		[PIECE_WHITE_ROOK  ] = white_rook_sq_table,   [PIECE_BLACK_ROOK  ] = black_rook_sq_table,
		[PIECE_WHITE_QUEEN ] = white_queen_sq_table,  [PIECE_BLACK_QUEEN ] = black_queen_sq_table,
	};

	int score = 0;

	for (PieceType piece_type = PIECE_TYPE_PAWN; piece_type <= PIECE_TYPE_QUEEN; ++piece_type) {
		Piece piece = pos_make_piece(piece_type, color);
		const i8 *table = square_tables[piece];
		u64 bb = pos_get_piece_bitboard(pos, piece);
		while (bb) {
			const Square sq = get_index_of_first_bit_and_unset(&bb);
			score += table[sq];
		}

		piece = pos_make_piece(piece_type, !color);
		table = square_tables[piece];
		bb = pos_get_piece_bitboard(pos, piece);
		while (bb) {
			const Square sq = get_index_of_first_bit_and_unset(&bb);
			score -= table[sq];
		}
	}

	const i8 *king_square_tables[] = {
		[PIECE_WHITE_KING] = white_king_middle_game_sq_table,
		[PIECE_BLACK_KING] = black_king_middle_game_sq_table,
	};
	if (pos_get_number_of_pieces_of_color(pos, COLOR_WHITE) < 5)
		king_square_tables[PIECE_WHITE_KING] = white_king_end_game_sq_table;
	if (pos_get_number_of_pieces_of_color(pos, COLOR_BLACK) < 5)
		king_square_tables[PIECE_BLACK_KING] = black_king_end_game_sq_table;

	Square sq = pos_get_king_square(pos, color);
	score += king_square_tables[PIECE_WHITE_KING][sq];
	sq = pos_get_king_square(pos, !color);
	score -= king_square_tables[PIECE_BLACK_KING][sq];

	return score;
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
	const int material_weight = 4;
	const int mobility_weight = 2;
	const int material = compute_material(pos);
	const int mobility = compute_mobility(pos);
	const int positioning = compute_positioning(pos);

	return material_weight * material +
	       mobility_weight * mobility + positioning;
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

	const Square target = move_get_target(move);
	const Square origin = move_get_origin(move);
	const Piece piece = pos_get_piece_at(pos, origin);
	const PieceType piece_type = pos_get_piece_type(piece);
	const Color piece_color = pos_get_side_to_move(pos);

	int score = 0;

	if (move_get_type(move) == MOVE_CAPTURE) {
		const Piece attacked_piece = pos_get_piece_at(pos, target);
		const Piece attacker_piece = pos_get_piece_at(pos, origin);
		const PieceType attacked = pos_get_piece_type(attacked_piece);
		const PieceType attacker = pos_get_piece_type(attacker_piece);

		score += target_table[attacked] + attacker_table[attacker];
	}

	/* Temporarily remove moving piece so it's not counted as an attacking
	 * piece. */
	pos_remove_piece(pos, origin);
	if (movegen_is_square_attacked(target, !piece_color, pos))
		score -= target_table[piece_type];
	else
		score += 1;
	pos_place_piece(pos, origin, piece);
	if (movegen_is_square_attacked(origin, !piece_color, pos))
		score += target_table[piece_type];

	static const i8 *number_of_possible_moves[7] = {
		[PIECE_TYPE_KNIGHT] = knight_number_of_possible_moves,
		[PIECE_TYPE_ROOK  ] = rook_number_of_possible_moves,
		[PIECE_TYPE_BISHOP] = bishop_number_of_possible_moves,
		[PIECE_TYPE_QUEEN ] = queen_number_of_possible_moves,
		[PIECE_TYPE_KING  ] = king_number_of_possible_moves,
	};
	if (piece_type == PIECE_TYPE_PAWN)
		score += piece_color == COLOR_WHITE ? pos_get_rank_of_square(target) : (RANK_7 - pos_get_rank_of_square(target));
	else
		score += number_of_possible_moves[piece_type][target];

	const i8 *square_tables[] = {
		[PIECE_WHITE_PAWN  ] = white_pawn_sq_table,   [PIECE_BLACK_PAWN  ] = black_pawn_sq_table,
		[PIECE_WHITE_KNIGHT] = white_knight_sq_table, [PIECE_BLACK_KNIGHT] = black_knight_sq_table,
		[PIECE_WHITE_BISHOP] = white_bishop_sq_table, [PIECE_BLACK_BISHOP] = black_bishop_sq_table,
		[PIECE_WHITE_ROOK  ] = white_rook_sq_table,   [PIECE_BLACK_ROOK  ] = black_rook_sq_table,
		[PIECE_WHITE_QUEEN ] = white_queen_sq_table,  [PIECE_BLACK_QUEEN ] = black_queen_sq_table,
	};

	if (piece_type == PIECE_TYPE_KING) {
		const i8 *king_square_tables[] = {
			[PIECE_WHITE_KING] = white_king_middle_game_sq_table,
			[PIECE_BLACK_KING] = black_king_middle_game_sq_table,
		};
		if (pos_get_number_of_pieces_of_color(pos, COLOR_WHITE) < 5)
			king_square_tables[PIECE_WHITE_KING] = white_king_end_game_sq_table;
		if (pos_get_number_of_pieces_of_color(pos, COLOR_BLACK) < 5)
			king_square_tables[PIECE_BLACK_KING] = black_king_end_game_sq_table;
		score += king_square_tables[piece][target];
		score -= king_square_tables[piece][origin];
	}
	if (piece_type != PIECE_TYPE_KING) {
		score += square_tables[piece][target];
		score -= square_tables[piece][origin];
	}

	return score;
}

void eval_init(void)
{
	init_possible_moves_table();
	init_square_tables();
}
