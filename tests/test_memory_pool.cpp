// test_memory_pool.cpp
//
// Unit tests for the MemoryPool<T> slab allocator.
// Uses doctest (single-header, no external deps).

#define TEST_MAIN
#include "test_framework.hpp"
#include "memory_pool.hpp"
#include "order.hpp"

using namespace hft;

TEST_CASE("MemoryPool basic allocation and deallocation") {
    MemoryPool<Order> pool(8);

    CHECK(pool.capacity()  == 8);
    CHECK(pool.allocated() == 0);
    CHECK(pool.available() == 8);
    CHECK(!pool.full());
    CHECK(pool.empty());

    Order* o1 = pool.allocate();
    REQUIRE(o1 != nullptr);
    CHECK(pool.allocated() == 1);
    CHECK(pool.available() == 7);

    Order* o2 = pool.allocate();
    REQUIRE(o2 != nullptr);
    CHECK(o1 != o2); // must return distinct blocks

    pool.deallocate(o1);
    CHECK(pool.allocated() == 1);
    CHECK(pool.available() == 7);

    pool.deallocate(o2);
    CHECK(pool.allocated() == 0);
    CHECK(pool.available() == 8);
    CHECK(pool.empty());
}

TEST_CASE("MemoryPool exhaustion returns nullptr") {
    MemoryPool<Order> pool(2);

    Order* a = pool.allocate();
    Order* b = pool.allocate();
    REQUIRE(a != nullptr);
    REQUIRE(b != nullptr);

    Order* c = pool.allocate(); // should fail — pool is full
    CHECK(c == nullptr);
    CHECK(pool.full());

    pool.deallocate(a);
    Order* d = pool.allocate(); // should succeed after freeing one slot
    REQUIRE(d != nullptr);
    CHECK(pool.full());

    pool.deallocate(b);
    pool.deallocate(d);
}

TEST_CASE("MemoryPool recycles freed blocks") {
    MemoryPool<Order> pool(4);

    std::vector<Order*> ptrs;
    for (int i = 0; i < 4; ++i) {
        ptrs.push_back(pool.allocate());
    }
    CHECK(pool.full());

    // Free all and re-allocate — no nullptr expected
    for (auto* p : ptrs) pool.deallocate(p);
    CHECK(pool.empty());

    for (int i = 0; i < 4; ++i) {
        CHECK(pool.allocate() != nullptr);
    }
    CHECK(pool.full());
}

TEST_CASE("MemoryPool addresses are stable (no realloc)") {
    MemoryPool<Order> pool(4);

    Order* a = pool.allocate();
    Order* b = pool.allocate();

    // Write something into a and b, free them, reallocate — data in OTHER slot unaffected
    // (verifies the slab isn't moving in memory)
    a->id = 42;
    b->id = 99;

    pool.deallocate(a);
    Order* c = pool.allocate(); // likely the same slot as a
    c->id = 7;

    CHECK(b->id == 99); // b must be untouched

    pool.deallocate(b);
    pool.deallocate(c);
}
