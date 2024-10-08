#include <yngine/allocators.hpp>

#include <iostream>

#if defined(__linux__) || defined(EMSCRIPTEN)
#include <sys/mman.h>
#elif defined(_WIN32)
#include <Windows.h>
#else
static_assert(false);
#endif

namespace Yngine {

ArenaAllocator::ArenaAllocator(std::size_t capacity)
    : capacity{capacity}
    , used{0} {
#if defined(__linux__) || defined(EMSCRIPTEN)
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

uint8_t* ArenaAllocator::allocate_aligned(std::size_t bytes, std::size_t alignment) {
    std::size_t used_expected = this->used.load();
    std::size_t aligned_used;
    std::size_t used_desired;

    do {
        aligned_used = (used_expected + (alignment - 1)) & -alignment;

        if (this->capacity - aligned_used < bytes)
            return nullptr;

        used_desired = aligned_used + bytes;
    } while (!this->used.compare_exchange_weak(used_expected, used_desired));

    return this->data + aligned_used;
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

}
