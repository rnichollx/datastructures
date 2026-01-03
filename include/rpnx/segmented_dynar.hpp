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

#ifndef RPNX_SEGMENTED_DYNAR_HPP
#define RPNX_SEGMENTED_DYNAR_HPP
#include <memory>
#include <bit>
#include <cassert>
#include <stdexcept>

namespace rpnx
{
    // segmented_dynar<T> implements a segmented dynamic array of type T,
    // it has the property that push_back has a worst-case time of O(log n),
    // while still providing O(1) amortized time for push_back, and O(1) time for access.
    // It does this by allocating segments of exponentially increasing sizes.
    template <typename T, typename Alloc = std::allocator<T>>
    class segmented_dynar
    {
        // Segmented Dynamic array using fixed-size segments of exponential sizes
    private:
        std::size_t m_size = 0;
        std::size_t m_capacity = 0;
        T** m_segments = nullptr;
        [[no_unique_address]] Alloc alloc;


        static constexpr std::size_t index_segment(std::size_t index) noexcept
        {
            if (index == 0 || index == 1)
            {
                return 0;
            }

            return std::bit_width(index) - 1;
        }

        static constexpr std::size_t index_subindex(std::size_t index) noexcept
        {
            if (index == 0 || index == 1)
            {
                return index;
            }

            std::size_t segment = index_segment(index);
            std::size_t base_index = 1 << segment;
            return index - base_index;
        }

        static constexpr std::size_t capacity_segment_count(std::size_t capacity) noexcept
        {
            if (capacity == 0)
            {
                return 0;
            }
            if (capacity == 1)
            {
                return 1;
            }
            return std::bit_width(capacity - 1);
        }

        static_assert(capacity_segment_count(1) == 1);

        static constexpr std::size_t segment_count_total_capacity(std::size_t segment_count) noexcept
        {
            if (segment_count == 0)
            {
                return 0;
            }
            return (1 << segment_count);
        }

        static constexpr std::size_t segment_size(std::size_t segment_index)
        {
            if (segment_index == 0 || segment_index == 1)
            {
                return 2;
            }
            return (1 << (segment_index));
        }

        static_assert(segment_size(0) == 2);
        static_assert(segment_size(1) == 2);
        static_assert(segment_size(2) == 4);


        static_assert(capacity_segment_count(0) == 0);
        static_assert(segment_count_total_capacity(0) == 0);
        static_assert(segment_size(0) == 2);

        static_assert(capacity_segment_count(2) == 1);
        static_assert(segment_count_total_capacity(1) == 2);
        static_assert(segment_size(1) == 2);

        static_assert(capacity_segment_count(4) == 2);
        static_assert(segment_count_total_capacity(2) == 4);
        static_assert(segment_size(2) == 4);

        static_assert(capacity_segment_count(8) == 3);
        static_assert(segment_count_total_capacity(3) == 8);
        static_assert(segment_size(3) == 8);

        static_assert(capacity_segment_count(16) == 4);
        static_assert(segment_count_total_capacity(4) == 16);
        static_assert(segment_size(4) == 16);

        static_assert(index_segment(0) == 0);
        static_assert(index_segment(1) == 0);
        static_assert(index_segment(2) == 1);
        static_assert(index_segment(3) == 1);
        static_assert(index_segment(4) == 2);
        static_assert(index_segment(5) == 2);
        static_assert(index_segment(6) == 2);
        static_assert(index_segment(7) == 2);
        static_assert(index_segment(8) == 3);
        static_assert(index_segment(9) == 3);
        static_assert(index_segment(15) == 3);
        static_assert(index_segment(16) == 4);


        static_assert(index_subindex(0) == 0);
        static_assert(index_subindex(1) == 1);
        static_assert(index_subindex(2) == 0);
        static_assert(index_subindex(3) == 1);
        static_assert(index_subindex(4) == 0);
        static_assert(index_subindex(5) == 1);

    public:
        segmented_dynar() = default;

        explicit segmented_dynar(const Alloc& a) noexcept : alloc(a) {}

        Alloc get_allocator() const noexcept
        {
            return alloc;
        }

        std::size_t capacity() const
        {
            return m_capacity;
        }

