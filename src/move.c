#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

#include "bit.h"
#include "pos.h"
#include "move.h"
#include "movegen.h"

static const Piece promotion_table[][4] = {
	[COLOR_WHITE][MOVE_KNIGHT_PROMOTION - 6] = PIECE_WHITE_KNIGHT,
	[COLOR_WHITE][MOVE_ROOK_PROMOTION   - 6] = PIECE_WHITE_ROOK,
	[COLOR_WHITE][MOVE_BISHOP_PROMOTION - 6] = PIECE_WHITE_BISHOP,
	[COLOR_WHITE][MOVE_QUEEN_PROMOTION  - 6] = PIECE_WHITE_QUEEN,
	[COLOR_BLACK][MOVE_KNIGHT_PROMOTION - 6] = PIECE_BLACK_KNIGHT,
	[COLOR_BLACK][MOVE_ROOK_PROMOTION   - 6] = PIECE_BLACK_ROOK,
	[COLOR_BLACK][MOVE_BISHOP_PROMOTION - 6] = PIECE_BLACK_BISHOP,
	[COLOR_BLACK][MOVE_QUEEN_PROMOTION  - 6] = PIECE_BLACK_QUEEN,
};
static const CastlingSide castling_table[] = {
	[MOVE_KING_CASTLE]  = CASTLING_SIDE_KING,
	[MOVE_QUEEN_CASTLE] = CASTLING_SIDE_QUEEN,
};

/*
 * Since the functions to do and undo moves do basically the same thing, I
 * created this macro. The conditionals that check the choice of doing or
 * undoing are optimized out.
 */
