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

#ifndef RPNX_TESTS_FAILING_ALLOCATOR_HPP
#define RPNX_TESTS_FAILING_ALLOCATOR_HPP

#include <memory>
#include <stdexcept>

namespace testutils
{
    // A custom allocator that can be made to fail after a certain number of allocations
    template <typename T>
    struct failing_allocator
    {
        using value_type = T;
        static int allocation_count;
        static int fail_at;
        static int total_allocations;

        failing_allocator() = default;

        template <typename U>
        failing_allocator(const failing_allocator<U>&)
        {
        }

        T* allocate(std::size_t n)
        {
            if (allocation_count >= fail_at)
            {
                throw std::bad_alloc();
            }
            T* p = std::allocator<T>().allocate(n);
            total_allocations++;
            allocation_count++;
            return p;
        }

        void deallocate(T* p, std::size_t n)
        {
            std::allocator<T>().deallocate(p, n);
            total_allocations--;
        }

        bool operator==(const failing_allocator&) const = default;
    };

    template <typename T>
    int failing_allocator<T>::allocation_count = 0;
    template <typename T>
    int failing_allocator<T>::fail_at = 999;
    template <typename T>
    int failing_allocator<T>::total_allocations = 0;
} // namespace testutils

#endif // RPNX_TESTS_FAILING_ALLOCATOR_HPP