        std::size_t size() const
        {
            return m_size;
        }

        void reserve(std::size_t new_capacity)
        {
            using segment_allocator_type = typename std::allocator_traits<Alloc>::template rebind_alloc<T*>;
            using segment_alloc_traits = std::allocator_traits<segment_allocator_type>;
            if (new_capacity <= m_capacity)
            {
                return;
            }
            std::size_t old_segment_count = capacity_segment_count(m_capacity);
            std::size_t new_segment_count = capacity_segment_count(new_capacity);
            segment_allocator_type typed_allocator(alloc);
            T** new_segments = segment_alloc_traits::allocate(typed_allocator, new_segment_count);

            using element_allocator_type = typename std::allocator_traits<Alloc>::template rebind_alloc<T>;
            using element_alloc_traits = std::allocator_traits<element_allocator_type>;
            element_allocator_type elem_alloc(alloc);
            try
            {
                for (std::size_t i = 0; i < old_segment_count; ++i)
                {
                    new_segments[i] = m_segments[i];
                }
                for (std::size_t i = old_segment_count; i < new_segment_count; ++i)
                {
                    new_segments[i] = nullptr;
                }
                for (std::size_t i = old_segment_count; i < new_segment_count; ++i)
                {
                    new_segments[i] = element_alloc_traits::allocate(elem_alloc, segment_size(i));
                }
                if (m_segments != nullptr)
                {
                    segment_alloc_traits::deallocate(typed_allocator, m_segments, old_segment_count);
                }
            }
            catch (...)
            {
                for (std::size_t i = old_segment_count; i < new_segment_count; ++i)
                {
                    if (new_segments[i] != nullptr)
                    {
                        element_alloc_traits::deallocate(elem_alloc, new_segments[i], segment_size(i));
                    }
                }
                segment_alloc_traits::deallocate(typed_allocator, new_segments, new_segment_count);
                throw;
            }
            m_segments = new_segments;
            m_capacity = segment_count_total_capacity(new_segment_count);
        }

        void push_back(T value)
        {
            if (size() >= capacity())
            {
                reserve(capacity() + 1);
            }
            std::size_t insertion_index = m_size;
            assert(capacity() > insertion_index);
            std::size_t segment_index = index_segment(insertion_index);
            std::size_t sub_index = index_subindex(insertion_index);

            using element_allocator_type = typename std::allocator_traits<Alloc>::template rebind_alloc<T>;
            using element_alloc_traits = std::allocator_traits<element_allocator_type>;
            element_allocator_type elem_alloc(alloc);
            T*& segment = m_segments[segment_index];
            assert(sub_index < segment_size(segment_index));
            assert(segment != nullptr);
            element_alloc_traits::construct(elem_alloc, &segment[sub_index], std::move(value));
            ++m_size;
        }

        T& operator[](std::size_t index)
        {
            std::size_t segment_index = index_segment(index);
            std::size_t sub_index = index_subindex(index);
            return m_segments[segment_index][sub_index];
        }

        T const& operator[](std::size_t index) const
        {
            std::size_t segment_index = index_segment(index);
            std::size_t sub_index = index_subindex(index);
            return m_segments[segment_index][sub_index];
        }

        T& at(std::size_t index)
        {
            if (index >= size())
            {
                throw std::out_of_range("segmented_dynar::at: index out of range");
            }
            return (*this)[index];
        }

        T const& at(std::size_t index) const
        {
            if (index >= size())
            {
                throw std::out_of_range("segmented_dynar::at: index out of range");
            }
            return (*this)[index];
        }

        void pop_back()
        {
            --m_size;
            std::size_t segment_index = index_segment(m_size);
            std::size_t sub_index = index_subindex(m_size);
            using element_allocator_type = typename std::allocator_traits<Alloc>::template rebind_alloc<T>;
            using element_alloc_traits = std::allocator_traits<element_allocator_type>;
            element_allocator_type elem_alloc(alloc);
            T*& segment = m_segments[segment_index];
            element_alloc_traits::destroy(elem_alloc, &segment[sub_index]);
        }

