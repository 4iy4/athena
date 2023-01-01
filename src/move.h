#ifndef MOVE_H
#define MOVE_H

/*
 * The moves are encoded in 16 bits in the following form:
 *
 *  0000 000000 000000
 * |____|______|______|
 *   |    |      |
 *   type to   from
 * 
 * In en passant captures the "to" square is the square the attacking piece
 * will move to, and in castling moves it's the square the king will move to.
 */

typedef enum move_type {
	MOVE_QUIET,
	MOVE_DOUBLE_PAWN_PUSH,
	MOVE_KING_CASTLE,
	MOVE_QUEEN_CASTLE,
	MOVE_CAPTURE,
	MOVE_EP_CAPTURE,
	MOVE_KNIGHT_PROMOTION,
	MOVE_ROOK_PROMOTION,
	MOVE_BISHOP_PROMOTION,
	MOVE_QUEEN_PROMOTION,
	MOVE_KNIGHT_PROMOTION_CAPTURE,
	MOVE_ROOK_PROMOTION_CAPTURE,
	MOVE_BISHOP_PROMOTION_CAPTURE,
	MOVE_QUEEN_PROMOTION_CAPTURE,
} MoveType;

typedef u16 Move;

bool move_is_legal(Position *pos, Move move);
void move_undo(Position *pos, Move move);
void move_do(Position *pos, Move move);
Move move_new(Square from, Square to, MoveType type);
bool move_is_capture(Move move);
Square move_get_origin(Move move);
Square move_get_target(Move move);
MoveType move_get_type(Move move);

#endif
