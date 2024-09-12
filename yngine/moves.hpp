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

// We need this for a very rare situation where current player cannot make any
// legal move with their ring. That case is not mentioned in the official rules,
// but the author of the game clarified that that player should pass their move
struct PassMove {
};

using Move = std::variant<
    PlaceRingMove,
    RingMove,
    RemoveRowMove,
    RemoveRingMove,
    PassMove
>;

constexpr std::size_t MOVE_LIST_NUMBER = 128;

class MoveList {
public:
    std::size_t get_size() const;
    void append(Move move);
    void reset();

    Move& operator[](std::size_t index);

private:
    std::size_t size = 0;
    std::array<Move, MOVE_LIST_NUMBER> moves;
};

}

#endif // YNGINE_MOVES_HPP
