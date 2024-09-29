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

void MoveList::reset() {
    this->size = 0;
}

}
