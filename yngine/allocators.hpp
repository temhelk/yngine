#ifndef YNGINE_ALLOCATORS_HPP
#define YNGINE_ALLOCATORS_HPP

// @TODO: add a pool allocator?
// @TODO: write a Windows version
// @TODO: memory usage tracking

#include <cstdint>
#include <type_traits>
#include <utility>

namespace Yngine {

class ArenaAllocator {
public:
    ArenaAllocator(std::size_t capacity);
    ~ArenaAllocator();

    ArenaAllocator(const ArenaAllocator &) = delete;
    ArenaAllocator(ArenaAllocator &&) = delete;
    ArenaAllocator &operator=(const ArenaAllocator &) = delete;
    ArenaAllocator &operator=(ArenaAllocator &&) = delete;

    uint8_t* allocate_bytes(std::size_t bytes);
    void clear();

    // Returns the memory for the object without initialization
    template<typename T>
    T* allocate_raw() {
        static_assert(std::is_trivially_destructible_v<T>);

        return static_cast<T*>(this->allocate_bytes(sizeof(T)));
    }

    // Allocates the object and initializes it with forwarded arguments
    template<typename T, typename... Args>
    T* allocate(Args&&... args) {
        static_assert(std::is_trivially_destructible_v<T>);

        void* ptr = this->allocate_bytes(sizeof(T));
        T* result = new (ptr) T(std::forward<Args>(args)...);

        return result;
    }

private:
    uint8_t* data;
    std::size_t used;
    std::size_t capacity;
};

}

#endif // YNGINE_ALLOCATORS_HPP
