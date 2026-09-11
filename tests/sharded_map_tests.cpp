// Copyright (c) 2026 Ryan P. Nicholl <rnicholl@protonmail.com>
// SPDX-License-Identifier: Apache-2.0
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "gtest/gtest.h"

#include "failing_allocator.hpp"
#include "rpnx/sharded_unordered_map.hpp"
#include "tracking_allocator.hpp"
#include <algorithm>
#include <numeric>
#include <type_traits>

namespace
{
    struct stateful_hash
    {
        static std::size_t next_seed;

        std::size_t seed;

        stateful_hash() : seed(next_seed++)
        {
        }

        std::size_t operator()(int) const
        {
            return seed;
        }
    };

    std::size_t stateful_hash::next_seed = 0;
} // namespace

static_assert(!std::is_copy_constructible_v< rpnx::conc_sharded_unordered_map< int, int > >);
static_assert(!std::is_copy_assignable_v< rpnx::conc_sharded_unordered_map< int, int > >);
static_assert(!std::is_move_constructible_v< rpnx::conc_sharded_unordered_map< int, int > >);
static_assert(!std::is_move_assignable_v< rpnx::conc_sharded_unordered_map< int, int > >);

TEST(conc_sharded_unordered_map, put_and_get)
{
    rpnx::conc_sharded_unordered_map< int, std::string > map;
    map.put(1, "one");
    map.put(2, "two");
    map.put(3, "three");

    EXPECT_EQ(map.get(1), "one");
    EXPECT_EQ(map.get(2), "two");
    EXPECT_EQ(map.get(3), "three");
}

TEST(conc_sharded_unordered_map, reuses_the_stored_stateful_hash)
{
    stateful_hash::next_seed = 0;
    rpnx::conc_sharded_unordered_map< int, std::string, stateful_hash > map(4);

    map.put(1, "one");

    EXPECT_EQ(map.get(1), "one");
}

TEST(conc_sharded_unordered_map, concurrent_access)
{
    rpnx::conc_sharded_unordered_map< int, int > map;

    const int num_threads = 32;
    const int num_elements_per_thread = 100000;

    auto insert_func = [&map](int thread_id)
    {
        for (int i = 0; i < num_elements_per_thread; ++i)
        {
            map.put(thread_id * num_elements_per_thread + i, i);
        }
    };

    std::vector< std::thread > threads;
    for (int t = 0; t < num_threads; ++t)
    {
        threads.emplace_back(insert_func, t);
    }

    for (auto& th : threads)
    {
        th.join();
    }

    for (int t = 0; t < num_threads; ++t)
    {
        for (int i = 0; i < num_elements_per_thread; ++i)
        {
            EXPECT_EQ(map.get(t * num_elements_per_thread + i), i);
        }
    }
}

TEST(conc_sharded_unordered_map, exclusive_range)
{
    rpnx::conc_sharded_unordered_map< int, int > map(4);
    for (int i = 0; i < 100; ++i)
    {
        map.put(i, i * 2);
    }

    int count = 0;
    for (auto const& [key, value] : map.range_exclusive())
    {
        EXPECT_EQ(value, key * 2);
        count++;
    }
    EXPECT_EQ(count, 100);

    const auto& cmap = map;
    count = 0;
    for (auto const& [key, value] : cmap.range_exclusive())
    {
        EXPECT_EQ(value, key * 2);
        count++;
    }
    EXPECT_EQ(count, 100);
}

TEST(conc_sharded_unordered_map, size)
{
    rpnx::conc_sharded_unordered_map< int, int > map(4);

    // Because no other thread is concurrently modifying this map, estimate_size is guaranteed to
    // return the actual size.
    EXPECT_EQ(map.estimate_size(), 0);
    EXPECT_EQ(map.size_exclusive(), 0);

    for (int i = 0; i < 100; ++i)
    {
        map.put(i, i);
    }

    EXPECT_EQ(map.estimate_size(), 100);
    EXPECT_EQ(map.size_exclusive(), 100);
}
