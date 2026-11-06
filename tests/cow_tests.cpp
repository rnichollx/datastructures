// Copyright (c) 2026 Ryan P. Nicholl <rnicholl@protonmail.com>
// SPDX-License-Identifier: Apache-2.0

#include <rpnx/cow.hpp>
#include <rpnx/shared_ptr.hpp>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <gtest/gtest.h>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace cow_tests
{
    /** @brief Record destruction of an inline shared value. */
    struct observed_value
    {
        std::atomic< std::size_t >* destructions;
        std::size_t number;

        /** @brief Initialize a value whose destruction is observable. */
        observed_value(std::atomic< std::size_t >& count, std::size_t input)
            : destructions(&count), number(input)
        {
        }

        /** @brief Count destruction of the stored value. */
        ~observed_value()
        {
            destructions->fetch_add(1, std::memory_order_relaxed);
        }
    };
} // namespace cow_tests

static_assert(sizeof(rpnx::shared_ptr< cow_tests::observed_value >) == sizeof(void*));
static_assert(!std::is_constructible_v< rpnx::shared_ptr< std::uint64_t >, std::uint64_t* >);

TEST(shared_ptr, ownership_and_value_lifetime)
{
    std::atomic< std::size_t > destructions{0};
    rpnx::shared_ptr< cow_tests::observed_value > first = rpnx::make_shared< cow_tests::observed_value >(destructions, 7);
    EXPECT_EQ(first.use_count(), 1);
    EXPECT_EQ(first->number, 7);

    rpnx::shared_ptr< cow_tests::observed_value > second = first;
    EXPECT_EQ(first.get(), second.get());
    EXPECT_EQ(first.use_count(), 2);
    first.reset();
    EXPECT_EQ(second.use_count(), 1);
    EXPECT_EQ(destructions.load(std::memory_order_relaxed), 0);

    rpnx::shared_ptr< cow_tests::observed_value > third = std::move(second);
    EXPECT_FALSE(second);
    EXPECT_EQ((*third).number, 7);
    third.reset();
    EXPECT_EQ(destructions.load(std::memory_order_relaxed), 1);
}

TEST(cow, shared_edits_detach_and_empty_values_compare)
{
    rpnx::cow< std::string > first("initial");
    rpnx::cow< std::string > second = first;
    second.edit() = "changed";
    EXPECT_EQ(first.read(), "initial");
    EXPECT_EQ(second.read(), "changed");

    first = std::string("assigned");
    EXPECT_EQ(first.read(), "assigned");
    second.set("replaced");
    EXPECT_EQ(second.read(), "replaced");

    rpnx::cow< std::string > empty(nullptr);
    rpnx::cow< std::string > also_empty(nullptr);
    EXPECT_TRUE(empty == also_empty);
    EXPECT_TRUE(empty < first);
    EXPECT_FALSE(first < empty);
}

TEST(cow, distinct_handles_detach_during_concurrent_edits)
{
    rpnx::cow< std::string > original("shared");
    std::vector< rpnx::cow< std::string > > handles(8, original);
    std::vector< std::thread > threads;
    for (std::size_t index = 0; index < handles.size(); ++index)
    {
        threads.emplace_back(
            [&, index]
            {
                handles[index].edit() = std::to_string(index);
            });
    }
    for (std::thread& thread : threads)
    {
        thread.join();
    }
    EXPECT_EQ(original.read(), "shared");
    for (std::size_t index = 0; index < handles.size(); ++index)
    {
        EXPECT_EQ(handles[index].read(), std::to_string(index));
    }
}

TEST(cow, last_owner_release_precedes_unique_edit)
{
    rpnx::cow< std::uint64_t > first(4);
    rpnx::cow< std::uint64_t > second = first;
    std::atomic< std::uint64_t > observed{0};
    std::atomic< bool > released{false};
    std::thread reader(
        [&]
        {
            observed.store(second.read(), std::memory_order_relaxed);
            second = rpnx::cow< std::uint64_t >(nullptr);
            released.store(true, std::memory_order_relaxed);
        });
    while (!released.load(std::memory_order_relaxed))
    {
        std::this_thread::yield();
    }
    first.edit() = 9;
    reader.join();
    EXPECT_EQ(observed.load(std::memory_order_relaxed), 4);
    EXPECT_EQ(first.read(), 9);
}
