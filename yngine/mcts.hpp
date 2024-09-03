#ifndef YNGINE_MCTS_HPP
#define YNGINE_MCTS_HPP

#include <yngine/board_state.hpp>
#include <yngine/allocators.hpp>

#include <XoshiroCpp.hpp>

#include <future>

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
    // Int limit is the amount of iterations to perform
    // Float limit is the amount of seconds to search for
    using SearchLimit = std::variant<int, float>;

    MCTS(std::size_t memory_limit_bytes);
    ~MCTS();

    MCTS(const MCTS &) = delete;
    MCTS(MCTS &&) = delete;
    MCTS &operator=(const MCTS &) = delete;
    MCTS &operator=(MCTS &&) = delete;

    std::future<Move> search(float seconds);
    void apply_move(Move move);

private:
    Move search_threaded(SearchLimit limit);

    XoshiroCpp::Xoshiro256StarStar xoshiro;

    BoardState board_state;

    ArenaAllocator arena;
    MCTSNode* root = nullptr;

    std::atomic<bool> stop_search;
    std::thread search_thread;
};

}

#endif // YNGINE_MCTS_HPP
