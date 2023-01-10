#ifndef EVALUATION_H
#define EVALUATION_H

int eval_evaluate(const Position *pos);
int eval_evaluate_move(Move move, Position *pos);
void eval_init(void);

#endif
