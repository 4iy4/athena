#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include "bit.h"
#include "pos.h"

/*
 * The piece placement is stored in two formats, in piece-centric bitboard
 * arrays and in a square-centric array.
 * 
 * In the piece-centric format there are two arrays, one indexed by the color
 * and one indexed by the piece type, both storing bitboards where the set bits
 * represent a piece of that color or type at a square, where the square is
 * counted from the least significant bit to the most significant bit, from 0
 * to 63. The bitboards store pieces using a Little-Endian Rank-File mapping
 * (LERF), which means each byte, from the least significant byte to the most
 * significant byte represent a rank, and each bit of these bytes represent a
 * square on that rank. Which means that A1 is square 0, H1 is square 7, A2 is
 * square 8, H2 is square 15 and so on.
 * 
 * The square-centric format is just a flat array indexed by the square number
 * in a LERF mapping and each element of the array is a piece, or PIECE_NONE if
 * the square is empty.
 * 
 * The castling rights are stored in a nibble, the 2 least significant bits are
 * for white and the next 2 bits for black, the least significant and most
 * significant bits of each are for the queen and king sides respectively. A
 * set bit means that the king has the right to castle. Notice that having
 * right to castle does not mean that castling really is possible
 * (castling ability), it only means that neither the king nor the rook
 * corresponding to the castling side have moved throughout the game, and the
 * rook hasn't been captured. In order for castling to be possible, the
 * following conditions must be met:
 * - The side must have castling rights;
 * - There are no pieces between the king and rook;
 * - The king is not in check;
 * - The king will not be in check after castling;
 * - The squares between the king's original and final positions are not being
 *   attacked.
 * 
 * The en passant square is not stored, but instead only its file. This is to
 * save space, since it is possible to recover the square by using the color of
 * the side to move. Because there are only 8 files the file is stored in just
 * the 3 least significant bits of a nibble and the most significant bit of the
 * nibble is set when there is an en passant square and unset otherwise. Since
 * both the castling rights and en passant file are stored in a nibble, I
 * stored both together in one byte.
 * 
 * Because changes to some of the position data can't be undone, like the
 * castling ability (if the rook moves back and forth there's no way to know
 * that it happened later on), all this irreversibe state is stored in a stack
 * where the top is the current state and to undo a move one only has to pop
 * the last irreversible state off the stack and undo the changes to the
 * reversibe data.
 */

struct irreversible_state {
	u8 castling_rights_and_enpassant;
	u8 halfmove_clock;
	u8 captured_piece;
	struct irreversible_state *previous;
};

struct position {
	struct irreversible_state *irreversible;
	u8 side_to_move;
	short fullmove_counter;
	u64 color_bb[2];
	u64 type_bb[6];
	Piece board[64];
};

/*
 * Check if ch is one of the characters in str, where str is a string containing
 * all characters to be checked and not separated by space.
 * For example:
 * is_one_of("ar4h", 'r')
 * returns 1, but
 * is_one_of("r57g", '9')
 * returns 0.
 */
static int is_one_of(const char *str, char ch)
{
	if (!ch || !strchr(str, ch))
		return 0;
	return 1;
}

static size_t parse_pieces(Position *pos, const char *str)
{
	const Piece table[] = {
		['P'] = PIECE_WHITE_PAWN,   ['p'] = PIECE_BLACK_PAWN,
		['N'] = PIECE_WHITE_KNIGHT, ['n'] = PIECE_BLACK_KNIGHT,
		['R'] = PIECE_WHITE_ROOK,   ['r'] = PIECE_BLACK_ROOK,
		['B'] = PIECE_WHITE_BISHOP, ['b'] = PIECE_BLACK_BISHOP,
		['Q'] = PIECE_WHITE_QUEEN,  ['q'] = PIECE_BLACK_QUEEN,
		['K'] = PIECE_WHITE_KING,   ['k'] = PIECE_BLACK_KING,
	};

	size_t rc = 0;
	File file = FILE_A;
	Rank rank = RANK_8;
	for (size_t i = 0; file <= FILE_H || rank > RANK_1; ++i) {
		if (!str[i])
			return 0;
		char ch = str[i];
		++rc;

		if (file > FILE_H) {
			if (str[i] && str[i] != '/')
				return 0;
			--rank;
			file = FILE_A;
			continue;
		}

		if (isdigit(ch)) {
			int digit = ch - '0';
			if (digit > 8 || digit < 1 || !digit || file + digit > 8)
				return 0;
			file += digit;
		} else if (is_one_of("PNRBQKpnrbqk", ch)) {
			Piece piece = table[(size_t)ch];
			Square sq = pos_file_rank_to_square(file, rank);
			pos_place_piece(pos, sq, piece);
			++file;
		} else {
			return 0;
		}
	}

	return rc;
}

