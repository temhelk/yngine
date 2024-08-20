#ifndef YNGINE_MOVES_HPP
#define YNGINE_MOVES_HPP

#include <yngine/common.hpp>

#include <cstdint>
#include <array>
#include <variant>

namespace Yngine {

struct PlaceRingMove {
    uint8_t index;
};

struct RingMove {
    uint8_t from;
    uint8_t to;
    Direction direction;
};

struct RemoveRowMove {
    uint8_t from;
    Direction direction;
};

struct RemoveRingMove {
    uint8_t index;
};

using Move = std::variant<
    PlaceRingMove,
    RingMove,
    RemoveRowMove,
    RemoveRingMove
>;

constexpr std::size_t MOVE_LIST_NUMBER = 128;

class MoveList {
public:
    std::size_t get_size() const;
    void append(Move move);
    void reset();

    Move operator[](std::size_t index) const;

private:
    std::size_t size = 0;
    std::array<Move, MOVE_LIST_NUMBER> moves;
};

}

#endif // YNGINE_MOVES_HPP
