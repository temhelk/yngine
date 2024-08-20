#ifndef YNGINE_BITBOARD_HPP
#define YNGINE_BITBOARD_HPP

#include <yngine/common.hpp>

#include <cstdint>
#include <ostream>

namespace Yngine {

class Bitboard {
public:
    Bitboard();
    Bitboard(__uint128_t n);

    operator bool() const;

    Bitboard operator~() const;
    Bitboard operator|(Bitboard rhs) const;
    Bitboard& operator|=(Bitboard rhs);
    Bitboard operator&(Bitboard rhs) const;
    Bitboard& operator&=(Bitboard rhs);

    uint8_t bit_scan() const;
    uint8_t bit_scan_direction(Direction direction) const;
    uint8_t bit_scan_reverse() const;
    uint8_t bit_scan_and_reset();
    uint8_t bit_scan_and_reset_reverse();
    uint8_t popcount() const;

    void shift_in_direction(Direction dir);

    bool get_bit(uint8_t index) const;
    void set_bit(uint8_t index);
    void clear_bit(uint8_t index);

    __uint128_t get_bits() const;

    static Bitboard get_game_board();

    static bool is_index_in_game(uint8_t index);
    static bool are_coords_in_game(uint8_t x, uint8_t y);

    static uint8_t coords_to_index(uint8_t x, uint8_t y);
    static Vec2 index_to_coords(uint8_t index);

    static uint8_t index_move_direction(uint8_t index, Direction direction, uint8_t times);

    friend std::ostream& operator<<(std::ostream& out, Bitboard bb);

private:
    __uint128_t bits = 0;
};

}

#endif // YNGINE_BITBOARD_HPP
