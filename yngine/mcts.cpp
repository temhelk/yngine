#include <yngine/mcts.hpp>

#include <limits>
#include <cmath>
#include <random>
#include <functional>
#include <algorithm>

namespace Yngine {

MCTSNode::MCTSNode(Move parent_move, MCTSNode* parent, Color color)
    : half_wins_and_simulations{0}
    , is_parent{false}
    , is_expandable{false}
    , unexpanded_child{nullptr}
    , is_fully_expanded{false}
    , parent_move{parent_move}
    , parent{parent}
    , color{color}
    , first_child{nullptr}
    , next_sibling{nullptr} {
}

void MCTSNode::create_children(PoolAllocator<MCTSNode>& arena, XoshiroCpp::Xoshiro256StarStar& prng, BoardState board_state) {
    if (this->is_parent.exchange(true) == false) {
        MoveList move_list;
        board_state.generate_moves(move_list);

        std::shuffle(&move_list[0], &move_list[move_list.get_size()], prng);

        const Color node_color = board_state.whose_move();

        const auto new_first_child = arena.allocate(
            move_list[0],
            this,
            node_color
        );

        if (!new_first_child) {
            this->is_parent.store(false);
            return;
        }

        this->first_child = new_first_child;

        bool failed_to_allocate_children = false;

        MCTSNode* last_child = new_first_child;
        for (int move_index = 1; move_index < move_list.get_size(); move_index++) {
            const auto move = move_list[move_index];

            MCTSNode* new_child = arena.allocate(
                move,
                this,
                node_color
            );

            // We failed to allocate some of the children, we have to revert
            // the tree to a consistent state, so we deallocate all of them
            if (!new_child) {
                failed_to_allocate_children = true;
                break;
            }

            last_child->next_sibling = new_child;
            last_child = new_child;
        }

        // Deallocate children if failed
        if (failed_to_allocate_children) {
            MCTSNode* current_child = this->first_child;
            while (current_child) {
                const auto next_child = current_child->next_sibling;

                arena.free(current_child);

                current_child = next_child;
            }

            this->first_child = nullptr;
            this->is_parent.store(false);

            return;
        }

        this->unexpanded_child.store(this->first_child);
        this->is_expandable.store(true, std::memory_order_release);
    }
}

MCTSNode* MCTSNode::add_child() {
    if (this->is_expandable.load(std::memory_order_acquire) == true) {
        MCTSNode* expected = this->unexpanded_child.load();
        MCTSNode* desired;

        do {
            if (expected == nullptr)
                return this;

            desired = expected->next_sibling;
        } while (!this->unexpanded_child.compare_exchange_weak(expected, desired));

        if (expected->next_sibling == nullptr) {
            this->is_fully_expanded.store(true);
        }

        return expected;
    } else {
        return this;
    }
}

void MCTSNode::add_half_wins_and_simulations(uint32_t half_wins, uint32_t simulations) {
    uint64_t increase =
        static_cast<uint64_t>(half_wins) << 32 |
        static_cast<uint64_t>(simulations);

    this->half_wins_and_simulations.fetch_add(increase);
}

std::pair<uint32_t, uint32_t> MCTSNode::get_half_wins_and_simulations() const {
    const uint64_t hw_and_s = this->half_wins_and_simulations.load();

    const uint32_t half_wins = static_cast<uint32_t>(hw_and_s >> 32);
    const uint32_t simulations = static_cast<uint32_t>(hw_and_s);

    return std::make_pair(half_wins, simulations);
}

float MCTSNode::compute_uct(uint32_t parent_simulations) const {
    const auto [half_wins, simulations] = this->get_half_wins_and_simulations();

    if (simulations == 0) {
        return std::numeric_limits<float>::infinity();
    }

    const float exploration_parameter = 0.5f; // std::numbers::sqrt2_v<float>;

    const float exploitation =
        (static_cast<float>(half_wins) / 2) /
        static_cast<float>(simulations);

    const float exploration =
        exploration_parameter *
        std::sqrt(
            std::log(static_cast<float>(parent_simulations)) /
            static_cast<float>(simulations)
        );

    return exploitation + exploration;
}

MCTS::MCTS(bool is_blitz, std::size_t memory_limit_bytes)
    : board_state{}
    , is_blitz{is_blitz}
    , memory_limit_bytes{memory_limit_bytes}
    , pool{memory_limit_bytes}
    , root{nullptr} {
}

MCTS::~MCTS() {
    this->stop_search();
}

std::optional<Move> MCTS::check_for_forced_move() {
    MoveList moves_from_root;
    this->board_state.generate_moves(moves_from_root);
    if (moves_from_root.get_size() == 1) {
        return moves_from_root[0];
    }

    return std::nullopt;
}

void MCTS::start_search(int thread_count) {
    // Make sure we're not running a search already @TODO: add assert
    this->stop_search();

    this->search_thread = std::jthread{std::bind_front(&MCTS::search_threaded, this), thread_count};
}

void MCTS::stop_search() {
    if (this->is_searching()) {
        this->search_thread.request_stop();
        this->search_thread.join();
    }
}

bool MCTS::is_searching() {
    return this->search_thread.joinable();
}

std::optional<SearchInfo> MCTS::get_search_info() {
    SearchInfo result{};

    if (!this->root)
        return std::nullopt;

    auto child = this->root->first_child;
    if (!child)
        return std::nullopt;

    uint32_t most_simulations = 0;
    MCTSNode* most_simulations_node = nullptr;
    while (true) {
        const auto simulations = child->get_half_wins_and_simulations().second;

        if (simulations > most_simulations) {
            most_simulations = simulations;
            most_simulations_node = child;
        }

        if (!child->next_sibling)
            break;

        child = child->next_sibling;
    }

    result.best_move = most_simulations_node->parent_move;

    const auto [half_wins, simulations] = most_simulations_node->get_half_wins_and_simulations();
    const auto root_simulations = this->root->get_half_wins_and_simulations().second;

    result.win_rate = (float)half_wins / 2 / simulations;
    result.confidence = (float)simulations / root_simulations;

    result.iterations = root_simulations;
    result.memory_used = this->pool.used_bytes();

    return result;
}

std::size_t MCTS::get_tree_size() {
    return MCTS::tree_size(this->root);
}

void MCTS::search_threaded(std::stop_token stoken, int thread_count) {
    // Allocate root node if we haven't retained a tree from previous search
    if (!this->root) {
        this->root = this->pool.allocate(
            PassMove{},
            nullptr,
            opposite(this->board_state.whose_move()) // Color here doesn't matter
        );

        if (!this->root) {
            abort();
        }
    }

    // Start workers
    std::vector<std::jthread> workers;
    workers.reserve(thread_count);

    for (int thread_index = 0; thread_index < thread_count; thread_index++) {
        workers.push_back(std::jthread{
            [this](std::stop_token worker_token) {
                this->search_worker(worker_token, this->root);
            },
            stoken
        });
    }

    // Wait for workers to finish
    for (auto& worker : workers) {
        worker.join();
    }
}

void MCTS::search_worker(std::stop_token stoken, MCTSNode* root) {
    std::random_device rd;
    XoshiroCpp::Xoshiro256StarStar prng((static_cast<uint64_t>(rd()) << 32) | rd());

    while (!stoken.stop_requested()) {
        // Selection phase
        auto [selected_node, selected_board_state] = MCTS::select(root, this->board_state, this->is_blitz);

        // Expansion phase
        auto [expanded_node, expanded_board_state] = MCTS::expand(selected_node, selected_board_state, pool, prng, this->is_blitz);

        // Simulation phase
        GameResult playout_result = MCTS::playout(expanded_node, expanded_board_state, prng, this->is_blitz);

        // Backpropagation phase
        MCTS::backup(expanded_node, playout_result);
    }
}

std::tuple<MCTSNode*, BoardState> MCTS::select(MCTSNode* root, BoardState root_board_state, bool is_blitz) {
    MCTSNode* current = root;
    BoardState current_board_state = root_board_state;

    while (current->is_fully_expanded.load()) {
        uint32_t parent_simulations = current->get_half_wins_and_simulations().second;

        MCTSNode* greatest_uct_node = current->first_child;
        float greatest_uct = greatest_uct_node->compute_uct(parent_simulations);

        MCTSNode* current_child = current->first_child;
        while (current_child->next_sibling) {
            if (std::isinf(greatest_uct))
                break;

            current_child = current_child->next_sibling;

            const auto current_uct = current_child->compute_uct(parent_simulations);
            if (current_uct > greatest_uct) {
                greatest_uct = current_uct;
                greatest_uct_node = current_child;
            }
        }

        current = greatest_uct_node;
        current_board_state.apply_move(greatest_uct_node->parent_move, is_blitz ? 4 : 2);
    }

    return std::tie(current, current_board_state);
}

std::tuple<MCTSNode*, BoardState> MCTS::expand(MCTSNode* node, BoardState board_state, PoolAllocator<MCTSNode>& pool, XoshiroCpp::Xoshiro256StarStar& prng, bool is_blitz) {
    MCTSNode* expanded_node = node;

    if (board_state.get_next_action() != NextAction::Done) {
        node->create_children(pool, prng, board_state);
        expanded_node = node->add_child();
    }

    auto expanded_board_state = board_state;

    // We don't always get a new child node from add_child()
    // sometimes we get the node we tried to expand
    if (expanded_node != node) {
        expanded_board_state.apply_move(expanded_node->parent_move, is_blitz ? 4 : 2);
    }

    return std::tie(expanded_node, expanded_board_state);
}

GameResult MCTS::playout(MCTSNode* node, BoardState board_state, XoshiroCpp::Xoshiro256StarStar& prng, bool is_blitz) {
    board_state.playout(prng, is_blitz ? 4 : 2);
    return board_state.game_result();
}

void MCTS::backup(MCTSNode* from, GameResult playout_result) {
    MCTSNode* propagation_current = from;
    while (propagation_current->parent) {
        uint32_t half_wins = 0;

        if (playout_result == GameResult::Draw) {
            half_wins = 1;
        } else {
            const auto node_color = propagation_current->color;

            if (playout_result == GameResult::WhiteWon && node_color == Color::White ||
                playout_result == GameResult::BlackWon && node_color == Color::Black) {
                half_wins = 2;
            }
        }

        propagation_current->add_half_wins_and_simulations(half_wins, 1);

        propagation_current = propagation_current->parent;
    }

    // Add 1 simulation to the root, we don't track wins for it
    propagation_current->add_half_wins_and_simulations(0, 1);
}

void MCTS::free_subtree(MCTSNode* node) {
    // @TODO: do we want to reverse the freeing order?
    MCTSNode* current_child = node->first_child;
    while (current_child) {
        MCTSNode* next_child = current_child->next_sibling;

        this->free_subtree(current_child);

        current_child = next_child;
    }

    this->pool.free(node);
}

void MCTS::apply_move(Move move) {
    assert(!this->is_searching());
    this->stop_search();

    this->board_state.apply_move(move, this->is_blitz ? 4 : 2);

    // Reuse part of the tree that we have from previous searches if possible
    if (this->root) {
        MCTSNode* new_root = nullptr;
        MCTSNode* current_child = this->root->first_child;

        if (!current_child) {
            this->root = nullptr;
            return;
        }

        while (current_child) {
            MCTSNode* next_child = current_child->next_sibling;

            if (current_child->parent_move == move) {
                assert(new_root == nullptr);
                new_root = current_child;
            } else {
                this->free_subtree(current_child);
            }

            current_child = next_child;
        }

        if (new_root) {
            new_root->next_sibling = nullptr;
            new_root->parent = nullptr;

            this->root = new_root;
        } else {
            this->root = nullptr;
        }
    }
}

void MCTS::set_board(BoardState board) {
    assert(!this->is_searching());
    this->stop_search();

    this->board_state = board;

    // We deallocate all nodes when we change the position
    // because of reuse, we might still have some after search
    this->root = nullptr;
    this->pool.clear();
}

BoardState MCTS::get_board() const {
    return this->board_state;
}

MCTSNode* MCTS::get_root() const {
    return this->root;
}

size_t MCTS::get_memory_limit_bytes() const {
    return this->memory_limit_bytes;
}

int MCTS::tree_size(MCTSNode* node) {
    if (!node) {
        return 0;
    }

    int children_sizes_sum = 0;

    MCTSNode* current_child = node->first_child;
    while (current_child) {
        children_sizes_sum += MCTS::tree_size(current_child);

        current_child = current_child->next_sibling;
    }

    return children_sizes_sum + 1;
}

}
