#ifndef POSITION_H
#define POSITION_H

typedef enum direction {
	NORTH,
	NORTHEAST,
	EAST,
	SOUTHEAST,
	SOUTH,
	SOUTHWEST,
	WEST,
	NORTHWEST,
} Direction;

typedef enum file {
	FILE_A, FILE_B, FILE_C, FILE_D,
	FILE_E, FILE_F, FILE_G, FILE_H,
} File;

typedef enum rank {
	RANK_1, RANK_2, RANK_3, RANK_4,
	RANK_5, RANK_6, RANK_7, RANK_8,
} Rank;

typedef enum square {
	A1, B1, C1, D1, E1, F1, G1, H1,
	A2, B2, C2, D2, E2, F2, G2, H2,
	A3, B3, C3, D3, E3, F3, G3, H3,
	A4, B4, C4, D4, E4, F4, G4, H4,
	A5, B5, C5, D5, E5, F5, G5, H5,
	A6, B6, C6, D6, E6, F6, G6, H6,
	A7, B7, C7, D7, E7, F7, G7, H7,
	A8, B8, C8, D8, E8, F8, G8, H8,
} Square;

/*
 * It's safe to get the opposite color with the ! operator on the color.
 */
typedef enum color {
	COLOR_WHITE,
	COLOR_BLACK,
} Color;

typedef enum piece_type {
	PIECE_TYPE_PAWN,
	PIECE_TYPE_KNIGHT,
	PIECE_TYPE_ROOK,
	PIECE_TYPE_BISHOP,
	PIECE_TYPE_QUEEN,
	PIECE_TYPE_KING,
} PieceType;

typedef enum piece {
	PIECE_WHITE_PAWN   = COLOR_WHITE | PIECE_TYPE_PAWN   << 1,
	PIECE_WHITE_KNIGHT = COLOR_WHITE | PIECE_TYPE_KNIGHT << 1,
	PIECE_WHITE_ROOK   = COLOR_WHITE | PIECE_TYPE_ROOK   << 1,
	PIECE_WHITE_BISHOP = COLOR_WHITE | PIECE_TYPE_BISHOP << 1,
	PIECE_WHITE_QUEEN  = COLOR_WHITE | PIECE_TYPE_QUEEN  << 1,
	PIECE_WHITE_KING   = COLOR_WHITE | PIECE_TYPE_KING   << 1,

	PIECE_BLACK_PAWN   = COLOR_BLACK | PIECE_TYPE_PAWN   << 1,
	PIECE_BLACK_KNIGHT = COLOR_BLACK | PIECE_TYPE_KNIGHT << 1,
	PIECE_BLACK_ROOK   = COLOR_BLACK | PIECE_TYPE_ROOK   << 1,
	PIECE_BLACK_BISHOP = COLOR_BLACK | PIECE_TYPE_BISHOP << 1,
	PIECE_BLACK_QUEEN  = COLOR_BLACK | PIECE_TYPE_QUEEN  << 1,
	PIECE_BLACK_KING   = COLOR_BLACK | PIECE_TYPE_KING   << 1,

	PIECE_NONE = 0xff /* Only used for the array board. */
} Piece;

typedef enum castling_side {
	CASTLING_SIDE_QUEEN,
	CASTLING_SIDE_KING,
} CastlingSide;

struct position;
typedef struct position Position;

void pos_print(const Position *pos);
void pos_decrement_fullmove_counter(Position *pos);
void pos_increment_fullmove_counter(Position *pos);
void pos_remove_castling(Position *pos, Color c, CastlingSide side);
void pos_add_castling(Position *pos, Color c, CastlingSide side);
void pos_flip_side_to_move(Position *pos);
void pos_set_captured_piece(Position *pos, Piece piece);
void pos_remove_piece(Position *pos, Square sq);
void pos_place_piece(Position *pos, Square sq, Piece piece);
void pos_reset_halfmove_clock(Position *pos);
void pos_increment_halfmove_clock(Position *pos);
void pos_unset_enpassant(Position *pos);
void pos_set_enpassant(Position *pos, File file);
Piece pos_get_captured_piece(const Position *pos);
int pos_has_castling_right(const Position *pos, Color c, CastlingSide side);
int pos_get_fullmove_counter(const Position *pos);
int pos_get_halfmove_clock(const Position *pos);
int pos_enpassant_possible(const Position *pos);
Square pos_get_enpassant(const Position *pos);
Color pos_get_side_to_move(const Position *pos);
Square pos_get_king_square(const Position *pos, Color c);
Piece pos_get_piece_at(const Position *pos, Square sq);
int pos_get_number_of_pieces(const Position *pos, Piece piece);
int pos_get_number_of_pieces_of_color(const Position *pos, Color c);
u64 pos_get_piece_bitboard(const Position *pos, Piece piece);
u64 pos_get_color_bitboard(const Position *pos, Color c);
void pos_backtrack_irreversible_state(Position *pos);
void pos_start_new_irreversible_state(Position *pos);
Position *pos_copy(const Position *pos);
Position *pos_create(const char *fen);
void pos_destroy(Position *pos);
Square pos_file_rank_to_square(File f, Rank r);
File pos_get_file_of_square(Square sq);
Rank pos_get_rank_of_square(Square sq);
Color pos_get_piece_color(Piece piece);
PieceType pos_get_piece_type(Piece piece);
Piece pos_make_piece(PieceType pt, Color c);

#endif
