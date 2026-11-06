// Copyright (c) 2026 Ryan P. Nicholl <rnicholl@protonmail.com>
// SPDX-License-Identifier: Apache-2.0
#include <rpnx/thin_shared_mutex.hpp>

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <future>
#include <gtest/gtest.h>
#include <latch>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <type_traits>
#include <vector>

static_assert(sizeof(rpnx::detail::basic_thin_shared_mutex< std::uint32_t >) == 4);
static_assert(alignof(rpnx::detail::basic_thin_shared_mutex< std::uint32_t >) == 4);
static_assert(sizeof(rpnx::thin_shared_mutex) <= 8);
static_assert(sizeof(rpnx::thin_shared_mutex) == sizeof(rpnx::detail::thin_shared_mutex_int));
static_assert(!std::is_copy_constructible_v< rpnx::thin_shared_mutex >);
static_assert(!std::is_move_constructible_v< rpnx::thin_shared_mutex >);
static_assert(!std::is_copy_assignable_v< rpnx::thin_shared_mutex >);
static_assert(!std::is_move_assignable_v< rpnx::thin_shared_mutex >);

/** @brief Exercises shared ownership contracts for both compact state widths. */
template < typename Mutex >
class shared_mutex_tests : public testing::Test
{
};

using shared_mutex_types = testing::Types< rpnx::detail::basic_thin_shared_mutex< std::uint32_t >, rpnx::thin_shared_mutex >;
TYPED_TEST_SUITE(shared_mutex_tests, shared_mutex_types);

TYPED_TEST(shared_mutex_tests, shared_and_exclusive_try_acquisition)
{
    TypeParam mutex;
    mutex.lock_shared();
    bool shared_acquired = false;
    bool exclusive_acquired = true;
    std::thread reader(
        [&]
        {
            shared_acquired = mutex.try_lock_shared();
            if (shared_acquired)
            {
                mutex.unlock_shared();
            }
            exclusive_acquired = mutex.try_lock();
            if (exclusive_acquired)
            {
                mutex.unlock();
            }
        });
    reader.join();
    EXPECT_TRUE(shared_acquired);
    EXPECT_FALSE(exclusive_acquired);
    mutex.unlock_shared();

    mutex.lock();
    std::thread contender(
        [&]
        {
            shared_acquired = mutex.try_lock_shared();
            if (shared_acquired)
            {
                mutex.unlock_shared();
            }
            exclusive_acquired = mutex.try_lock();
            if (exclusive_acquired)
            {
                mutex.unlock();
            }
        });
    contender.join();
    EXPECT_FALSE(shared_acquired);
    EXPECT_FALSE(exclusive_acquired);
    mutex.unlock();
    EXPECT_TRUE(mutex.try_lock());
    mutex.unlock();
}

TYPED_TEST(shared_mutex_tests, readers_hold_ownership_together)
{
    TypeParam mutex;
    std::atomic< std::size_t > entered{0};
    std::latch release(1);
    std::vector< std::thread > readers;
    for (std::size_t index = 0; index < 8; ++index)
    {
        readers.emplace_back(
            [&]
            {
                std::shared_lock< TypeParam > lock(mutex);
                entered.fetch_add(1, std::memory_order_relaxed);
                release.wait();
            });
    }
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (entered.load(std::memory_order_relaxed) != readers.size() && std::chrono::steady_clock::now() < deadline)
    {
        std::this_thread::yield();
    }
    EXPECT_EQ(entered.load(std::memory_order_relaxed), readers.size());
    release.count_down();
    for (std::thread& reader : readers)
    {
        reader.join();
    }
}

