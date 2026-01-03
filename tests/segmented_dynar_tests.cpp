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