        void shrink_to_fit()
        {
            std::size_t required_segment_count = capacity_segment_count(m_size);
            if (required_segment_count < capacity_segment_count(m_capacity))
            {
                using segment_allocator_type = typename std::allocator_traits<Alloc>::template rebind_alloc<T*>;
                using segment_alloc_traits = std::allocator_traits<segment_allocator_type>;
                segment_allocator_type segment_allocator(alloc);

                using element_allocator_type = typename std::allocator_traits<Alloc>::template rebind_alloc<T>;
                using element_alloc_traits = std::allocator_traits<element_allocator_type>;
                element_allocator_type elem_alloc(alloc);

                if (required_segment_count == 0)
                {
                    for (std::size_t i = 0; i < capacity_segment_count(m_capacity); ++i)
                    {
                        elem_alloc.deallocate(m_segments[i], segment_size(i));
                    }
                    segment_allocator.deallocate(m_segments, capacity_segment_count(m_capacity));
                    m_segments = nullptr;
                    m_capacity = 0;
                    return;
                }
                T** new_segments = segment_alloc_traits::allocate(segment_allocator, required_segment_count);
                for (std::size_t i = 0; i < required_segment_count; ++i)
                {
                    new_segments[i] = m_segments[i];
                }
                for (std::size_t i = required_segment_count; i < capacity_segment_count(m_capacity); ++i)
                {
                    elem_alloc.deallocate(m_segments[i], segment_size(i));
                }
                segment_allocator.deallocate(m_segments, capacity_segment_count(m_capacity));
                m_segments = new_segments;
                m_capacity = segment_count_total_capacity(required_segment_count);
            }
        }

        void clear()
        {
            while (size() > 0)
            {
                pop_back();
            }
        }

        void reset()
        {
            clear();
            shrink_to_fit();
        }

        ~segmented_dynar()
        {
            reset();
        }

        template <typename... Args>
        T& emplace_back(Args&&... args)
        {
            if (size() >= capacity())
            {
                reserve(capacity() + 1);
            }
            std::size_t insertion_index = m_size;
            assert(capacity()> insertion_index);
            std::size_t segment_index = index_segment(insertion_index);
            std::size_t sub_index = index_subindex(insertion_index);
            using element_allocator_type = typename std::allocator_traits<Alloc>::template rebind_alloc<T>;
            using element_alloc_traits = std::allocator_traits<element_allocator_type>;
            element_allocator_type elem_alloc(alloc);
            T*& segment = m_segments[segment_index];
            assert(sub_index < segment_size(segment_index));
            assert(segment != nullptr);
            element_alloc_traits::construct(elem_alloc, &segment[sub_index], std::forward<Args>(args)...);
            ++m_size;
            return segment[sub_index];
        }

        segmented_dynar(segmented_dynar&& other) noexcept : m_size(other.m_size), m_capacity(other.m_capacity),
                                                            m_segments(other.m_segments), alloc(std::move(other.alloc))
        {
            other.m_size = 0;
            other.m_capacity = 0;
            other.m_segments = nullptr;
        }

        segmented_dynar& operator=(segmented_dynar&& other) noexcept
        {
            if (this != &other)
            {
                reset();
                m_size = other.m_size;
                m_capacity = other.m_capacity;
                m_segments = other.m_segments;
                alloc = std::move(other.alloc);

                other.m_size = 0;
                other.m_capacity = 0;
                other.m_segments = nullptr;
            }
            return *this;
        }

        segmented_dynar(const segmented_dynar& other) : 
            alloc(std::allocator_traits<Alloc>::select_on_container_copy_construction(other.alloc))
        {
            reserve(other.m_size);
            for (std::size_t i = 0; i < other.m_size; ++i)
            {
                push_back(other[i]);
            }
        }

        segmented_dynar& operator=(const segmented_dynar& other)
        {
            if (this != &other)
            {
                if constexpr (std::allocator_traits<Alloc>::propagate_on_container_copy_assignment::value)
                {
                    if (alloc != other.alloc)
                    {
                        reset();
                    }
                    alloc = other.alloc;
                }
                clear();
                reserve(other.m_size);
                for (std::size_t i = 0; i < other.m_size; ++i)
                {
                    push_back(other[i]);
                }
            }
            return *this;
        }
    };
}
#endif //RPNXHADIX_SEGMENTED_DYNAR_HPP