TYPED_TEST(shared_mutex_tests, writer_reservation_closes_reader_admission)
{
    TypeParam mutex;
    std::shared_lock< TypeParam > original_reader(mutex);
    std::promise< void > writer_entered;
    std::future< void > writer_ready = writer_entered.get_future();
    std::latch release_writer(1);
    std::thread writer(
        [&]
        {
            std::unique_lock< TypeParam > lock(mutex);
            writer_entered.set_value();
            release_writer.wait();
        });
    std::future< bool > reservation = std::async(std::launch::async,
                                                 [&]
                                                 {
                                                     auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
                                                     while (std::chrono::steady_clock::now() < deadline)
                                                     {
                                                         if (!mutex.try_lock_shared())
                                                         {
                                                             return true;
                                                         }
                                                         mutex.unlock_shared();
                                                         std::this_thread::yield();
                                                     }
                                                     return false;
                                                 });
    bool reserved = reservation.get();
    EXPECT_TRUE(reserved);
    std::future< bool > late_reader = std::async(std::launch::async,
                                                 [&]
                                                 {
                                                     std::shared_lock< TypeParam > lock(mutex);
                                                     return true;
                                                 });
    original_reader.unlock();
    EXPECT_EQ(writer_ready.wait_for(std::chrono::seconds(5)), std::future_status::ready);
    EXPECT_EQ(late_reader.wait_for(std::chrono::milliseconds(20)), std::future_status::timeout);
    release_writer.count_down();
    writer.join();
    EXPECT_TRUE(late_reader.get());
}

TYPED_TEST(shared_mutex_tests, readers_observe_complete_writes)
{
    TypeParam mutex;
    std::uint64_t value = 0;
    std::uint64_t checksum = 0;
    std::atomic< bool > inconsistent{false};
    std::latch start(8);
    std::vector< std::thread > threads;
    for (std::size_t index = 0; index < 8; ++index)
    {
        threads.emplace_back(
            [&, index]
            {
                start.arrive_and_wait();
                for (std::size_t iteration = 0; iteration < 2000; ++iteration)
                {
                    if (index % 3 == 0)
                    {
                        std::unique_lock< TypeParam > lock(mutex);
                        ++value;
                        checksum = value * 7;
                    }
                    else
                    {
                        std::shared_lock< TypeParam > lock(mutex);
                        if (checksum != value * 7)
                        {
                            inconsistent.store(true, std::memory_order_relaxed);
                        }
                    }
                }
            });
    }
    for (std::thread& thread : threads)
    {
        thread.join();
    }
    EXPECT_FALSE(inconsistent.load());
    EXPECT_EQ(value, 6000U);
}

TYPED_TEST(shared_mutex_tests, many_parked_addresses_complete)
{
    std::array< TypeParam, 128 > mutexes;
    std::array< std::uint64_t, 128 > values{};
    for (TypeParam& mutex : mutexes)
    {
        mutex.lock_shared();
    }
    std::latch ready(mutexes.size());
    std::vector< std::thread > writers;
    for (std::size_t index = 0; index < mutexes.size(); ++index)
    {
        writers.emplace_back(
            [&, index]
            {
                ready.count_down();
                std::unique_lock< TypeParam > lock(mutexes[index]);
                values[index] = index + 1;
            });
    }
    ready.wait();
    for (TypeParam& mutex : mutexes)
    {
        mutex.unlock_shared();
    }
    for (std::thread& writer : writers)
    {
        writer.join();
    }
    for (std::size_t index = 0; index < values.size(); ++index)
    {
        EXPECT_EQ(values[index], index + 1);
    }
}

TYPED_TEST(shared_mutex_tests, destruction_after_last_reader_handoff)
{
    for (std::size_t iteration = 0; iteration < 200; ++iteration)
    {
        TypeParam* mutex = new TypeParam;
        mutex->lock_shared();
        std::latch ready(1);
        std::thread successor(
            [&]
            {
                ready.count_down();
                mutex->lock();
                mutex->unlock();
                delete mutex;
            });
        ready.wait();
        mutex->unlock_shared();
        successor.join();
    }
}

TEST(thin_shared_mutex, waits_compare_upper_32_bits)
{
    if constexpr (std::atomic< std::uint64_t >::is_always_lock_free)
    {
        std::atomic< std::uint64_t > state{std::uint64_t{1} << 40};
        std::future< void > returned = std::async(std::launch::async,
                                                  [&]
                                                  {
                                                      rpnx::detail::wait_on_mutex_address(&state, std::uint64_t{0});
                                                  });
        std::future_status status = returned.wait_for(std::chrono::seconds(5));
        rpnx::detail::wake_all_mutex_address< std::uint64_t >(&state);
        returned.get();
        EXPECT_EQ(status, std::future_status::ready);
    }
}
