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
	PIECE_VALUE_KNIGHT = 300,
	PIECE_VALUE_BISHOP = 350,
	PIECE_VALUE_ROOK = 500,
	PIECE_VALUE_QUEEN = 1000,
	PIECE_VALUE_KING = 10000,
};

/*
 * These tables store the number of possible moves for a piece when the board
 * contains only that piece, so no occupancy for sliding pieces.
 */
static int white_pawn_number_of_possible_moves[64];
static int black_pawn_number_of_possible_moves[64];
static int knight_number_of_possible_moves[64];
static int rook_number_of_possible_moves[64];
static int bishop_number_of_possible_moves[64];
static int queen_number_of_possible_moves[64];
static int king_number_of_possible_moves[64];

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
		score += 2 * target_table[piece_type];

	static const int *number_of_possible_moves[7] = {
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

	return score;
}

void eval_init(void)
{
	init_possible_moves_table();
}
