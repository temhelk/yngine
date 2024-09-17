#include <yngine/mcts.hpp>

#include <limits>
#include <numbers>
#include <cmath>
#include <random>
#include <iostream>
#include <chrono>

namespace Yngine {

MCTSNode::MCTSNode(BoardState board_state, Move parent_move, MCTSNode* parent)
    : half_wins_and_simulations{0}
    , is_parent{false}
    , is_expandable{false}
    , unexpanded_child{nullptr}
    , is_fully_expanded{false}
    , board_state{board_state}
    , parent_move{parent_move}
    , parent{parent}
    , first_child{nullptr}
    , next_sibling{nullptr} {
}

void MCTSNode::create_children(ArenaAllocator& arena, XoshiroCpp::Xoshiro256StarStar& prng) {
    if (this->is_parent.exchange(true) == false) {
        MoveList move_list;
        this->board_state.generate_moves(move_list);

        std::shuffle(&move_list[0], &move_list[move_list.get_size()], prng);

        // @TODO: handle out of memory case
        const auto new_first_child = arena.allocate<MCTSNode>(
            this->board_state.with_move(move_list[0]),
            move_list[0],
            this
        );
        this->first_child = new_first_child;

        MCTSNode* last_child = new_first_child;
        for (int move_index = 1; move_index < move_list.get_size(); move_index++) {
            const auto move = move_list[move_index];

            MCTSNode* new_child = arena.allocate<MCTSNode>(
                this->board_state.with_move(move),
                move,
                this
            );

            last_child->next_sibling = new_child;
            last_child = new_child;
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

    const float exploration_parameter = std::numbers::sqrt2_v<float>;

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
    : arena{memory_limit_bytes} {
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
    // Allocate root node
    MCTSNode* root = this->arena.allocate<MCTSNode>(this->board_state, PassMove{}, nullptr);
    if (!root) {
        abort();
    }

    // Start workers
    std::vector<std::thread> workers;
    for (int thread_index = 0; thread_index < thread_count; thread_index++) {
        workers.push_back(std::thread{&MCTS::search_worker, this, root, limit});
    }

    // Wait for workers to finish
    for (auto& worker : workers) {
        worker.join();
    }

    uint32_t most_simulations = 0;
    MCTSNode* most_simulations_node = nullptr;

    // @TODO: handle case where no children of the root were created
    auto child = root->first_child;
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
    const auto root_simulations = root->get_half_wins_and_simulations().second;

    std::cerr << "DEBUG: win rate = "
        << ((float)half_wins / 2 / simulations)
        << ", move confidence = " << ((float)simulations / root_simulations) << std::endl;

    std::cerr << "DEBUG: iters = " << root_simulations << ", memory used (MB) = " << (this->arena.used_bytes() / 1024 / 1024) << std::endl;

    this->arena.clear();

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
        MCTSNode* selected_node = MCTS::select(root);

        // Expansion phase
        MCTSNode* expanded_node = MCTS::expand(selected_node, arena, prng);

        // Simulation phase
        GameResult playout_result = MCTS::playout(expanded_node, prng);

        // Backpropagation phase
        MCTS::backup(expanded_node, playout_result);

        iter++;
    }
}

MCTSNode* MCTS::select(MCTSNode* root) {
    MCTSNode* current = root;

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
    }

    return current;
}

MCTSNode* MCTS::expand(MCTSNode* node, ArenaAllocator& arena, XoshiroCpp::Xoshiro256StarStar& prng) {
    MCTSNode* result = node;

    if (node->board_state.get_next_action() != NextAction::Done) {
        node->create_children(arena, prng);
        result = node->add_child();
    }

    return result;
}

GameResult MCTS::playout(MCTSNode* node, XoshiroCpp::Xoshiro256StarStar& prng) {
    auto board_state_copy = node->board_state;

    if (board_state_copy.get_next_action() == NextAction::Done) {
        return board_state_copy.game_result();
    }

    board_state_copy.playout(prng);

    return board_state_copy.game_result();
}

void MCTS::backup(MCTSNode* from, GameResult playout_result) {
    MCTSNode* propagation_current = from;
    while (propagation_current->parent) {
        uint32_t half_wins = 0;

        if (playout_result == GameResult::Draw) {
            half_wins = 1;
        } else {
            const auto node_color = propagation_current->parent->board_state.whose_move();

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

void MCTS::apply_move(Move move) {
    this->board_state.apply_move(move);
}

void MCTS::set_board(BoardState board) {
    this->board_state = board;
}

BoardState MCTS::get_board() const {
    return this->board_state;
}

}
