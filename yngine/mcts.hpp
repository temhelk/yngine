#ifndef YNGINE_MCTS_HPP
#define YNGINE_MCTS_HPP

#include <yngine/board_state.hpp>
#include <yngine/allocators.hpp>

#include <XoshiroCpp.hpp>

namespace Yngine {

struct MCTSNode {
    int half_wins = 0;
    int simulations = 0;

    BoardState board_state;
    Move parent_move;

    MCTSNode* parent = nullptr;
    MCTSNode* first_child = nullptr;
    MCTSNode* next_sibling = nullptr;

    float compute_uct() const;
};

class MCTS {
public:
    MCTS();
    MCTS(const MCTS &) = delete;
    MCTS(MCTS &&) = delete;
    MCTS &operator=(const MCTS &) = delete;
    MCTS &operator=(MCTS &&) = delete;

    Move search(int iter_num);
    void apply_move(Move move);

private:
    // @TODO: seed it properly
    XoshiroCpp::Xoshiro256StarStar xoshiro;

    BoardState board_state;

    ArenaAllocator arena;
    MCTSNode* root = nullptr;
};

}

#endif // YNGINE_MCTS_HPP
