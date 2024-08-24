#include <yngine/bitboard.hpp>

#include <cassert>
#include <ostream>

namespace Yngine {

Bitboard::Bitboard() : bits{} {}

Bitboard::Bitboard(__uint128_t n) : bits{n} {}

Bitboard::operator bool() const {
    return this->bits != 0;
}

Bitboard Bitboard::operator~() const {
    return Bitboard{~this->bits};
}

Bitboard Bitboard::operator|(Bitboard rhs) const {
    return Bitboard{this->bits | rhs.bits};
}

Bitboard& Bitboard::operator|=(Bitboard rhs) {
    this->bits |= rhs.bits;
    return *this;
}

Bitboard Bitboard::operator&(Bitboard rhs) const {
    return Bitboard{this->bits & rhs.bits};
}

Bitboard& Bitboard::operator&=(Bitboard rhs) {
    this->bits &= rhs.bits;
    return *this;
}

uint8_t Bitboard::bit_scan() const {
    assert(this->bits != 0);

    const uint64_t low  = this->bits;
    const uint64_t high = this->bits >> 64;

    uint8_t results[2] = {
        static_cast<uint8_t>(std::countr_zero(low)),
        static_cast<uint8_t>(std::countr_zero(high) + 64),
    };

    const uint32_t result_index = !low;

    return results[result_index];
}

uint8_t Bitboard::bit_scan_direction(Direction direction) const {
    uint8_t index;
    if (do_bits_increase_in_direction(direction)) {
        index = this->bit_scan();
    } else {
        index = this->bit_scan_reverse();
    }
    return index;
}

uint8_t Bitboard::bit_scan_reverse() const {
    assert(this->bits != 0);

    const uint64_t low  = this->bits;
    const uint64_t high = this->bits >> 64;

    uint8_t results[2] = {
        static_cast<uint8_t>(63 - std::countl_zero(low)),
        static_cast<uint8_t>(127 - std::countl_zero(high)),
    };

    const uint32_t result_index = (high != 0);

    return results[result_index];
}

uint8_t Bitboard::bit_scan_and_reset() {
    assert(this->bits != 0);

    const uint64_t low  = this->bits;
    const uint64_t high = this->bits >> 64;

    uint8_t results[2] = {
        static_cast<uint8_t>(std::countr_zero(low)),
        static_cast<uint8_t>(std::countr_zero(high) + 64),
    };

    const uint32_t result_index = !low;

    this->bits &= this->bits - 1;

    return results[result_index];
}

uint8_t Bitboard::bit_scan_and_reset_reverse() {
    assert(this->bits != 0);

    const uint64_t low  = this->bits;
    const uint64_t high = this->bits >> 64;

    uint8_t results[2] = {
        static_cast<uint8_t>(63 - std::countl_zero(low)),
        static_cast<uint8_t>(127 - std::countl_zero(high)),
    };

    const uint32_t result_index = (high != 0);
    const uint8_t result = results[result_index];

    this->clear_bit(result);

    return result;
}

uint8_t Bitboard::popcount() const {
    const uint64_t low  = this->bits;
    const uint64_t high = this->bits >> 64;

    return std::popcount(low) + std::popcount(high);
}

void Bitboard::shift_in_direction(Direction dir) {
    switch (dir) {
    case Direction::SE: {
        this->bits <<= 1;
    } break;
    case Direction::NE: {
        this->bits <<= 11;
    } break;
    case Direction::N: {
        this->bits <<= 10;
    } break;
    case Direction::NW: {
        this->bits >>= 1;
    } break;
    case Direction::SW: {
        this->bits >>= 11;
    } break;
    case Direction::S: {
        this->bits >>= 10;
    } break;
    }
}

bool Bitboard::get_bit(uint8_t index) const {
    assert(index < 11*11);
    return this->bits & ((__uint128_t)(1) << index);
}

void Bitboard::set_bit(uint8_t index) {
    this->bits |= ((__uint128_t)1 << index);
}

void Bitboard::clear_bit(uint8_t index) {
    this->bits &= ~((__uint128_t)1 << index);
}

__uint128_t Bitboard::get_bits() const {
    return this->bits;
}

Bitboard Bitboard::get_game_board() {
    return Bitboard{((__uint128_t)0x783F8FF3FEFFD << 64) | 0xFF7FEFF9FE3F83C0};
}

bool Bitboard::is_index_in_game(uint8_t index) {
    if (index >= 128) {
        return false;
    }

    const auto game_board = Bitboard::get_game_board();
    return game_board.get_bit(index);
}

bool Bitboard::are_coords_in_game(uint8_t x, uint8_t y) {
    if (x >= 11 || y >= 11) {
        return false;
    }

    const auto game_board = Bitboard::get_game_board();
    return game_board.get_bit(Bitboard::coords_to_index(x, y));
}

uint8_t Bitboard::coords_to_index(uint8_t x, uint8_t y) {
    assert(x < 11 && y < 11);
    return 11 * y + x;
}

std::pair<uint8_t, uint8_t> Bitboard::index_to_coords(uint8_t index) {
    return std::make_pair(index % 11, index / 11);
}

uint8_t Bitboard::index_move_direction(uint8_t index, Direction direction, uint8_t times) {
    switch (direction) {
    case Direction::SE:
        return index + times;
    case Direction::NE:
        return index + 11 * times;
    case Direction::N:
        return index + 10 * times;
    case Direction::NW:
        return index - times;
    case Direction::SW:
        return index - 11 * times;
    case Direction::S:
        return index - 10 * times;
    default:
        abort();
    }
}

std::ostream& operator<<(std::ostream& out, Bitboard bb) {
    const auto game_board = Bitboard::get_game_board();

    for (int y = 10; y >= 0; y--) {
        for (int t = 0; t < y; t++)
            out << "    ";

        const auto diagonal_length = 11 - y;

        for (int n = 0; n < diagonal_length; n++) {
            if (game_board.get_bit(Bitboard::coords_to_index(n, y + n)))
                out << bb.get_bit(Bitboard::coords_to_index(n, y + n)) << "       ";
            else
                out << " " << "       ";
        }

        out << "\n";
    }

    for (int x = 1; x < 11; x++) {
        for (int t = 0; t < x; t++)
            out << "    ";

        const auto diagonal_length = 11 - x;

        for (int n = 0; n < diagonal_length; n++) {
            if (game_board.get_bit(Bitboard::coords_to_index(x + n, n)))
                out << bb.get_bit(Bitboard::coords_to_index(x + n, n)) << "       ";
            else
                out << " " << "       ";
        }

        out << "\n";
    }

    return out;
}

}
