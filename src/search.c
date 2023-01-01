#include <stdlib.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

#include <check.h>

#include "bit.h"
#include "pos.h"
#include "move.h"
#include "movegen.h"
#include "eval.h"
#include "rng.h"

static void sort_moves(Move *list, size_t len, Position *pos)
{
	if (!len)
		return;
	for (size_t i = 0; i < len - 1; ++i) {
		size_t promising = i;
		for (size_t j = i + 1; j < len; ++j) {
			int pvalue = eval_evaluate_move(list[promising], pos);
			int jvalue = eval_evaluate_move(list[j], pos);
			if (pvalue < jvalue)
				promising = j;
		}
		if (promising != i) {
			Move tmp = list[i];
			list[i] = list[promising];
			list[promising] = tmp;
		}
	}
}

static int quiescence_search(Position *pos, int alpha, int beta, int *nodes)
{
	int score = eval_evaluate(pos);
	alpha = score > alpha ? score : alpha;

	size_t len;
	Move *moves = movegen_get_pseudo_legal_moves(pos, &len);
	for (size_t i = 0; i < len; ++i) {
		Move move = moves[i];
		if (!move_is_legal(pos, move) || !move_is_capture(move))
			continue;
		move_do(pos, move);
		score = -quiescence_search(pos, -beta, -alpha, nodes);
		move_undo(pos, move);
		++*nodes;
		alpha = score > alpha ? score : alpha;
		if (alpha >= beta)
			break;
	}
	free(moves);

	return alpha;
}

static int alpha_beta(Position *pos, int depth, int alpha, int beta, int *nodes)
{
	NodeData pos_data;
	if (eval_get_node_data(&pos_data, pos) && pos_data.depth >= depth)
		return pos_data.score;
	if (!depth)
		return quiescence_search(pos, alpha, beta, nodes);

	NodeType type = NODE_TYPE_ALL;
	size_t len;
	Move *moves = movegen_get_pseudo_legal_moves(pos, &len);
	sort_moves(moves, len, pos);
	for (size_t i = 0; i < len; ++i) {
		Move move = moves[i];
		if (!move_is_legal(pos, move))
			continue;
		move_do(pos, move);
		int score = -alpha_beta(pos, depth - 1, -beta, -alpha, nodes);
		move_undo(pos, move);
		++*nodes;
		if (score > alpha) {
			alpha = score;
			type = NODE_TYPE_PV;
		}
		if (alpha >= beta) {
			type = NODE_TYPE_CUT;
			break;
		}
	}
	free(moves);

	eval_node_data_init(&pos_data, alpha, depth, type, pos);
	eval_store_node_data(&pos_data);
	return alpha;
}

void search_init(void)
{
	eval_init();
}

void search_finish(void)
{
	eval_finish();
}

/*
 * It will return 0 in case of checkmate.
 */
Move search_search(const Position *pos)
{
	const int depth = 6;
	const Move null_move = 0;

	Position *mut_pos = pos_copy(pos);
	size_t len;
	Move *moves = movegen_get_pseudo_legal_moves(mut_pos, &len);
	sort_moves(moves, len, mut_pos);
	int alpha = INT_MIN + 1, beta = INT_MAX;
	Move best_move = null_move;
	int nodes = 0;
	for (size_t i = 0; i < len; ++i) {
		Move move = moves[i];
		if (!move_is_legal(mut_pos, move))
			continue;
		move_do(mut_pos, move);
		int score = -alpha_beta(mut_pos, depth - 1, -beta, -alpha, &nodes);
		nodes += 1;
		move_undo(mut_pos, move);
		if (score > alpha) {
			alpha = score;
			best_move = move;
		}
		if (alpha >= beta)
			break;
	}
	/* Play any move if no best move was found (probably because all moves
	 * lead to a checkmate.) */
	if (best_move == null_move && len != 0) {
		for (size_t i = 0; i < len; ++i) {
			if (move_is_legal(mut_pos, moves[i]))
				best_move = moves[i];
		}
	}
	free(moves);
	pos_destroy(mut_pos);

	printf("searched %d nodes\n", nodes);
	return best_move;
}
