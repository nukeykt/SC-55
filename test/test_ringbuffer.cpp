#include <catch2/catch_test_macros.hpp>
#include "ringbuffer.h"

TEST_CASE("RingbufferView")
{
    GenericBuffer storage;
    REQUIRE(storage.Init(4));

    // test write head to the right of the read head
    RingbufferView<uint8_t> ringbuffer(storage);
    REQUIRE(ringbuffer.GetReadableCount() == 0);
    REQUIRE(ringbuffer.GetWritableCount() == 3);
    ringbuffer.UncheckedWriteOne(1);
    REQUIRE(ringbuffer.GetReadableCount() == 1);
    REQUIRE(ringbuffer.GetWritableCount() == 2);
    ringbuffer.UncheckedWriteOne(2);
    ringbuffer.UncheckedWriteOne(3);
    REQUIRE(ringbuffer.GetReadableCount() == 3);
    REQUIRE(ringbuffer.GetWritableCount() == 0);

    uint8_t x = 0;
    ringbuffer.UncheckedReadOne(x);
    REQUIRE(x == 1);
    REQUIRE(ringbuffer.GetReadableCount() == 2);
    REQUIRE(ringbuffer.GetWritableCount() == 1);
    ringbuffer.UncheckedReadOne(x);
    REQUIRE(x == 2);
    REQUIRE(ringbuffer.GetReadableCount() == 1);
    REQUIRE(ringbuffer.GetWritableCount() == 2);
    ringbuffer.UncheckedReadOne(x);
    REQUIRE(x == 3);
    REQUIRE(ringbuffer.GetReadableCount() == 0);
    REQUIRE(ringbuffer.GetWritableCount() == 3);

    // test write head to the left of the read head
    ringbuffer.UncheckedWriteOne(1); // w/r index 0
    ringbuffer.UncheckedReadOne(x);
    ringbuffer.UncheckedWriteOne(2); // w/r index 1
    ringbuffer.UncheckedReadOne(x);
    ringbuffer.UncheckedWriteOne(3); // w/r index 2
    ringbuffer.UncheckedReadOne(x);
    ringbuffer.UncheckedWriteOne(4); // w index 3
    ringbuffer.UncheckedWriteOne(5); // w index 0
    REQUIRE(ringbuffer.GetReadableCount() == 2);
    REQUIRE(ringbuffer.GetWritableCount() == 1);
    ringbuffer.UncheckedReadOne(x); // r index 3 with w at 1
    REQUIRE(x == 4);
    ringbuffer.UncheckedReadOne(x); // r index 0 with w at 1
    REQUIRE(x == 5);

    storage.Free();
}
