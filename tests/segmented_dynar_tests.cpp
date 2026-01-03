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

#include "rpnx/segmented_dynar.hpp"
#include "failing_allocator.hpp"
#include "tracking_allocator.hpp"
#include <algorithm>
#include <numeric>

TEST(segmented_dynar, construct_empty)
{
    rpnx::segmented_dynar<int> arr;
    EXPECT_EQ(arr.size(), 0);
}

TEST(segmented_dynar, push_back_and_access)
{
    rpnx::segmented_dynar<int> arr;
    arr.push_back(10);
    arr.push_back(20);
    arr.push_back(30);
    arr.push_back(40);
    arr.push_back(50);

    EXPECT_EQ(arr.size(), 5);
    EXPECT_EQ(arr[0], 10);
    EXPECT_EQ(arr[1], 20);
    EXPECT_EQ(arr[2], 30);
    EXPECT_EQ(arr[3], 40);
    EXPECT_EQ(arr[4], 50);
}

TEST(segmented_dynar, at_out_of_range)
{
    rpnx::segmented_dynar<int> arr;
    arr.push_back(10);
    arr.push_back(20);

    EXPECT_THROW(arr.at(2), std::out_of_range);
    EXPECT_THROW(arr.at(100), std::out_of_range);
}

TEST(segmented_dynar, reserve_capacity)
{
    rpnx::segmented_dynar<int> arr;
    arr.reserve(10);
    EXPECT_GE(arr.capacity(), 10);

    for (int i = 0; i < 10; ++i)
    {
        arr.push_back(i);
    }

    EXPECT_EQ(arr.size(), 10);
    for (int i = 0; i < 10; ++i)
    {
        EXPECT_EQ(arr[i], i);
    }
}

TEST(segmented_dynar, pop_back)
{
    rpnx::segmented_dynar<int> arr;
    arr.push_back(1);
    arr.push_back(2);
    arr.push_back(3);

    EXPECT_EQ(arr.size(), 3);
    arr.pop_back();
    EXPECT_EQ(arr.size(), 2);
    EXPECT_EQ(arr[0], 1);
    EXPECT_EQ(arr[1], 2);

    arr.pop_back();
    EXPECT_EQ(arr.size(), 1);
    EXPECT_EQ(arr[0], 1);

    arr.pop_back();
    EXPECT_EQ(arr.size(), 0);
}

TEST(segmented_dynar, clear_and_reset)
{
    rpnx::segmented_dynar<int> arr;
    for (int i = 0; i < 20; ++i)
    {
        arr.push_back(i);
    }

    EXPECT_EQ(arr.size(), 20);
    arr.clear();
    EXPECT_EQ(arr.size(), 0);

    for (int i = 0; i < 15; ++i)
    {
        arr.push_back(i);
    }
    EXPECT_EQ(arr.size(), 15);

    arr.reset();
    EXPECT_EQ(arr.size(), 0);
    EXPECT_EQ(arr.capacity(), 0);
}

TEST(segmented_dynar, emplace_back)
{
    rpnx::segmented_dynar<std::pair<int, std::string>> arr;
    arr.emplace_back(1, "one");
    arr.emplace_back(2, "two");
    arr.emplace_back(3, "three");

    EXPECT_EQ(arr.size(), 3);
    EXPECT_EQ(arr[0].first, 1);
    EXPECT_EQ(arr[0].second, "one");
    EXPECT_EQ(arr[1].first, 2);
    EXPECT_EQ(arr[1].second, "two");
    EXPECT_EQ(arr[2].first, 3);
    EXPECT_EQ(arr[2].second, "three");
}

TEST(segmented_dynar, large_number_of_elements)
{
    rpnx::segmented_dynar<int> arr;
    const int num_elements = 10000;

    for (int i = 0; i < num_elements; ++i)
    {
        arr.push_back(i);
    }

    EXPECT_EQ(arr.size(), num_elements);
    for (int i = 0; i < num_elements; ++i)
    {
        EXPECT_EQ(arr[i], i);
    }
}

