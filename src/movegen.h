#ifndef MOVEGEN_H
#define MOVEGEN_H

int movegen_get_number_of_possible_moves(Piece piece, Square sq);
bool movegen_is_square_attacked(Square sq, Color by_side, const Position *pos);
int movegen_get_number_of_pseudo_legal_moves(const Position *pos, Color c);
Move *movegen_get_pseudo_legal_moves(const Position *pos, size_t *len);
void movegen_init(void);

#endif
