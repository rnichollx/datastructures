// Copyright (c) 2026 Ryan P. Nicholl <rnicholl@protonmail.com>
// SPDX-License-Identifier: Apache-2.0
#include <rpnx/fast_mutex.hpp>
#include <rpnx/thin_mutex.hpp>

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <latch>
#include <mutex>
#include <thread>
#include <type_traits>
#include <vector>

/** @brief Runs the same ownership, publication, and progress tests on both mutex implementations. */
template < typename Mutex >
class mutex_tests : public testing::Test
{
};

using mutex_types = testing::Types< rpnx::thin_mutex, rpnx::fast_mutex >;
TYPED_TEST_SUITE(mutex_tests, mutex_types);

TYPED_TEST(mutex_tests, ownership_and_try_lock)
{
    static_assert(!std::is_copy_constructible_v< TypeParam >);
    static_assert(!std::is_copy_assignable_v< TypeParam >);
    static_assert(!std::is_move_constructible_v< TypeParam >);
    static_assert(!std::is_move_assignable_v< TypeParam >);
    TypeParam mutex;
    ASSERT_TRUE(mutex.try_lock());
    bool acquired = true;
    std::thread contender(
        [&]
        {
            acquired = mutex.try_lock();
            if (acquired)
            {
                mutex.unlock();
            }
        });
    contender.join();
    EXPECT_FALSE(acquired);
    mutex.unlock();
    std::unique_lock< TypeParam > lock(mutex, std::try_to_lock);
    EXPECT_TRUE(lock.owns_lock());
}

TYPED_TEST(mutex_tests, contended_publication)
{
    TypeParam mutex;
    std::latch start(8);
    std::uint64_t count = 0;
    std::uint64_t checksum = 0;
    std::vector< std::thread > threads;
    for (std::size_t index = 0; index < 8; ++index)
    {
        threads.emplace_back(
            [&]
            {
                start.arrive_and_wait();
                for (std::size_t iteration = 0; iteration < 10000; ++iteration)
                {
                    std::lock_guard< TypeParam > lock(mutex);
                    ++count;
                    checksum += count;
                }
            });
    }
    for (std::thread& thread : threads)
    {
        thread.join();
    }
    EXPECT_EQ(count, 80000u);
    EXPECT_EQ(checksum, count * (count + 1) / 2);
}

TYPED_TEST(mutex_tests, blocked_waiters_complete)
{
    TypeParam mutex;
    std::latch ready(16);
    std::atomic< std::size_t > entered{0};
    std::vector< std::thread > threads;
    mutex.lock();
    for (std::size_t index = 0; index < 16; ++index)
    {
        threads.emplace_back(
            [&]
            {
                ready.count_down();
                std::lock_guard< TypeParam > lock(mutex);
                entered.fetch_add(1, std::memory_order_relaxed);
            });
    }
    ready.wait();
    // Give the contenders time to reach the blocking path while ownership is held.
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    EXPECT_EQ(entered.load(std::memory_order_relaxed), 0u);
    mutex.unlock();
    for (std::thread& thread : threads)
    {
        thread.join();
    }
    EXPECT_EQ(entered.load(std::memory_order_relaxed), 16u);
}

TYPED_TEST(mutex_tests, destruction_after_handoff)
{
    for (std::size_t iteration = 0; iteration < 200; ++iteration)
    {
        TypeParam* mutex = new TypeParam;
        std::latch ready(1);
        mutex->lock();
        std::thread successor(
            [mutex, &ready]
            {
                ready.count_down();
                mutex->lock();
                mutex->unlock();
                delete mutex;
            });
        ready.wait();
        std::this_thread::sleep_for(std::chrono::microseconds(50));
        // The successor can destroy the mutex before this unlock returns.
        mutex->unlock();
        successor.join();
    }
}

TYPED_TEST(mutex_tests, mixed_acquisition_progress)
{
    TypeParam mutex;
    std::latch start(8);
    std::size_t count = 0;
    std::vector< std::thread > threads;
    for (std::size_t index = 0; index < 8; ++index)
    {
        threads.emplace_back(
            [&, index]
            {
                start.arrive_and_wait();
                for (std::size_t iteration = 0; iteration < 5000; ++iteration)
                {
                    if (index % 2 == 0)
                    {
                        std::unique_lock< TypeParam > lock(mutex, std::try_to_lock);
                        if (lock.owns_lock())
                        {
                            ++count;
                            continue;
                        }
                    }
                    std::lock_guard< TypeParam > lock(mutex);
                    ++count;
                }
            });
    }
    for (std::thread& thread : threads)
    {
        thread.join();
    }
    EXPECT_EQ(count, 40000u);
}

TYPED_TEST(mutex_tests, condition_variable_handoff)
{
    TypeParam mutex;
    std::condition_variable_any changed;
    std::size_t value = 0;
    bool timed_out = false;
    std::thread peer(
        [&]
        {
            std::unique_lock< TypeParam > lock(mutex);
            for (std::size_t iteration = 0; iteration < 200; ++iteration)
            {
                if (!changed.wait_for(lock, std::chrono::seconds(5),
                                      [&]
                                      {
                                          return value % 2 == 1;
                                      }))
                {
                    timed_out = true;
                    return;
                }
                ++value;
                changed.notify_one();
            }
        });
    {
        std::unique_lock< TypeParam > lock(mutex);
        for (std::size_t iteration = 0; iteration < 200; ++iteration)
        {
            if (!changed.wait_for(lock, std::chrono::seconds(5),
                                  [&]
                                  {
                                      return value % 2 == 0;
                                  }))
            {
                timed_out = true;
                break;
            }
            ++value;
            changed.notify_one();
        }
    }
    peer.join();
    EXPECT_FALSE(timed_out);
    EXPECT_EQ(value, 400u);
}

TYPED_TEST(mutex_tests, scoped_lock_opposite_orders)
{
    TypeParam first;
    TypeParam second;
    std::latch start(2);
    std::size_t count = 0;
    std::thread peer(
        [&]
        {
            start.arrive_and_wait();
            for (std::size_t iteration = 0; iteration < 10000; ++iteration)
            {
                std::scoped_lock lock(second, first);
                ++count;
            }
        });
    start.arrive_and_wait();
    for (std::size_t iteration = 0; iteration < 10000; ++iteration)
    {
        std::scoped_lock lock(first, second);
        ++count;
    }
    peer.join();
    EXPECT_EQ(count, 20000u);
}

TYPED_TEST(mutex_tests, adjacent_mutexes)
{
    std::array< TypeParam, 64 > mutexes;
    std::array< std::size_t, 64 > counts{};
    std::latch start(8);
    std::vector< std::thread > threads;
    for (std::size_t index = 0; index < 8; ++index)
    {
        threads.emplace_back(
            [&]
            {
                start.arrive_and_wait();
                for (std::size_t iteration = 0; iteration < 100; ++iteration)
                {
                    for (std::size_t slot = 0; slot < mutexes.size(); ++slot)
                    {
                        std::lock_guard< TypeParam > lock(mutexes[slot]);
                        ++counts[slot];
                    }
                }
            });
    }
    for (std::thread& thread : threads)
    {
        thread.join();
    }
    for (std::size_t count : counts)
    {
        EXPECT_EQ(count, 800u);
    }
}