TEST(segmented_dynar, copy_constructor_and_assignment)
{
    rpnx::segmented_dynar<int> arr1;
    for (int i = 0; i < 10; ++i)
    {
        arr1.push_back(i);
    }

    rpnx::segmented_dynar<int> arr2 = arr1; // Copy constructor
    EXPECT_EQ(arr2.size(), arr1.size());
    for (int i = 0; i < arr2.size(); ++i)
    {
        EXPECT_EQ(arr2[i], arr1[i]);
    }

    rpnx::segmented_dynar<int> arr3;
    arr3 = arr1; // Copy assignment
    EXPECT_EQ(arr3.size(), arr1.size());
    for (int i = 0; i < arr3.size(); ++i)
    {
        EXPECT_EQ(arr3[i], arr1[i]);
    }
}

TEST(segmented_dynar, reserve_leak_on_exception)
{
    using Alloc = testutils::failing_allocator<int>;
    testutils::failing_allocator<int>::allocation_count = 0;
    testutils::failing_allocator<int>::fail_at = 999;
    testutils::failing_allocator<int>::total_allocations = 0;

    rpnx::segmented_dynar<int, Alloc> arr;

    // Initial allocations to set up some state
    arr.push_back(1);
    arr.push_back(2);

    int initial_allocations = testutils::failing_allocator<int>::total_allocations;

    // Now we want reserve to fail after some segments are allocated
    testutils::failing_allocator<int>::fail_at = testutils::failing_allocator<int>::allocation_count + 2;
    // Fail on the 2nd NEW element allocation in reserve

    EXPECT_THROW(arr.reserve(100), std::bad_alloc);

    EXPECT_EQ(testutils::failing_allocator<int>::total_allocations, initial_allocations);
}

TEST(segmented_dynar, allocator_propagation_copy_assignment)
{
    rpnx::segmented_dynar<int, testutils::tracking_allocator<int>> c1(testutils::tracking_allocator<int>(1));
    rpnx::segmented_dynar<int, testutils::tracking_allocator<int>> c2(testutils::tracking_allocator<int>(2));

    c2 = c1;
    // Should propagate
    EXPECT_EQ(c2.get_allocator().id, 1);
}

TEST(segmented_dynar, allocator_non_propagation_copy_assignment)
{
    rpnx::segmented_dynar<int, testutils::non_propagate_allocator<int>> c1(testutils::non_propagate_allocator<int>(1));
    rpnx::segmented_dynar<int, testutils::non_propagate_allocator<int>> c2(testutils::non_propagate_allocator<int>(2));

    c2 = c1;
    // Should NOT propagate
    EXPECT_EQ(c2.get_allocator().id, 2);
}

TEST(segmented_dynar, allocator_copy_construction)
{
    rpnx::segmented_dynar<int, testutils::tracking_allocator<int>> c1(testutils::tracking_allocator<int>(1));
    rpnx::segmented_dynar<int, testutils::tracking_allocator<int>> c2 = c1;

    // Should use select_on_container_copy_construction
    EXPECT_EQ(c2.get_allocator().id, 101);
}

TEST(segmented_dynar, iterator_begin_end_empty)
{
    rpnx::segmented_dynar<int> arr;
    EXPECT_EQ(arr.begin(), arr.end());
    EXPECT_EQ(arr.cbegin(), arr.cend());
}

TEST(segmented_dynar, iterator_forward_iteration)
{
    rpnx::segmented_dynar<int> arr;
    for (int i = 0; i < 100; ++i)
    {
        arr.push_back(i);
    }

    int expected = 0;
    for (auto it = arr.begin(); it != arr.end(); ++it)
    {
        EXPECT_EQ(*it, expected++);
    }
    EXPECT_EQ(expected, 100);
}

TEST(segmented_dynar, iterator_backward_iteration)
{
    rpnx::segmented_dynar<int> arr;
    for (int i = 0; i < 100; ++i)
    {
        arr.push_back(i);
    }

    int expected = 99;
    auto it = arr.end();
    while (it != arr.begin())
    {
        --it;
        EXPECT_EQ(*it, expected--);
    }
    EXPECT_EQ(expected, -1);
}

