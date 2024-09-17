#ifndef YNGINE_ALLOCATORS_HPP
#define YNGINE_ALLOCATORS_HPP

// @TODO: add a pool allocator?

#include <cstdint>
#include <type_traits>
#include <utility>
#include <atomic>

namespace Yngine {

class ArenaAllocator {
public:
    ArenaAllocator(std::size_t capacity);
    ~ArenaAllocator();

    ArenaAllocator(const ArenaAllocator &) = delete;
    ArenaAllocator(ArenaAllocator &&) = delete;
    ArenaAllocator &operator=(const ArenaAllocator &) = delete;
    ArenaAllocator &operator=(ArenaAllocator &&) = delete;

    uint8_t* allocate_aligned(std::size_t size, std::size_t alignment);
    void clear();

    std::size_t used_bytes() const;
    std::size_t capacity_bytes() const;

    // Returns the memory for the object without initialization
    template<typename T>
    T* allocate_raw() {
        static_assert(std::is_trivially_destructible_v<T>);

        return static_cast<T*>(this->allocate_aligned(sizeof(T), std::alignment_of_v<T>));
    }

    // Allocates the object and initializes it with forwarded arguments
    template<typename T, typename... Args>
    T* allocate(Args&&... args) {
        static_assert(std::is_trivially_destructible_v<T>);

        void* ptr = this->allocate_aligned(sizeof(T), std::alignment_of_v<T>);
        if (!ptr) {
            return nullptr;
        }

        T* result = new (ptr) T(std::forward<Args>(args)...);

        return result;
    }

private:
    uint8_t* data;
    std::atomic<std::size_t> used;
    std::size_t capacity;
};

}

#endif // YNGINE_ALLOCATORS_HPP
