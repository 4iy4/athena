#ifndef EVALUATION_H
#define EVALUATION_H

int eval_evaluate(const Position *pos);
int eval_get_average_mvv_lva_score(void);
int eval_compute_mvv_lva_score(Move move, const Position *pos);
int eval_evaluate_move(Move move, Position *pos);
void eval_init(void);

#endif
