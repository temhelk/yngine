#include <yngine/allocators.hpp>

#include <iostream>

#if defined(__linux__)
#include <sys/mman.h>
#elif defined(_WIN32)
#include <Windows.h>
#endif

namespace Yngine {

ArenaAllocator::ArenaAllocator(std::size_t capacity)
    : capacity{capacity}
    , used{0} {
#if defined(__linux__)
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
#elif defined(_WIN32)
    const auto data = VirtualAlloc(
        nullptr,
        capacity,
        MEM_COMMIT | MEM_RESERVE,
        PAGE_READWRITE
    );

    if (data == nullptr) {
        std::cerr << "Failed to allocate an arena using VirtualAlloc" << std::endl;
        std::abort();
    }
#endif

    this->data = static_cast<uint8_t*>(data);
}

ArenaAllocator::~ArenaAllocator() {
#if defined(__linux__)
    munmap(this->data, this->capacity);
#elif defined(_WIN32)
    VirtualFree(this->data, 0, MEM_RELEASE);
#endif
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
