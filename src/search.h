#ifndef SEARCH_H
#define SEARCH_H

Move search_get_best_move(const Position *pos, int depth);
void search_finish(void);
void search_init(void);

#endif