static size_t parse_side(Position *pos, const char *str)
{
	switch (str[0]) {
	case 'w':
		pos->side_to_move = COLOR_WHITE;
		break;
	case 'b':
		pos->side_to_move = COLOR_BLACK;
		break;
	default:
		return 0;
	}
	return 1;
}

static size_t parse_castling(Position *pos, const char *str)
{
	if (str[0] == '-')
		return 1;
	int Kcnt = 0, Qcnt = 0, kcnt = 0, qcnt = 0;
	size_t rc = 0;
	for (; str[rc] && str[rc] != ' '; ++rc) {
		switch (str[rc]) {
		case 'K':
			pos_add_castling(pos, COLOR_WHITE,  CASTLING_SIDE_KING);
			++Kcnt;
			break;
		case 'Q':
			pos_add_castling(pos, COLOR_WHITE, CASTLING_SIDE_QUEEN);
			++Qcnt;
			break;
		case 'k':
			pos_add_castling(pos, COLOR_BLACK, CASTLING_SIDE_KING);
			++kcnt;
			break;
		case 'q':
			pos_add_castling(pos, COLOR_BLACK,  CASTLING_SIDE_QUEEN);
			++qcnt;
			break;
		default:
			return 0;
		}
		if (Kcnt > 1 || Qcnt > 1 || kcnt > 1 || qcnt > 1)
			return 0;
	}
	return rc;
}

static size_t parse_enpassant(Position *pos, const char *str)
{
	if (str[0] == '-')
		return 1;
	if (!str[0] || !str[1])
		return 0;
	else if (str[0] < 'a' || str[0] > 'h' ||
		 (str[1] != '3' && str[1] != '6'))
		return 0;
	File file = str[0] - 'a';
	pos_set_enpassant(pos, file);
	return 2;
}

static size_t parse_halfmove_clock(Position *pos, const char *str)
{
	char *endptr = NULL;
	errno = 0;
	unsigned long clock = strtoul(str, &endptr, 10);
	if (errno == ERANGE)
		return 0;
	else if (endptr == str)
		return 0;
	else if (clock > SHRT_MAX)
		return 0;
	pos->irreversible->halfmove_clock = (u8)clock;
	return endptr - str;
}

/*
 * Both the parse_fullmove_counter and parse_halfmove_clock functions parse the
 * number up to the first invalid character (a character that is not part of a
 * number). So the functions will not fail because of the space character after
 * the number in the FEN string.
 */
static size_t parse_fullmove_counter(Position *pos, const char *str)
{
	char *endptr = NULL;
	errno = 0;
	unsigned long counter = strtoul(str, &endptr, 10);
	if (errno == ERANGE)
		return 0;
	else if (endptr == str)
		return 0;
	else if (counter > SHRT_MAX)
		return 0;
	pos->fullmove_counter = (short)counter;
	return endptr - str;
}

/*
 * Modifies a position by parsing a FEN string and returns the number of
 * characters read, if it's less than the length then an error ocurred. Each of
 * the parse_* functions return the number of characters that were read, and if
 * the sequence of characters is invalid they return 0.
 */
static size_t parse_fen(Position *pos, const char *fen)
{
	size_t (*const steps[])(Position *, const char *) = {
		parse_pieces,
		parse_side,
		parse_castling,
		parse_enpassant,
		parse_halfmove_clock,
		parse_fullmove_counter,
	};
	const size_t num_steps = sizeof(steps) / sizeof(steps[0]);

	size_t rc = 0, ret = 0;
	for (size_t i = 0; i < num_steps; ++i) {
		ret = steps[i](pos, fen);
		if (!ret)
			return 0;
		fen += ret;
		rc += ret;
		if (i < num_steps - 1) {
			if (fen[0] != ' ')
				return 0;
			else
				++rc;
		}
		++fen;
	}

	return rc;
}

