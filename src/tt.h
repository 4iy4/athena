#ifndef TT_H
#define TT_H

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

bool tt_get(NodeData *data, const Position *pos);
void tt_store(const NodeData *data);
void tt_entry_init(NodeData *pos_data, int score, int depth, NodeType type, Move best_move, const Position *pos);
void tt_init(void);
void tt_finish(void);

#endif
