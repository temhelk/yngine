#ifndef YNGINE_ALLOCATORS_HPP
#define YNGINE_ALLOCATORS_HPP

#include <cstdint>
#include <type_traits>
#include <utility>
#include <atomic>
#include <cstring>

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

template<typename T>
class PoolAllocator {
public:
    struct Node : public T {
        using T::T;

        Node* prev_free_node = nullptr;
    };

    PoolAllocator(std::size_t capacity)
        : arena{capacity}
        , last_free_node{nullptr} {
    }

    template<typename... Args>
    T* allocate(Args&&... args) {
        Node* expected = this->last_free_node.load();
        Node* desired;

        do {
            if (expected == nullptr) {
                return this->arena.template allocate<Node>(std::forward<Args>(args)...);
            }

            desired = expected->prev_free_node;
        } while (!this->last_free_node.compare_exchange_weak(expected, desired));


        T* result = new (expected) T(std::forward<Args>(args)...);

        return result;
    }

    void free(T* ptr) {
        assert(ptr);
#ifdef DEBUG
        memset(ptr, 0, sizeof(T));
#endif

        Node* expected = this->last_free_node.load();
        Node* desired;

        do {
            Node* node = static_cast<Node*>(ptr);
            node->prev_free_node = expected;

            desired = node;
        } while (!this->last_free_node.compare_exchange_weak(expected, desired));
    }

    void clear() {
        this->last_free_node.store(nullptr);
        this->arena.clear();
    }

    std::size_t used_bytes() const {
        return this->arena.used_bytes();
    }

private:
    ArenaAllocator arena;
    std::atomic<Node*> last_free_node;
};

}

#endif // YNGINE_ALLOCATORS_HPP
