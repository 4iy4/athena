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
#include "tt.h"
#include "eval.h"
#include "rng.h"

/*
 * Return the index of what seems to be the most promising move based on the
 * evaluation of the move and the transposition table and all the PV nodes are
 * considered more valuable than any other nodes.
 */
static size_t get_most_promising_move(const Move *moves, size_t len, Position *pos)
{
	int best_score = INT_MIN;
	size_t best_idx = 0;
	for (size_t i = 0; i < len; ++i) {
		const Move move = moves[i];
		NodeData pos_data;
		if (tt_get(&pos_data, pos) && pos_data.type == NODE_TYPE_PV) {
			if (move == pos_data.best_move)
				return i;
		}
		int score = eval_evaluate_move(move, pos);
		if (score > best_score) {
			best_idx = i;
			best_score = score;
		}
	}
	return best_idx;
}

static bool is_in_check(const Position *pos)
{
	const Color c = pos_get_side_to_move(pos);
	const Square king_sq = pos_get_king_square(pos, c);
	return movegen_is_square_attacked(king_sq, !c, pos);
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

/*
 * It will return INT_MAX on checkmate and 0 on stalemate.
 */
static int alpha_beta(Position *pos, int depth, int alpha, int beta, int *nodes)
{
	NodeData pos_data;
	if (tt_get(&pos_data, pos) && pos_data.depth >= depth)
		return pos_data.score;
	if (!depth)
		return quiescence_search(pos, alpha, beta, nodes);

	NodeType type = NODE_TYPE_ALL;
	size_t len = 0;
	Move *moves = movegen_get_pseudo_legal_moves(pos, &len);
	if (!len) {
		if (is_in_check(pos))
			return INT_MAX;
		else
			return 0;
	}
	Move *moves_ptr = moves;
	size_t legal_moves_cnt = 0;
	Move best_move;
	do {
		/* Lazily sort moves instead of doing it all at once, this way
		 * we avoid wasting time sorting moves of branches that are
		 * pruned. */
		if (len > 1) {
			Move first = moves[0];
			size_t i = get_most_promising_move(moves, len, pos);
			Move most_promising = moves[i];
			moves[0] = most_promising;
			moves[i] = first;
		}

		Move move = *moves;
		if (!move_is_legal(pos, move)) {
			--len;
			++moves;
			continue;
		}
		++legal_moves_cnt;
		move_do(pos, move);
		int score = -alpha_beta(pos, depth - 1, -beta, -alpha, nodes);
		move_undo(pos, move);
		++*nodes;
		if (score > alpha) {
			alpha = score;
			best_move = move;
			type = NODE_TYPE_PV;
		}
		if (alpha >= beta) {
			type = NODE_TYPE_CUT;
			break;
		}

		--len;
		++moves;
	} while (len);
	free(moves_ptr);
	if (!legal_moves_cnt) {
		if (is_in_check(pos))
			return INT_MAX;
		else
			return 0;
	}

	tt_entry_init(&pos_data, alpha, depth, type, best_move, pos);
	tt_store(&pos_data);
	return alpha;
}

void search_init(void)
{
	tt_init();
	eval_init();
}

void search_finish(void)
{
	tt_finish();
}

/*
 * It will return 0 in case of checkmate or stalemate.
 */
Move search_search(const Position *pos)
{
	const int depth = 6;
	const Move null_move = 0;

	Position *mut_pos = pos_copy(pos);
	size_t len;
	Move *moves = movegen_get_pseudo_legal_moves(mut_pos, &len);
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
	 * lead to a checkmate or stalemate.) */
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
