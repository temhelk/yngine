#ifndef YNGINE_MCTS_HPP
#define YNGINE_MCTS_HPP

#include <yngine/board_state.hpp>
#include <yngine/allocators.hpp>

#include <XoshiroCpp.hpp>

#include <thread>

namespace Yngine {

struct SearchInfo {
    Move best_move;
    size_t iterations;
    float win_rate;
    float confidence;
    size_t memory_used;
};

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
    // @TODO: move memory limit into search function?
    MCTS(bool is_blitz, std::size_t memory_limit_bytes);
    ~MCTS();

    MCTS(const MCTS &) = delete;
    MCTS(MCTS &&) = delete;
    MCTS &operator=(const MCTS &) = delete;
    MCTS &operator=(MCTS &&) = delete;

    // If we are finding a move for the AI agent, we don't want to
    // wait for any amount of time if the move is forced (only one available)
    // so the game UI uses that function to get that move if available
    // while when analyzing a game, we don't care that there's only on move
    // available, we want to get different statistics (like winrate and so on)
    std::optional<Move> check_for_forced_move();
    void start_search(int thread_count);
    void stop_search();
    bool is_searching();
    std::optional<SearchInfo> get_search_info();
    // Should be called after the search has ended
    std::size_t get_tree_size();

    void apply_move(Move move);
    void set_board(BoardState board);

    BoardState get_board() const;
    MCTSNode* get_root() const;

    size_t get_memory_limit_bytes() const;

    static int tree_size(MCTSNode* node);

private:
    void search_threaded(std::stop_token stoken, int thread_count);
    void search_worker(std::stop_token stoken, MCTSNode* root);

    static std::tuple<MCTSNode*, BoardState> select(MCTSNode* root, BoardState root_board_state, bool is_blitz);
    static std::tuple<MCTSNode*, BoardState> expand(MCTSNode* node, BoardState board_state, PoolAllocator<MCTSNode>& pool, XoshiroCpp::Xoshiro256StarStar& prng, bool is_blitz);
    static GameResult playout(MCTSNode* node, BoardState board_state, XoshiroCpp::Xoshiro256StarStar& prng, bool is_blitz);
    static void backup(MCTSNode* from, GameResult playout_result);

    void free_subtree(MCTSNode* node);

    BoardState board_state;
    bool is_blitz;
    size_t memory_limit_bytes;

    PoolAllocator<MCTSNode> pool;

    MCTSNode* root;

    std::jthread search_thread;
};

}

#endif // YNGINE_MCTS_HPP