void pos_print(const Position *pos)
{
	const char piece_table[] = {
		[PIECE_TYPE_PAWN ] = 'p', [PIECE_TYPE_KNIGHT] = 'n',
		[PIECE_TYPE_ROOK ] = 'r', [PIECE_TYPE_BISHOP] = 'b',
		[PIECE_TYPE_QUEEN] = 'q', [PIECE_TYPE_KING  ] = 'k',
	};

	Rank rank = RANK_8;
	File file = FILE_A;
	for (rank = RANK_8, file = FILE_A; file <= FILE_H || rank > RANK_1;) {
		if (file > FILE_H) {
			--rank;
			file = FILE_A;
			putchar('\n');
		}

		Square sq = pos_file_rank_to_square(file, rank);
		Piece piece = pos_get_piece_at(pos, sq);
		char ch = '\0';
		if (piece == PIECE_NONE) {
			ch = '0';
		} else {
			PieceType piece_type = pos_get_piece_type(piece);
			Color color = pos_get_piece_color(piece);
			ch = piece_table[piece_type];
			if (color == COLOR_WHITE)
				ch = toupper(ch);
		}
		printf("%c ", ch);
		++file;
	}
	printf("\n\n");

	Color color = pos_get_side_to_move(pos);
	if (color == COLOR_WHITE)
		printf("Turn: white\n");
	else
		printf("Turn: black\n");

	printf("En passant: ");
	if (pos_enpassant_possible(pos)) {
		Square sq = pos_get_enpassant(pos);
		file = pos_get_file_of_square(sq);
		rank = pos_get_rank_of_square(sq);
		printf("%c%d\n", file + 'A', rank + 1);
	} else {
		printf("-\n");
	}

	printf("Castling rights: ");
	if (pos_has_castling_right(pos, COLOR_WHITE, CASTLING_SIDE_KING))
		putchar('K');
	if (pos_has_castling_right(pos, COLOR_WHITE, CASTLING_SIDE_QUEEN))
		putchar('Q');
	if (pos_has_castling_right(pos, COLOR_BLACK, CASTLING_SIDE_KING))
		putchar('k');
	if (pos_has_castling_right(pos, COLOR_BLACK, CASTLING_SIDE_QUEEN))
		putchar('q');
	printf("\n");

	printf("Halfmove clock: %d\n", pos_get_halfmove_clock(pos));
	printf("Fullmove counter: %d\n", pos_get_fullmove_counter(pos));
}

void pos_decrement_fullmove_counter(Position *pos)
{
	--pos->fullmove_counter;
}

void pos_increment_fullmove_counter(Position *pos)
{
	++pos->fullmove_counter;
}

void pos_remove_castling(Position *pos, Color c, CastlingSide side)
{
	pos->irreversible->castling_rights_and_enpassant &= ~(1 << side <<
	                                                      2 * c);
}

void pos_add_castling(Position *pos, Color c, CastlingSide side)
{
	pos->irreversible->castling_rights_and_enpassant |= 1 << side <<
	                                                    2 * c;
}

void pos_flip_side_to_move(Position *pos)
{
	if (pos->side_to_move == COLOR_WHITE)
		pos->side_to_move = COLOR_BLACK;
	else
		pos->side_to_move = COLOR_WHITE;
}

void pos_set_captured_piece(Position *pos, Piece piece)
{
	pos->irreversible->captured_piece = piece;
}

/*
 * Remove a piece from a square.
 */
void pos_remove_piece(Position *pos, Square sq)
{
	const Piece piece = pos_get_piece_at(pos, sq);
	const u64 bb = U64(0x1) << sq;
	pos->color_bb[pos_get_piece_color(piece)] &= ~bb;
	pos->type_bb[pos_get_piece_type(piece)]  &= ~bb;
	pos->board[sq] = PIECE_NONE;
}

