#ifndef YNGINE_MOVES_HPP
#define YNGINE_MOVES_HPP

#include <yngine/common.hpp>

#include <XoshiroCpp.hpp>

#include <cstdint>
#include <array>
#include <variant>

namespace Yngine {

struct PlaceRingMove {
    uint8_t index;

    bool operator==(const PlaceRingMove&) const = default;
};

struct RingMove {
    uint8_t from;
    uint8_t to;
    Direction direction;

    bool operator==(const RingMove&) const = default;
};

struct RemoveRowMove {
    uint8_t from;
    Direction direction;

    bool operator==(const RemoveRowMove&) const;
};

struct RemoveRingMove {
    uint8_t index;

    bool operator==(const RemoveRingMove&) const = default;
};

// We need this for a very rare situation where the current player cannot make any
// legal moves with their rings. That case is not mentioned in the official rules,
// but the author of the game clarified that that player should pass their move
struct PassMove {
    bool operator==(const PassMove&) const = default;
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
    Move get_random(XoshiroCpp::Xoshiro256StarStar& prng) const;
    void append(Move move);
    void reset();

    Move& operator[](std::size_t index);
    Move operator[](std::size_t index) const;

private:
    std::size_t size = 0;
    std::array<Move, MOVE_LIST_NUMBER> moves;
};

}

#endif // YNGINE_MOVES_HPP
