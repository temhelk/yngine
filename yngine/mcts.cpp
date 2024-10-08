#include <yngine/mcts.hpp>

#include <limits>
#include <cmath>
#include <random>
#include <iostream>
#include <chrono>

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

MCTS::MCTS(std::size_t memory_limit_bytes)
    : board_state{}
    , pool{memory_limit_bytes}
    , root{nullptr}
    , stop_search{false} {
}

MCTS::~MCTS() {
    this->stop_search = true;
    this->search_thread.join();
}

std::future<Move> MCTS::search(float seconds, int thread_count) {
    std::packaged_task<Move(MCTS*, SearchLimit, int)> task{&MCTS::search_threaded};
    auto future = task.get_future();

    if (this->search_thread.joinable()) {
        this->search_thread.join();
    }
    this->search_thread = std::thread{std::move(task), this, seconds, thread_count};

    return std::move(future);
}

Move MCTS::search_threaded(SearchLimit limit, int thread_count) {
    // Check if we only have one move, if so return it immediatly
    MoveList moves_from_root;
    this->board_state.generate_moves(moves_from_root);
    if (moves_from_root.get_size() == 1) {
        return moves_from_root[0];
    }

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
    std::vector<std::thread> workers;
    for (int thread_index = 0; thread_index < thread_count; thread_index++) {
        workers.push_back(std::thread{&MCTS::search_worker, this, this->root, limit});
    }

    // Wait for workers to finish
    for (auto& worker : workers) {
        worker.join();
    }

    uint32_t most_simulations = 0;
    MCTSNode* most_simulations_node = nullptr;

    // @TODO: handle case where no children of the root were created
    auto child = this->root->first_child;
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

    const auto best_move = most_simulations_node->parent_move;

    const auto [half_wins, simulations] = most_simulations_node->get_half_wins_and_simulations();
    const auto root_simulations = this->root->get_half_wins_and_simulations().second;

    std::cout << "DEBUG: win rate = "
        << ((float)half_wins / 2 / simulations)
        << ", move confidence = " << ((float)simulations / root_simulations) << std::endl;

    std::cout << "DEBUG: iters = " << root_simulations << ", memory used (MB) = " << (this->pool.used_bytes() / 1024 / 1024) << ", tree size = " << MCTS::tree_size(this->root) << "\n" << std::endl;

    return best_move;
}

void MCTS::search_worker(MCTSNode* root, SearchLimit limit) {
    const auto start_time = std::chrono::steady_clock::now();

    std::random_device rd;
    XoshiroCpp::Xoshiro256StarStar prng((static_cast<uint64_t>(rd()) << 32) | rd());

    int iter = 0;
    while (!this->stop_search) {
        // Check if we exceeded the computational budget
        if (auto* limit_iters = std::get_if<int>(&limit)) {
            if (iter >= *limit_iters) {
                break;
            }
        } else if (auto* limit_seconds = std::get_if<float>(&limit)) {
            const auto elapsed = std::chrono::steady_clock::now() - start_time;
            const auto elapsed_seconds = std::chrono::duration<float>(elapsed).count();

            if (elapsed_seconds >= *limit_seconds) {
                break;
            }
        } else {
            assert(false);
        }

        // Selection phase
        auto [selected_node, selected_board_state] = MCTS::select(root, this->board_state);

        // Expansion phase
        MCTSNode* expanded_node = MCTS::expand(selected_node, selected_board_state, pool, prng);

        // Simulation phase
        GameResult playout_result = MCTS::playout(expanded_node, selected_board_state, prng);

        // Backpropagation phase
        MCTS::backup(expanded_node, playout_result);

        iter++;
    }
}

std::tuple<MCTSNode*, BoardState> MCTS::select(MCTSNode* root, BoardState root_board_state) {
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
        current_board_state.apply_move(greatest_uct_node->parent_move);
    }

    return std::tie(current, current_board_state);
}

MCTSNode* MCTS::expand(MCTSNode* node, BoardState board_state, PoolAllocator<MCTSNode>& pool, XoshiroCpp::Xoshiro256StarStar& prng) {
    MCTSNode* result = node;

    if (board_state.get_next_action() != NextAction::Done) {
        node->create_children(pool, prng, board_state);
        result = node->add_child();
    }

    return result;
}

GameResult MCTS::playout(MCTSNode* node, BoardState board_state, XoshiroCpp::Xoshiro256StarStar& prng) {
    board_state.playout(prng);
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
    this->board_state.apply_move(move);

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

    if (this->root) {
        auto [half_wins, simulations] = this->root->get_half_wins_and_simulations();
        std::cout << "DEBUG: move winrate = " << (float)half_wins / 2 / simulations << "\n";
    }

    std::cout << "DEBUG: tree size after move = " << MCTS::tree_size(this->root) << "\n" << std::endl;
}

void MCTS::set_board(BoardState board) {
    this->board_state = board;
}

BoardState MCTS::get_board() const {
    return this->board_state;
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
