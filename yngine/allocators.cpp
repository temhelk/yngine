#include <yngine/allocators.hpp>

#include <iostream>

#include <sys/mman.h>

namespace Yngine {

ArenaAllocator::ArenaAllocator(std::size_t capacity)
    : capacity{capacity}
    , used{0} {
    const auto data = mmap(
        nullptr,
        capacity,
        PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS,
        -1,
        0
    );

    if (data == MAP_FAILED) {
        std::cerr << "Failed to allocate an arena using mmap" << std::endl;
        std::abort();
    }

    this->data = static_cast<uint8_t*>(data);
}

ArenaAllocator::~ArenaAllocator() {
    munmap(this->data, this->capacity);
}


uint8_t* ArenaAllocator::allocate_bytes(std::size_t bytes) {
    if (this->used + bytes > this->capacity) {
        return nullptr;
    }

    const auto result = this->data + this->used;
    this->used += bytes;

    return result;
}

void ArenaAllocator::clear() {
    this->used = 0;
}

std::size_t ArenaAllocator::used_bytes() const {
    return this->used;
}

std::size_t ArenaAllocator::capacity_bytes() const {
    return this->capacity;
}

std::size_t ArenaAllocator::left_bytes() const {
    return this->capacity - this->used;
}

}
