#ifndef YNGINE_BOARD_STATE_HPP
#define YNGINE_BOARD_STATE_HPP

#include <yngine/bitboard.hpp>
#include <yngine/moves.hpp>

#include <XoshiroCpp.hpp>

#include <optional>

namespace Yngine {

enum class NextAction : uint8_t {
    RingPlacement,
    RingMovement,
    RowRemoval,
    RingRemoval,
    Done,
};

class BoardState {
public:
    BoardState();

    // MoveList should be empty before calling this function
    void generate_moves(MoveList& move_list) const;
    void apply_move(Move move);

    void playout(XoshiroCpp::Xoshiro256StarStar& prng);

    NextAction get_next_action() const;
    GameResult game_result() const;
    Color whose_move() const;

    friend std::ostream& operator<<(std::ostream& out, BoardState board_state);

private:
    void generate_ring_placement_moves(MoveList& move_list) const;
    void generate_ring_moves(MoveList& move_list) const;
    void generate_row_removal(MoveList& move_list) const;
    void generate_ring_removal(MoveList& move_list) const;
    std::optional<Color> check_rows(RingMove last_move) const;

    static uint8_t length_of_row(Bitboard bitboard, uint8_t index, Direction direction);
    static Bitboard line_in_direction(uint8_t index, Direction direction, uint8_t length);

    NextAction next_action;
    Color ring_and_row_removal_color;
    Color last_ring_move_color;

    // If from index is 0 then no move was made before
    RingMove last_ring_move;

    Bitboard white_rings;
    Bitboard black_rings;

    Bitboard white_markers;
    Bitboard black_markers;
};

}

#endif // YNGINE_BOARD_STATE_HPP
