#include <yngine/moves.hpp>
#include <yngine/board_state.hpp>

#include <cassert>

namespace Yngine {

bool RemoveRowMove::operator==(const RemoveRowMove& rhs) const {
    if (this->from      == rhs.from &&
        this->direction == rhs.direction) {
        return true;
    }

    const auto alternative_index =
        Bitboard::index_move_direction(this->from, this->direction, 4);

    if (alternative_index         == rhs.from &&
        opposite(this->direction) == rhs.direction) {
        return true;
    }

    return false;
}

std::size_t MoveList::get_size() const {
    return this->size;
}

void MoveList::append(Move move) {
    assert(this->size < MOVE_LIST_NUMBER);
    this->moves[this->size++] = move;
}

Move& MoveList::operator[](std::size_t index) {
    return this->moves[index];
}

Move MoveList::operator[](std::size_t index) const {
    return this->moves[index];
}

Move MoveList::get_random(XoshiroCpp::Xoshiro256StarStar& prng) const {
    // Biased distribution from [0 to move_list_size)
    const auto rand_32 = static_cast<uint32_t>(prng());
    auto rand_move_index =
        static_cast<uint64_t>(rand_32) * static_cast<uint64_t>(this->get_size());
    rand_move_index >>= 32;

    assert(rand_move_index < this->get_size());

    return (*this)[rand_move_index];
}

void MoveList::reset() {
    this->size = 0;
}

}
