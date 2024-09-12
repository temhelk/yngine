#include <yngine/moves.hpp>

#include <cassert>

namespace Yngine {

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
