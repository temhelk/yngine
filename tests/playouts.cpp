#include <yngine/board_state.hpp>

#include <iostream>
#include <random>

int main() {
    int draws = 0;
    int white_wins = 0;
    int black_wins = 0;
    int total_moves = 0;

    Yngine::MoveList move_list{};

    std::mt19937 prng{1337};

    for (int i = 0; i < 1000; i++) {
        Yngine::BoardState board{};

        int moves = 0;
        while (board.get_next_action() != Yngine::NextAction::Done) {
            board.generate_moves(move_list);
            moves++;

            std::uniform_int_distribution<size_t> dist{0, move_list.get_size() - 1};
            const auto move = move_list[dist(prng)];

            board.apply_move(move);

            move_list.reset();
        }
        total_moves += moves;

        switch (board.game_result()) {
        case Yngine::GameResult::Draw: {
            draws++;
        } break;
        case Yngine::GameResult::WhiteWon: {
            white_wins++;
        } break;
        case Yngine::GameResult::BlackWon: {
            black_wins++;
        } break;
        }
    }

    std::cout << "Draws:       " << draws << std::endl;
    std::cout << "White wins:  " << white_wins << std::endl;
    std::cout << "Black wins:  " << black_wins << std::endl;
    std::cout << "Total moves: " << total_moves << std::endl;

    std::cout << "Moves per game: " << (total_moves / 100'000.0) << std::endl;

    if (draws != 378 ||
        white_wins != 306 ||
        black_wins != 316 ||
        total_moves != 71608) {
        return 1;
    }

    return 0;
}
