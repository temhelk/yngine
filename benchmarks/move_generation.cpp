#include <yngine/board_state.hpp>

#include <iostream>
#include <chrono>

int main() {
    XoshiroCpp::Xoshiro256StarStar prng{0};

    const auto start = std::chrono::high_resolution_clock::now();

    const int number_of_playouts = 100'000;
    for (int i = 0; i < number_of_playouts; i++) {
        Yngine::BoardState board_state;
        board_state.playout(prng);
    }

    const auto end = std::chrono::high_resolution_clock::now();

    const std::chrono::duration<double> diff = end - start;

    std::cout << (number_of_playouts / diff.count()) << std::endl;

    return 0;
}