TEST(segmented_dynar, iterator_post_increment_decrement)
{
    rpnx::segmented_dynar<int> arr;
    arr.push_back(1);
    arr.push_back(2);

    auto it = arr.begin();
    EXPECT_EQ(*(it++), 1);
    EXPECT_EQ(*it, 2);
    EXPECT_EQ(*(it--), 2);
    EXPECT_EQ(*it, 1);
}

TEST(segmented_dynar, iterator_random_access_arithmetic)
{
    rpnx::segmented_dynar<int> arr;
    for (int i = 0; i < 1000; ++i)
    {
        arr.push_back(i);
    }

    auto it = arr.begin();
    EXPECT_EQ(*(it + 50), 50);
    EXPECT_EQ(*(it + 500), 500);

    it += 100;
    EXPECT_EQ(*it, 100);

    it -= 50;
    EXPECT_EQ(*it, 50);

    auto it2 = it + 400;
    EXPECT_EQ(it2 - it, 400);
    EXPECT_EQ(it - it2, -400);
}

TEST(segmented_dynar, iterator_subscript_operator)
{
    rpnx::segmented_dynar<int> arr;
    for (int i = 0; i < 100; ++i)
    {
        arr.push_back(i);
    }

    auto it = arr.begin();
    for (int i = 0; i < 100; ++i)
    {
        EXPECT_EQ(it[i], i);
    }
}

TEST(segmented_dynar, iterator_comparison_operators)
{
    rpnx::segmented_dynar<int> arr;
    for (int i = 0; i < 10; ++i) arr.push_back(i);

    auto it1 = arr.begin() + 2;
    auto it2 = arr.begin() + 5;

    EXPECT_TRUE(it1 < it2);
    EXPECT_TRUE(it1 <= it2);
    EXPECT_TRUE(it2 > it1);
    EXPECT_TRUE(it2 >= it1);
    EXPECT_FALSE(it1 == it2);
    EXPECT_TRUE(it1 != it2);

    auto it3 = it1;
    EXPECT_TRUE(it1 == it3);
    EXPECT_FALSE(it1 != it3);
    EXPECT_TRUE(it1 <= it3);
    EXPECT_TRUE(it1 >= it3);
}

TEST(segmented_dynar, iterator_const_iterator_interop)
{
    rpnx::segmented_dynar<int> arr;
    arr.push_back(10);

    rpnx::segmented_dynar<int>::iterator it = arr.begin();
    rpnx::segmented_dynar<int>::const_iterator cit = it; // Conversion

    EXPECT_EQ(*it, *cit);
    EXPECT_TRUE(it == cit);
    EXPECT_FALSE(it != cit);
    EXPECT_TRUE(it <= cit);
    EXPECT_TRUE(it >= cit);
}

TEST(segmented_dynar, iterator_stl_algorithms)
{
    rpnx::segmented_dynar<int> arr;
    for (int i = 0; i < 100; ++i) arr.push_back(100 - i);

    std::sort(arr.begin(), arr.end());

    for (int i = 0; i < 100; ++i)
    {
        EXPECT_EQ(arr[i], i + 1);
    }

    auto it = std::find(arr.begin(), arr.end(), 50);
    EXPECT_NE(it, arr.end());
    EXPECT_EQ(*it, 50);

    int sum = std::accumulate(arr.begin(), arr.end(), 0);
    EXPECT_EQ(sum, 100 * 101 / 2);
}

TEST(segmented_dynar, iterator_large_scale_iteration)
{
    rpnx::segmented_dynar<int> arr;
    const int count = 10000;
    for (int i = 0; i < count; ++i) arr.push_back(i);

    int i = 0;
    for (int val : arr)
    {
        EXPECT_EQ(val, i++);
    }
    EXPECT_EQ(i, count);
}

TEST(segmented_dynar, iterator_arrow_operator)
{
    struct S
    {
        int x;
    };
    rpnx::segmented_dynar<S> arr;
    arr.push_back({42});

    auto it = arr.begin();
    EXPECT_EQ(it->x, 42);
}