/*
 * Place a piece at a square, if another piece is at this square, it will be
 * removed first.
 * 
 * There's an important detail about this function. The bitboards are stored in
 * a piece centric format, so we can't just overwrite the old piece with the
 * new one if the pieces are of different types, the bitboard of the piece
 * being replaced would still store the piece as if it's still on the board,
 * because only the new piece's board would be modified. Because of that, the
 * old piece must be removed first with the pos_remove_piece function by the
 * caller. The reason why this is not done here is to avoid slowing down code
 * that places a piece in an empty square.
 */
void pos_place_piece(Position *pos, Square sq, Piece piece)
{
	const u64 bb = U64(0x1) << sq;
	if (pos->board[sq] != PIECE_NONE)
		pos_remove_piece(pos, sq);
	pos->color_bb[pos_get_piece_color(piece)] |= bb;
	pos->type_bb[pos_get_piece_type(piece)]  |= bb;
	pos->board[sq] = piece;
}

void pos_reset_halfmove_clock(Position *pos)
{
	pos->irreversible->halfmove_clock = 0;
}

void pos_increment_halfmove_clock(Position *pos)
{
	++pos->irreversible->halfmove_clock;
}

void pos_unset_enpassant(Position *pos)
{
	pos->irreversible->castling_rights_and_enpassant &= 0xf;
}

/*
 * Set the possibility of en passant and store the file.
 */
void pos_set_enpassant(Position *pos, File file)
{
	pos->irreversible->castling_rights_and_enpassant &= 0x8f;
	pos->irreversible->castling_rights_and_enpassant |= 0x80;
	pos->irreversible->castling_rights_and_enpassant |= (file & 0x7) << 4;
}

Piece pos_get_captured_piece(const Position *pos)
{
	return pos->irreversible->captured_piece;
}

int pos_has_castling_right(const Position *pos, Color c, CastlingSide side)
{
	return (pos->irreversible->castling_rights_and_enpassant &
	        0x1 << side << 2 * c) != 0;
}

int pos_get_fullmove_counter(const Position *pos)
{
	return pos->fullmove_counter;
}

int pos_get_halfmove_clock(const Position *pos)
{
	return pos->irreversible->halfmove_clock;
}

int pos_enpassant_possible(const Position *pos)
{
	return pos->irreversible->castling_rights_and_enpassant & 0x80;
}

Square pos_get_enpassant(const Position *pos)
{
	const File f = (pos->irreversible->castling_rights_and_enpassant
	                & 0x70) >> 4;
	const Rank r = pos->side_to_move == COLOR_WHITE ? RANK_6 : RANK_3;
	return pos_file_rank_to_square(f, r);
}

Color pos_get_side_to_move(const Position *pos)
{
	return pos->side_to_move;
}

Square pos_get_king_square(const Position *pos, Color c)
{
	const Piece piece = pos_make_piece(PIECE_TYPE_KING, c);
	const u64 bb = pos_get_piece_bitboard(pos, piece);
	return get_index_of_first_bit(bb);
}

Piece pos_get_piece_at(const Position *pos, Square sq)
{
	return pos->board[sq];
}

int pos_get_number_of_pieces(const Position *pos, Piece piece)
{
	const u64 bb = pos_get_piece_bitboard(pos, piece);

	return count_bits(bb);
}

u64 pos_get_piece_bitboard(const Position *pos, Piece piece)
{
	const PieceType type = pos_get_piece_type(piece);
	const Color color = pos_get_piece_color(piece);
	const u64 bb = pos->type_bb[type] & pos->color_bb[color];
	return bb;
}

u64 pos_get_color_bitboard(const Position *pos, Color c)
{
	return pos->color_bb[c];
}

void pos_backtrack_irreversible_state(Position *pos)
{
	struct irreversible_state *const current = pos->irreversible;
	pos->irreversible = pos->irreversible->previous;
	free(current);
}

/*
 * This function must be called before externally calling any function that
 * modifies the irreversible state of the position.
 *
 * It creates a new copy of the old irreversible state and pushes
 * it onto the stack, making it the current one. The reversible state is
 * preserved since changes can be undone.
 */
