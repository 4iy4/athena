#ifndef EVALUATION_H
#define EVALUATION_H

/*
 * PV-nodes are positions that have a score in the interval [alpha, beta). All
 * the child nodes have been searched because there was not prunning and the
 * value returned is exact, that is, the best possible score since there was no
 * branch cut-off during the search.
 *
 * A cut-node had a beta-cutoff performed during its search, so a minimum of one
 * move for this position has been searched, since that's needed for the
 * prunning. Because not all the child nodes are searched the score returned is
 * a lower bound, so a player could possibly get a better score from the moves
 * that weren't used in the search but the opponent wouldn't allow (hence the
 * branch cut-off).
 *
 * If no moves exceeded alpha, this node is called an all-node. In
 * this case alpha is returned as the score so the score is an upper bound.
 */
typedef enum node_type {
	NODE_TYPE_PV,
	NODE_TYPE_CUT,
	NODE_TYPE_ALL,
} NodeType;

typedef struct node_data {
	int score;
	u8 depth;
	u8 type;
	u64 hash;
	Move best_move;
} NodeData;

int eval_evaluate(const Position *pos);
int eval_evaluate_move(Move move, Position *pos);
bool eval_get_node_data(NodeData *data, const Position *pos);
void eval_store_node_data(const NodeData *data);
void eval_node_data_init(NodeData *pos_data, int score, int depth, NodeType type, Move best_move, const Position *pos);
void eval_finish(void);
void eval_init(void);

#endif
