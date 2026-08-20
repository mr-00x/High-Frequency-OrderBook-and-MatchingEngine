#pragma once

#include <cstddef>
#include <cassert>
#include <vector>
#include <cstdint>

namespace hft {

// ---------------------------------------------------------------------------
// MemoryPool<T>
//
// A fixed-size slab allocator for objects of type T.
// Avoids new/delete overhead on the hot matching path.
//
// Design:
//   - Pre-allocates `capacity` raw blocks up front.
//   - Hands out blocks via a free-list (intrusive singly-linked list stored
//     in the block memory itself while free).
//   - allocate() is O(1) pop from free-list.
//   - deallocate() is O(1) push onto free-list.
//   - NOT thread-safe by itself; callers must hold a lock if sharing.
// ---------------------------------------------------------------------------

template<typename T>
class MemoryPool {
public:
    explicit MemoryPool(std::size_t capacity)
        : capacity_(capacity)
        , allocated_(0)
    {
        static_assert(sizeof(T) >= sizeof(void*),
            "Object too small to store free-list pointer");

        // Allocate one large slab
        slab_.resize(capacity);

        // Build the free-list: each block's first bytes store a pointer to the next free block
        free_head_ = &slab_[0];
        for (std::size_t i = 0; i + 1 < capacity; ++i) {
            // Store next pointer in the unused memory of the current slot
            *reinterpret_cast<void**>(&slab_[i]) = &slab_[i + 1];
        }
        *reinterpret_cast<void**>(&slab_[capacity - 1]) = nullptr;
    }

    // Non-copyable, non-movable (addresses must remain stable)
    MemoryPool(const MemoryPool&)            = delete;
    MemoryPool& operator=(const MemoryPool&) = delete;
    MemoryPool(MemoryPool&&)                 = delete;
    MemoryPool& operator=(MemoryPool&&)      = delete;

    // Allocate one T-sized block; returns nullptr if pool is exhausted
    [[nodiscard]] T* allocate() noexcept {
        if (free_head_ == nullptr) {
            return nullptr; // pool exhausted
        }
        void* block = free_head_;
        free_head_ = *reinterpret_cast<void**>(block);
        ++allocated_;
        return static_cast<T*>(block);
    }

    // Return a block to the pool (does NOT call destructor — caller must destruct first)
    void deallocate(T* ptr) noexcept {
        assert(ptr != nullptr);
        // Store next free pointer in returned block
        *reinterpret_cast<void**>(ptr) = free_head_;
        free_head_ = ptr;
        --allocated_;
    }

    [[nodiscard]] std::size_t capacity()  const noexcept { return capacity_; }
    [[nodiscard]] std::size_t allocated() const noexcept { return allocated_; }
    [[nodiscard]] std::size_t available() const noexcept { return capacity_ - allocated_; }
    [[nodiscard]] bool        empty()     const noexcept { return allocated_ == 0; }
    [[nodiscard]] bool        full()      const noexcept { return free_head_ == nullptr; }

private:
    std::size_t  capacity_;
    std::size_t  allocated_;
    void*        free_head_;
    std::vector<T> slab_;   // backing storage (keeps pointers stable)
};

} // namespace hft
