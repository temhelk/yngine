#include <yngine/mcts.hpp>

#include <limits>
#include <numbers>
#include <cmath>
#include <random>
#include <iostream>
#include <chrono>

namespace Yngine {

float MCTSNode::compute_uct() const {
    if (this->simulations == 0) {
        return std::numeric_limits<float>::infinity();
    }

    const float exploration_parameter = std::numbers::sqrt2_v<float>;

    const float exploitation =
        (static_cast<float>(this->half_wins) / 2) /
        static_cast<float>(this->simulations);

    const float exploration =
        exploration_parameter *
        std::sqrt(
            std::log(static_cast<float>(this->parent->simulations)) /
            static_cast<float>(this->simulations)
        );

    return exploitation + exploration;
}

MCTS::MCTS(std::size_t memory_limit_bytes)
    : arena{memory_limit_bytes} {
    std::random_device rd;
    this->xoshiro = XoshiroCpp::Xoshiro256StarStar((static_cast<uint64_t>(rd()) << 32) | rd());
}

MCTS::~MCTS() {
    this->stop_search = true;
    this->search_thread.join();
}

std::future<Move> MCTS::search(float seconds) {
    std::packaged_task<Move(MCTS*, SearchLimit)> task{&MCTS::search_threaded};
    auto future = task.get_future();

    if (this->search_thread.joinable()) {
        this->search_thread.join();
    }
    this->search_thread = std::thread{std::move(task), this, seconds};

    return std::move(future);
}

Move MCTS::search_threaded(SearchLimit limit) {
    this->root = this->arena.allocate<MCTSNode>(
        MCTSNode{ .board_state = this->board_state }
    );

    if (!this->root) {
        abort();
    }

    const auto start_time = std::chrono::steady_clock::now();

    int iter = 0;
    while (!this->stop_search) {
        // Selection phase
        MCTSNode* current = this->root;
        while (current->first_child) {
            MCTSNode* greatest_uct_node = current->first_child;
            float greatest_uct = greatest_uct_node->compute_uct();

            MCTSNode* current_child = current->first_child;
            while (current_child->next_sibling) {
                if (std::isinf(greatest_uct))
                    break;

                current_child = current_child->next_sibling;

                const auto current_uct = current_child->compute_uct();
                if (current_uct > greatest_uct) {
                    greatest_uct = current_uct;
                    greatest_uct_node = current_child;
                }
            }

            current = greatest_uct_node;
        }

        GameResult playout_result;
        MCTSNode* backpropagate_from;
        if (current->simulations != 0) {
            if (current->board_state.get_next_action() != NextAction::Done) {
                // Expansion phase
                MoveList move_list;
                current->board_state.generate_moves(move_list);

                // Shuffle move list to remove biases
                std::shuffle(&move_list[0], &move_list[move_list.get_size()], this->xoshiro);

                // Check if we can allocate enough nodes, and if we can't
                // stop iteration and return the most promising moves as of now
                if (this->arena.left_bytes() < sizeof(MCTSNode) * move_list.get_size()) {
                    break;
                }

                const auto new_first_child = this->arena.allocate<MCTSNode>(MCTSNode{
                    .board_state = current->board_state.with_move(move_list[0]),
                    .parent_move = move_list[0],

                    .parent = current,
                });
                current->first_child = new_first_child;

                MCTSNode* last_child = new_first_child;
                for (int move_index = 1; move_index < move_list.get_size(); move_index++) {
                    const auto move = move_list[move_index];

                    auto new_board_state = current->board_state.with_move(move);

                    const auto new_child = this->arena.allocate<MCTSNode>(MCTSNode{
                        .board_state = new_board_state,
                        .parent_move = move,

                        .parent = current,
                    });

                    last_child->next_sibling = new_child;
                    last_child = new_child;
                }

                auto first_child_board_state = current->first_child->board_state;
                first_child_board_state.playout(this->xoshiro);

                // Simulation phase
                playout_result = first_child_board_state.game_result();
                backpropagate_from = current->first_child;
            } else {
                playout_result = current->board_state.game_result();
                backpropagate_from = current;
            }
        } else {
            auto current_board_state = current->board_state;
            current_board_state.playout(this->xoshiro);

            playout_result = current_board_state.game_result();
            backpropagate_from = current;
        }

        // Backpropagation
        const auto our_color = this->board_state.whose_move();
        const auto we_won =
            playout_result == GameResult::WhiteWon && our_color == Color::White ||
            playout_result == GameResult::BlackWon && our_color == Color::Black;

        auto propagation_current = backpropagate_from;
        while (propagation_current->parent) {
            propagation_current->simulations++;

            if (playout_result == GameResult::Draw) {
                propagation_current->half_wins += 1;
            } else {
                const auto node_color = propagation_current->parent->board_state.whose_move();

                if ( we_won && node_color == our_color ||
                    !we_won && node_color != our_color) {
                    propagation_current->half_wins += 2;
                }
            }

            propagation_current = propagation_current->parent;
        }

        this->root->simulations++;
        if (playout_result == GameResult::Draw) {
            this->root->half_wins += 1;
        } else if (!we_won) {
            this->root->half_wins += 2;
        }

        iter++;

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
    }

    if (!this->root->first_child) {
        std::cerr << "No iterations were performed for some reason, exiting..." << std::endl;
        abort();
    }

    int most_simulations = -1;
    MCTSNode* most_simulations_node = nullptr;

    auto child = this->root->first_child;
    while (true) {
        if (child->simulations > most_simulations) {
            most_simulations = child->simulations;
            most_simulations_node = child;
        }

        if (!child->next_sibling)
            break;

        child = child->next_sibling;
    }

    const auto best_move = most_simulations_node->parent_move;

    std::cerr << "DEBUG: win rate = "
        << ((float)most_simulations_node->half_wins / 2 / most_simulations_node->simulations)
        << ", move confidence = " << ((float)most_simulations_node->simulations / iter) << std::endl;

    std::cerr << "DEBUG: iters = " << iter << ", memory used (MB) = " << (this->arena.used_bytes() / 1024 / 1024) << std::endl;

    this->root = nullptr;
    this->arena.clear();

    return best_move;
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

void MCTS::reseed() {
    std::random_device rd;
    this->xoshiro = XoshiroCpp::Xoshiro256StarStar((static_cast<uint64_t>(rd()) << 32) | rd());
}

}