void pos_start_new_irreversible_state(Position *pos)
{
	struct irreversible_state *current = pos->irreversible;
	struct irreversible_state *new = malloc(sizeof(*new));
	if (!new) {
		fprintf(stderr, "Could not allocate memory.\n");
		exit(1);
	}
	memcpy(new, current, sizeof(struct irreversible_state));
	new->previous = current;
	pos->irreversible = new;
}

Position *pos_copy(const Position *pos)
{
	Position *copy = malloc(sizeof(Position));
	if (!pos) {
		fprintf(stderr, "Could not allocate memory.\n");
		exit(1);
	}
	memcpy(copy, pos, sizeof(Position));

	copy->irreversible = malloc(sizeof(struct irreversible_state));
	if (!pos->irreversible) {
		fprintf(stderr, "Could not allocate memory.\n");
		exit(1);
	}
	struct irreversible_state *ptr = copy->irreversible;
	for (struct irreversible_state *is = pos->irreversible; is; is = is->previous) {
		memcpy(ptr, is, sizeof(struct irreversible_state));
		if (is->previous) {
			ptr->previous = malloc(sizeof(struct irreversible_state));
			if (!ptr->previous) {
				fprintf(stderr, "Could not allocate memory.\n");
				exit(1);
			}
			ptr = ptr->previous;
		}
	}
	return copy;
}

/*
 * Create a new position using FEN. It returns NULL if the FEN is invalid. It is
 * assumed that the string it not empty and that there are no leading or
 * trailing white spaces. Keep in mind that whether the position is actually
 * valid according to the rules of chess is not checked, so even if the FEN
 * string is a valid FEN string according to the grammar, the position might be
 * illegal. For example, the number of pawns in the board is not checked, so it
 * is possible to set up a position using a FEN string that describes a board
 * with 9 pawns. This is intentional, as the user might want to set up a
 * non-standard board.
 */
Position *pos_create(const char *fen)
{
	Position *pos = malloc(sizeof(Position));
	if (!pos) {
		fprintf(stderr, "Could not allocate memory.\n");
		exit(1);
	}
	pos->irreversible = malloc(sizeof(struct irreversible_state));
	if (!pos->irreversible) {
		fprintf(stderr, "Could not allocate memory.\n");
		exit(1);
	}

	pos->fullmove_counter = 0;
	pos->irreversible->previous = NULL;
	pos->irreversible->captured_piece = PIECE_NONE;
	pos_reset_halfmove_clock(pos);
	pos_unset_enpassant(pos);
	pos_remove_castling(pos, COLOR_WHITE, CASTLING_SIDE_KING);
	pos_remove_castling(pos, COLOR_WHITE, CASTLING_SIDE_QUEEN);
	pos_remove_castling(pos, COLOR_BLACK, CASTLING_SIDE_KING);
	pos_remove_castling(pos, COLOR_BLACK, CASTLING_SIDE_QUEEN);
	for (Square sq = A1; sq <= H8; ++sq)
		pos->board[sq] = PIECE_NONE;
	for (size_t i = 0; i < 6; ++i)
		pos->type_bb[i] = 0;
	for (size_t i = 0; i < 2; ++i)
		pos->color_bb[i] = 0;

	size_t rc = parse_fen(pos, fen);
	if (rc != strlen(fen)) {
		pos_destroy(pos);
		return NULL;
	}
	return pos;
}

void pos_destroy(Position *pos)
{
	struct irreversible_state *prev = NULL;

	for (struct irreversible_state *p = pos->irreversible; p; p = prev) {
		prev = p->previous;
		free(p);
	}
	free(pos);
}

Square pos_file_rank_to_square(File f, Rank r)
{
	return 8 * r + f;
}

File pos_get_file_of_square(Square sq)
{
	return sq % 8;
}

Rank pos_get_rank_of_square(Square sq)
{
	return sq / 8;
}

Color pos_get_piece_color(Piece piece)
{
	return piece & 0x1;
}

PieceType pos_get_piece_type(Piece piece)
{
	return piece >> 1;
}

Piece pos_make_piece(PieceType pt, Color c)
{
	return pt << 1 | c;
}