#define ACTION_FOR_MOVE(do_or_undo)\
const int choice_do = 0;\
const int choice_undo = !choice_do;\
const MoveType type = move_get_type(move);\
const Square from = move_get_origin(move);\
const Square to = move_get_target(move);\
const Piece piece = choice_##do_or_undo == choice_do ?\
pos_get_piece_at(pos, from) : pos_get_piece_at(pos, to);\
const Color color = pos_get_piece_color(piece);\
\
if (choice_##do_or_undo == choice_do)\
	pos_start_new_irreversible_state(pos);\
\
if (type == MOVE_QUIET)\
	do_or_undo##_quiet(pos, from, to, piece);\
else if (type == MOVE_DOUBLE_PAWN_PUSH)\
	do_or_undo##_double_push(pos, from, to, piece);\
else if (type == MOVE_QUEEN_CASTLE || type == MOVE_KING_CASTLE)\
	do_or_undo##_castling(pos, from, to, piece, castling_table[type]);\
else if (type == MOVE_CAPTURE)\
	do_or_undo##_capture(pos, from, to, piece);\
else if (type == MOVE_EP_CAPTURE)\
	do_or_undo##_ep_capture(pos, from, to, piece);\
else if (type >= MOVE_KNIGHT_PROMOTION && type <= MOVE_QUEEN_PROMOTION)\
	do_or_undo##_promotion(pos, from, to, promotion_table[color][type - 6], 0);\
else if (type >= MOVE_KNIGHT_PROMOTION_CAPTURE)\
	do_or_undo##_promotion(pos, from, to, promotion_table[color][type - 10], 1);\
\
if (choice_##do_or_undo == choice_undo)\
	pos_backtrack_irreversible_state(pos);\
pos_flip_side_to_move(pos);

static void do_promotion(Position *pos, Square from, Square to,
			 Piece promoted_to, int is_capture)
{
	const Color c = pos_get_piece_color(promoted_to);

	if (is_capture) {
		const Piece captured_piece = pos_get_piece_at(pos, to);
		if (captured_piece == PIECE_WHITE_ROOK && to == A1)
			pos_remove_castling(pos, COLOR_WHITE, CASTLING_SIDE_QUEEN);
		else if (captured_piece == PIECE_WHITE_ROOK && to == H1)
			pos_remove_castling(pos, COLOR_WHITE, CASTLING_SIDE_KING);
		else if (captured_piece == PIECE_BLACK_ROOK && to == A8)
			pos_remove_castling(pos, COLOR_BLACK, CASTLING_SIDE_QUEEN);
		else if (captured_piece == PIECE_BLACK_ROOK && to == H8)
			pos_remove_castling(pos, COLOR_BLACK, CASTLING_SIDE_KING);
		pos_reset_halfmove_clock(pos);
		pos_set_captured_piece(pos, pos_get_piece_at(pos, to));
		pos_remove_piece(pos, to);
	}
	pos_unset_enpassant(pos);
	pos_increment_halfmove_clock(pos);
	pos_remove_piece(pos, from);
	pos_place_piece(pos, to, promoted_to);

	if (c == COLOR_BLACK)
		pos_increment_fullmove_counter(pos);
}

static void undo_promotion(Position *pos, Square from, Square to,
			   Piece promoted_to, int is_capture)
{
	const Color c = pos_get_piece_color(promoted_to);
	const Piece pawn = c == COLOR_WHITE ? PIECE_WHITE_PAWN :
	                                      PIECE_BLACK_PAWN;

	pos_remove_piece(pos, to);
	pos_place_piece(pos, from, pawn);
	if (is_capture)
		pos_place_piece(pos, to, pos_get_captured_piece(pos));
}

static void do_castling(Position *pos, Square from, Square to, Piece piece,
			CastlingSide side)
{
	const Color c = pos_get_piece_color(piece);
	const Piece rook = c == COLOR_WHITE ? PIECE_WHITE_ROOK :
	                                      PIECE_BLACK_ROOK;
	Square rook_from, rook_to;

	if (side == CASTLING_SIDE_KING) {
		rook_from = c == COLOR_WHITE ? H1 : H8;
		rook_to = c == COLOR_WHITE ? F1 : F8;
	} else {
		rook_from = c == COLOR_WHITE ? A1 : A8;
		rook_to = c == COLOR_WHITE ? D1 : D8;
	}

	pos_unset_enpassant(pos);
	pos_increment_halfmove_clock(pos);
	pos_remove_piece(pos, rook_from);
	pos_place_piece(pos, rook_to, rook);
	pos_remove_piece(pos, from);
	pos_place_piece(pos, to, piece);
	pos_remove_castling(pos, c, CASTLING_SIDE_KING);
	pos_remove_castling(pos, c, CASTLING_SIDE_QUEEN);

	if (c == COLOR_BLACK)
		pos_increment_fullmove_counter(pos);
}

static void undo_castling(Position *pos, Square from, Square to, Piece piece,
			  CastlingSide side)
{
	const Color c = pos_get_piece_color(piece);
	const Piece rook = c == COLOR_WHITE ? PIECE_WHITE_ROOK :
	                                      PIECE_BLACK_ROOK;
	Square rook_from, rook_to;

	if (side == CASTLING_SIDE_KING) {
		rook_from = c == COLOR_WHITE ? H1 : H8;
		rook_to = c == COLOR_WHITE ? F1 : F8;
	} else {
		rook_from = c == COLOR_WHITE ? A1 : A8;
		rook_to = c == COLOR_WHITE ? D1 : D8;
	}

	pos_remove_piece(pos, rook_to);
	pos_place_piece(pos, rook_from, rook);
	pos_remove_piece(pos, to);
	pos_place_piece(pos, from, piece);

	if (c == COLOR_BLACK)
		pos_decrement_fullmove_counter(pos);
}

static void do_ep_capture(Position *pos, Square from, Square to, Piece piece)
{
	const Color c = pos_get_piece_color(piece);
	const Square pawn_sq = c == COLOR_WHITE ? to - 8 : to + 8;
	const Piece pawn = c == COLOR_WHITE ? PIECE_BLACK_PAWN :
	                                      PIECE_WHITE_PAWN;

	pos_unset_enpassant(pos);
	pos_set_captured_piece(pos, pawn);
	pos_remove_piece(pos, pawn_sq);
	pos_remove_piece(pos, from);
	pos_place_piece(pos, to, piece);
	pos_reset_halfmove_clock(pos);

	if (c == COLOR_BLACK)
		pos_increment_fullmove_counter(pos);
}

static void undo_ep_capture(Position *pos, Square from, Square to, Piece piece)
{
	const Color c = pos_get_piece_color(piece);
	const Square pawn_sq = c == COLOR_WHITE ? to - 8 : to + 8;
	const Piece pawn = c == COLOR_WHITE ? PIECE_BLACK_PAWN :
	                                      PIECE_WHITE_PAWN;

	pos_remove_piece(pos, to);
	pos_place_piece(pos, from, piece);
	pos_place_piece(pos, pawn_sq, pawn);

	if (c == COLOR_BLACK)
		pos_decrement_fullmove_counter(pos);
}

static void do_capture(Position *pos, Square from, Square to, Piece piece)
{
	const PieceType piece_type = pos_get_piece_type(piece);
	const Color piece_color = pos_get_piece_color(piece);
	const Piece captured_piece = pos_get_piece_at(pos, to);

	if (captured_piece == PIECE_WHITE_KING || captured_piece == PIECE_BLACK_KING) {
		puts("KING CAPTURED");
		printf("from = %d\n", from);
		printf("to = %d\n", to);
		abort();
	}
	pos_unset_enpassant(pos);
	pos_set_captured_piece(pos, captured_piece);
	pos_remove_piece(pos, to);
	pos_remove_piece(pos, from);
	pos_place_piece(pos, to, piece);
	pos_reset_halfmove_clock(pos);

	switch (piece_type) {
	case PIECE_TYPE_KING:
		pos_remove_castling(pos, piece_color, CASTLING_SIDE_KING);
		pos_remove_castling(pos, piece_color, CASTLING_SIDE_QUEEN);
		break;
	case PIECE_TYPE_ROOK:
		if (piece_color == COLOR_WHITE && from == A1)
			pos_remove_castling(pos, piece_color, CASTLING_SIDE_QUEEN);
		else if (piece_color == COLOR_WHITE && from == H1)
			pos_remove_castling(pos, piece_color, CASTLING_SIDE_KING);
		else if (piece_color == COLOR_BLACK && from == A8)
			pos_remove_castling(pos, piece_color, CASTLING_SIDE_QUEEN);
		else if (piece_color == COLOR_BLACK && from == H8)
			pos_remove_castling(pos, piece_color, CASTLING_SIDE_KING);
		break;
	default:
		break;
	}

	if (pos_get_piece_type(captured_piece) == PIECE_TYPE_ROOK) {
		if (captured_piece == PIECE_WHITE_ROOK && to == A1)
			pos_remove_castling(pos, COLOR_WHITE, CASTLING_SIDE_QUEEN);
		else if (captured_piece == PIECE_WHITE_ROOK && to == H1)
			pos_remove_castling(pos, COLOR_WHITE, CASTLING_SIDE_KING);
		else if (captured_piece == PIECE_BLACK_ROOK && to == A8)
			pos_remove_castling(pos, COLOR_BLACK, CASTLING_SIDE_QUEEN);
		else if (captured_piece == PIECE_BLACK_ROOK && to == H8)
			pos_remove_castling(pos, COLOR_BLACK, CASTLING_SIDE_KING);
	}

	if (piece_color == COLOR_BLACK)
		pos_increment_fullmove_counter(pos);
}

static void undo_capture(Position *pos, Square from, Square to, Piece piece)
{
	const Color piece_color = pos_get_piece_color(piece);
	const Piece captured_piece = pos_get_captured_piece(pos);

	pos_remove_piece(pos, to);
	pos_place_piece(pos, from, piece);
	pos_place_piece(pos, to, captured_piece);

	if (piece_color == COLOR_BLACK)
		pos_decrement_fullmove_counter(pos);
}

static void do_double_push(Position *pos, Square from, Square to, Piece piece)
{
	const Color c = pos_get_piece_color(piece);

	pos_increment_halfmove_clock(pos);
	pos_remove_piece(pos, from);
	pos_place_piece(pos, to, piece);
	pos_set_enpassant(pos, pos_get_file_of_square(from));
	pos_reset_halfmove_clock(pos);

	if (c == COLOR_BLACK)
		pos_increment_fullmove_counter(pos);
}

static void undo_double_push(Position *pos, Square from, Square to, Piece piece)
{
	const Color c = pos_get_piece_color(piece);

	pos_remove_piece(pos, to);
	pos_place_piece(pos, from, piece);
	pos_unset_enpassant(pos);

	if (c == COLOR_BLACK)
		pos_decrement_fullmove_counter(pos);
}

static void do_quiet(Position *pos, Square from, Square to, Piece piece)
{
	const PieceType pt = pos_get_piece_type(piece);
	const Color c = pos_get_piece_color(piece);

	pos_unset_enpassant(pos);
	pos_increment_halfmove_clock(pos);
	pos_remove_piece(pos, from);
	pos_place_piece(pos, to, piece);

	switch (pt) {
	case PIECE_TYPE_PAWN:
		pos_reset_halfmove_clock(pos);
		break;
	case PIECE_TYPE_KING:
		pos_remove_castling(pos, c, CASTLING_SIDE_KING);
		pos_remove_castling(pos, c, CASTLING_SIDE_QUEEN);
		break;
	case PIECE_TYPE_ROOK:
		if (c == COLOR_WHITE && from == A1)
			pos_remove_castling(pos, c, CASTLING_SIDE_QUEEN);
		else if (c == COLOR_WHITE && from == H1)
			pos_remove_castling(pos, c, CASTLING_SIDE_KING);
		else if (c == COLOR_BLACK && from == A8)
			pos_remove_castling(pos, c, CASTLING_SIDE_QUEEN);
		else if (c == COLOR_BLACK && from == H8)
			pos_remove_castling(pos, c, CASTLING_SIDE_KING);
		break;
	default:
		break;
	}

	if (c == COLOR_BLACK)
		pos_increment_fullmove_counter(pos);
}

static void undo_quiet(Position *pos, Square from, Square to, Piece piece)
{
	const Color c = pos_get_piece_color(piece);

	pos_remove_piece(pos, to);
	pos_place_piece(pos, from, piece);

	if (c == COLOR_BLACK)
		pos_decrement_fullmove_counter(pos);
}

/*
 * This function returns 1 is a move is legal and 0 otherwise. Although the
 * pos argument is non-const the original position is restored so it is safe to
 * call it. A const argument would require copying the position to make changes
 * in the copy and that would be slower.
 */
bool move_is_legal(Position *pos, Move move)
{
	const Color color = pos_get_side_to_move(pos);
	move_do(pos, move);
	const Square sq = pos_get_king_square(pos, color);
	if (movegen_is_square_attacked(sq, !color, pos)) {
		move_undo(pos, move);
		return false;
	}
	move_undo(pos, move);
	return true;
}

void move_undo(Position *pos, Move move)
{
	ACTION_FOR_MOVE(undo);
}

void move_do(Position *pos, Move move)
{
	ACTION_FOR_MOVE(do);
}

Move move_new(Square from, Square to, MoveType type)
{
	return (type & 0xf) << 12 | (to & 0x3f) << 6 | (from & 0x3f);
}

bool move_is_capture(Move move)
{
	const MoveType type = move_get_type(move);

	return type == MOVE_CAPTURE ||
	       type == MOVE_EP_CAPTURE ||
	       type == MOVE_KNIGHT_PROMOTION_CAPTURE ||
	       type == MOVE_ROOK_PROMOTION_CAPTURE ||
	       type == MOVE_BISHOP_PROMOTION_CAPTURE ||
	       type == MOVE_QUEEN_PROMOTION_CAPTURE;
}

Square move_get_origin(Move move)
{
	return move & 0x3f;
}

Square move_get_target(Move move)
{
	return move >> 6 & 0x3f;
}

MoveType move_get_type(Move move)
{
	return move >> 12 & 0xf;
}
