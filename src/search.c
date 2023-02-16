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

static const int INFINITE = SHRT_MAX;

#define MAX_DEPTH 128
#define MAX_KILLER_MOVES 2

Move killer_moves[MAX_DEPTH][MAX_KILLER_MOVES];

/*
 * This function stores a new killer move by shifting all the killer moves for
 * a certain depth, discarding the move in the last slot, the oldest one, and
 * then places the new move in the first slot. It is important that all the
 * slots contain different moves, otherwise we waste computation time in move
 * ordering looking for the same killer move again.
 */
static void store_killer(Move move, int depth)
{
	const size_t depth_idx = depth - 1;;

	for (int i = 0; i < MAX_KILLER_MOVES; ++i) {
		if (move == killer_moves[depth_idx][i])
			return;
	}
	for (int i = MAX_KILLER_MOVES - 1; i > 0; --i)
		killer_moves[depth_idx][i] = killer_moves[depth_idx][i - 1];
	killer_moves[depth_idx][0] = move;
}

static bool is_killer(Move move, int depth)
{
	for (int i = 0; i < MAX_KILLER_MOVES; ++i) {
		Move killer = killer_moves[depth - 1][i];
		if (killer && move == killer)
			return true;
	}
	return false;
}

/*
 * Return the index of what seems to be the most promising by evaluating moves.
 *
 * The best move of PV nodes are stored in the transposition table so and since
 *  all the moves of PV nodes moves have been searched we know for sure that
 * that move is the best for that position. So the best move of PV nodes have
 * higher priority than any other moves.
 *
 * The killer moves are searched next because they caused a beta cutoff and are
 * likely to cause a beta cutoff again in the rest of the moves. However, some
 * captures have the potential to make the killer move not a good choice (for
 * example, if white captures black's queen in the other branch before black
 * makes the killer move then the killer move might no longer be an option for
 * black). So very good captures have priority.
 * 
 * To simulate this priority order we have some offsets that act as the starting
 * point for the score of a move which is then added to the offset. Captures
 * have a smaller offset than killer moves, but they are still close enough for
 * captures to surpass the killer move if it is good enough and/or the killer
 * move is bad enough.
 * 
 * There is no offset for the best move of PV nodes because they are always
 * searched first, so if the position is a PV node we just return the move we
 * have in the transposition table. And other moves have offset 0 because they
 * have lower priority than captures.
 */
static size_t get_most_promising_move(const Move *moves, size_t len, Position *pos, int depth)
{
	static const int capture_offset = INFINITE / 64;
	static const int killer_offset = INFINITE / 32;
	int best_score = -INFINITE;
	size_t best_idx = 0;

	for (size_t i = 0; i < len; ++i) {
		const Move move = moves[i];
		NodeData pos_data;
		if (tt_get(&pos_data, pos) && pos_data.type == NODE_TYPE_PV) {
			if (move == pos_data.best_move)
				return i;
		}
	}

	for (size_t i = 0; i < len; ++i) {
		const Move move = moves[i];
		int score = 0;
		if (is_killer(move, depth))
			score = killer_offset + eval_evaluate_move(move, pos);
		else if (move_is_capture(move))
			score = capture_offset + eval_evaluate_move(move, pos);
		else
			score = eval_evaluate_move(move, pos);

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
 * It will return MAX_INT on checkmate and 0 on stalemate.
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
			size_t i = get_most_promising_move(moves, len, pos, depth);
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
			if (!move_is_capture(move))
				store_killer(move, depth);
			type = NODE_TYPE_CUT;
			break;
		}

		--len;
		++moves;
	} while (len);
	free(moves_ptr);
	if (!legal_moves_cnt) {
		if (is_in_check(pos))
			return INFINITE;
		else
			return 0;
	}

	tt_entry_init(&pos_data, alpha, depth, type, best_move, pos);
	tt_store(&pos_data);
	return alpha;
}

void search_init(void)
{
	for (size_t i = 0; i < MAX_DEPTH; ++i) {
		for (size_t j = 0; j < MAX_KILLER_MOVES; ++j)
			killer_moves[i][j] = 0;
	}
	tt_init();
	eval_init();
}

void search_finish(void)
{
	tt_finish();
}

static Move search(Position *pos, int depth)
{
	const Move null_move = 0;

	size_t len;
	Move *moves = movegen_get_pseudo_legal_moves(pos, &len);

	int alpha = -INFINITE, beta = INFINITE;
	Move best_move = null_move;
	int nodes = 0;
	for (size_t i = 0; i < len; ++i) {
		Move move = moves[i];
		if (!move_is_legal(pos, move))
			continue;
		move_do(pos, move);
		int score = -alpha_beta(pos, depth - 1, -beta, -alpha, &nodes);
		nodes += 1;
		move_undo(pos, move);
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
			if (move_is_legal(pos, moves[i]))
				best_move = moves[i];
		}
	}
	free(moves);

	printf("searched %d nodes\n", nodes);
	return best_move;
}

/*
 * It will return 0 in case of checkmate or stalemate. If depth is less than or
 * equal to 0 the function will use a default depth.
 */
Move search_get_best_move(const Position *pos, int depth)
{
	const int default_depth = 6;
	const Move null_move = 0;

	Position *mut_pos = pos_copy(pos);
	if (depth <= 0 || depth > MAX_DEPTH)
		depth = default_depth;
	
	Move best_move = null_move;
	for (int curr_depth = 1; curr_depth <= depth; ++curr_depth)
		best_move = search(mut_pos, curr_depth);
	return best_move;
}
