#include <yngine/mcts.hpp>

#include <limits>
#include <numbers>
#include <cmath>
#include <random>
#include <iostream>

namespace Yngine {

float MCTSNode::compute_uct() const {
    if (this->simulations == 0) {
        return std::numeric_limits<float>::infinity();
    }

    // @TODO: find the best value for this, or maybe let users change it
    // with it being 100 we won without letting our opponent complete any rows
    // while otherwise we usually win with one ring difference
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

// @TODO: don't hardcode the arena capacity
MCTS::MCTS()
    : arena{17'179'869'184} {
    std::random_device rd;
    this->xoshiro = XoshiroCpp::Xoshiro256StarStar((static_cast<uint64_t>(rd()) << 32) | rd());
}

Move MCTS::search(int iter_num) {
    this->root = this->arena.allocate<MCTSNode>(
        MCTSNode{ .board_state = this->board_state }
    );

    for (int iter = 0; iter < iter_num; iter++) {
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
        if (!we_won) {
            this->root->half_wins += 2;
        }
    }

    // @TODO: make sure we made at least one iteration / or handle nullptr for most sims node
    // Find the most visited node
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

    std::cout << "DEBUG: win rate = "
        << ((float)most_simulations_node->half_wins / 2 / most_simulations_node->simulations)
        << ", move confidence = " << ((float)most_simulations_node->simulations / iter_num) << std::endl;

    this->root = nullptr;
    this->arena.clear();

    return best_move;
}

void MCTS::apply_move(Move move) {
    this->board_state.apply_move(move);
}

}
