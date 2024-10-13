#ifndef YNGINE_MCTS_HPP
#define YNGINE_MCTS_HPP

#include <yngine/board_state.hpp>
#include <yngine/allocators.hpp>

#include <XoshiroCpp.hpp>

#include <future>

namespace Yngine {

struct MCTSNode {
    MCTSNode(Move parent_move, MCTSNode* parent, Color color);

    std::pair<uint32_t, uint32_t> get_half_wins_and_simulations() const;
    float compute_uct(uint32_t parent_simulations) const;
    void create_children(PoolAllocator<MCTSNode>& arena, XoshiroCpp::Xoshiro256StarStar& prng, BoardState board_state);
    MCTSNode* add_child();
    void add_half_wins_and_simulations(uint32_t half_wins, uint32_t simulations);

    std::atomic<uint64_t> half_wins_and_simulations;
    std::atomic<bool> is_parent;
    std::atomic<bool> is_expandable;
    std::atomic<MCTSNode*> unexpanded_child;
    std::atomic<bool> is_fully_expanded;

    const Move parent_move;
    const Color color;

    MCTSNode* parent;
    MCTSNode* first_child;
    MCTSNode* next_sibling;
};

class MCTS {
public:
    // Int limit is the amount of iterations to perform,
    //   right now it can perform up to (thread count - 1) more iterations that specified
    // Float limit is the amount of seconds to search for
    using SearchLimit = std::variant<int, float>;

    // @TODO: move memory limit into search function?
    MCTS(std::size_t memory_limit_bytes);
    ~MCTS();

    MCTS(const MCTS &) = delete;
    MCTS(MCTS &&) = delete;
    MCTS &operator=(const MCTS &) = delete;
    MCTS &operator=(MCTS &&) = delete;

    std::future<Move> search(SearchLimit search_limit, int thread_count=1);
    void apply_move(Move move);
    void set_board(BoardState board);
    BoardState get_board() const;
    MCTSNode* get_root() const;

    static int tree_size(MCTSNode* node);

private:
    Move search_threaded(SearchLimit limit, int thread_count);
    void search_worker(MCTSNode* root, SearchLimit limit);

    static std::tuple<MCTSNode*, BoardState> select(MCTSNode* root, BoardState root_board_state);
    static MCTSNode* expand(MCTSNode* node, BoardState board_state, PoolAllocator<MCTSNode>& pool, XoshiroCpp::Xoshiro256StarStar& prng);
    static GameResult playout(MCTSNode* node, BoardState board_state, XoshiroCpp::Xoshiro256StarStar& prng);
    static void backup(MCTSNode* from, GameResult playout_result);

    void free_subtree(MCTSNode* node);

    BoardState board_state;

    PoolAllocator<MCTSNode> pool;

    MCTSNode* root;

    std::atomic<bool> stop_search;
    std::thread search_thread;
};

}

#endif // YNGINE_MCTS_HPP
