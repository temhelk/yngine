#ifndef YNGINE_COMMON_HPP
#define YNGINE_COMMON_HPP

#include <cassert>
#include <cstdint>
#include <utility>

namespace Yngine {

template<class... Ts>
struct variant_overloaded : Ts... { using Ts::operator()...; };

using Vec2 = std::pair<uint8_t, uint8_t>;

enum class GameResult {
    Draw,
    WhiteWon,
    BlackWon,
};

enum class Color : uint8_t {
    White,
    Black,
};

inline Color opposite(Color color) {
    if (color == Color::White)
        return Color::Black;
    else
        return Color::White;
}

enum class Direction : uint8_t {
    SE = 0,
    NE = 1,
    N  = 2,
    NW = 3,
    SW = 4,
    S  = 5,
};

inline Direction opposite(Direction direction) {
    switch (direction) {
    case Direction::SE:
        return Direction::NW;
    case Direction::NE:
        return Direction::SW;
    case Direction::N:
        return Direction::S;
    case Direction::NW:
        return Direction::SE;
    case Direction::SW:
        return Direction::NE;
    case Direction::S:
        return Direction::N;
    default:
        assert(false);
    }
}

inline Vec2 direction_to_vec2[6] = {
    std::make_pair( 1,  0),
    std::make_pair( 0,  1),
    std::make_pair(-1,  1),
    std::make_pair(-1,  0),
    std::make_pair( 0, -1),
    std::make_pair( 1, -1),
};

inline bool do_bits_increase_in_direction(Direction dir) {
    switch (dir) {
    case Direction::SE:
    case Direction::NE:
    case Direction::N:
        return true;
    case Direction::NW:
    case Direction::SW:
    case Direction::S:
        return false;

    default:
        assert(false);
    }
}

}

#endif // YNGINE_COMMON_HPP
