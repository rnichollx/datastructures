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
#include <bit>
#include <cassert>
#include <limits>
#include <memory>
#include <stdexcept>

namespace rpnx
{
    /**
     * @brief Dynamic array backed by exponentially sized stable segments.
     *
     * Elements are stored in separately allocated segments. Growing the
     * container adds segments without relocating existing elements, so element
     * references and pointers remain valid across `reserve()` and append
     * operations. Random access is constant time. Appending is amortized O(1)
     * and O(log n) in the worst case because growth may allocate multiple
     * segments and a replacement segment-pointer table.
     *
     * Iterators are invalidated by operations that change the logical element
     * sequence. `shrink_to_fit()` may release unused segments but does not move
     * retained elements. Unless stated otherwise, operations provide the basic
     * exception guarantee.
     *
     * @tparam T Stored element type.
     * @tparam Alloc Allocator used for elements and rebound for segment metadata.
     */
    template < typename T, typename Alloc = std::allocator< T > >
    class segmented_dynar
    {
        // Segmented Dynamic array using fixed-size segments of exponential sizes
      private:
        std::size_t m_size = 0;
        std::size_t m_capacity = 0;
        T** m_segments = nullptr;
        [[no_unique_address]] Alloc m_alloc;

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
            std::size_t base_index = std::size_t{1} << segment;
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
            return (std::size_t{1} << segment_count);
        }

        static constexpr std::size_t segment_size(std::size_t segment_index)
        {
            if (segment_index == 0 || segment_index == 1)
            {
                return 2;
            }
            return (std::size_t{1} << segment_index);
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
        /// Constructs an empty container with a default-constructed allocator.
        segmented_dynar() = default;

        /** @brief Constructs an empty container with an allocator. @param a Allocator to copy. */
        explicit segmented_dynar(const Alloc& a) noexcept : m_alloc(a)
        {
        }

        /** @brief Returns the allocator associated with the container. @return A copy of the allocator. */
        Alloc get_allocator() const noexcept
        {
            return m_alloc;
        }

        /** @brief Returns the number of elements that fit in allocated segments. @return Current capacity. */
        std::size_t capacity() const
        {
            return m_capacity;
        }

        /** @brief Returns the number of constructed elements. @return Current element count. */
        std::size_t size() const
        {
            return m_size;
        }

        /**
         * @brief Ensures capacity for at least a requested number of elements.
         * @param new_capacity Requested minimum capacity.
         * @throws std::length_error if the requested capacity exceeds the
         * representable segmented layout.
         * @note Existing elements are not moved; their references remain valid.
         */
        void reserve(std::size_t new_capacity)
        {
            using segment_allocator_type = typename std::allocator_traits< Alloc >::template rebind_alloc< T* >;
            using segment_alloc_traits = std::allocator_traits< segment_allocator_type >;
            if (new_capacity <= m_capacity)
            {
                return;
            }
            if (new_capacity > (std::size_t{1} << (std::numeric_limits< std::size_t >::digits - 1)))
            {
                throw std::length_error("segmented_dynar::reserve: requested capacity is too large");
            }
            std::size_t old_segment_count = capacity_segment_count(m_capacity);
            std::size_t new_segment_count = capacity_segment_count(new_capacity);
            segment_allocator_type typed_allocator(m_alloc);
            T** new_segments = segment_alloc_traits::allocate(typed_allocator, new_segment_count);

            using element_allocator_type = typename std::allocator_traits< Alloc >::template rebind_alloc< T >;
            using element_alloc_traits = std::allocator_traits< element_allocator_type >;
            element_allocator_type elem_alloc(m_alloc);
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

        /** @brief Appends a copied element. @param value Value to copy into the new final element. @note Existing element references remain valid. */
        void push_back(T const& value)
        {
            emplace_back(value);
        }

        /** @brief Appends a moved element. @param value Value to move into the new final element. @note Existing element references remain valid. */
        void push_back(T&& value)
        {
            emplace_back(std::move(value));
        }

        /** @brief Accesses an element without bounds checking. @param index Zero-based index. @return Mutable element reference. @pre `index < size()`. */
        T& operator[](std::size_t index)
        {
            std::size_t segment_index = index_segment(index);
            std::size_t sub_index = index_subindex(index);
            return m_segments[segment_index][sub_index];
        }

        /** @brief Accesses an element without bounds checking. @param index Zero-based index. @return Immutable element reference. @pre `index < size()`. */
        T const& operator[](std::size_t index) const
        {
            std::size_t segment_index = index_segment(index);
            std::size_t sub_index = index_subindex(index);
            return m_segments[segment_index][sub_index];
        }

        /** @brief Accesses an element with bounds checking. @param index Zero-based index. @return Mutable element reference. @throws std::out_of_range if `index >= size()`. */
        T& at(std::size_t index)
        {
            if (index >= size())
            {
                throw std::out_of_range("segmented_dynar::at: index out of range");
            }
            return (*this)[index];
        }

        /** @brief Accesses an element with bounds checking. @param index Zero-based index. @return Immutable element reference. @throws std::out_of_range if `index >= size()`. */
        T const& at(std::size_t index) const
        {
            if (index >= size())
            {
                throw std::out_of_range("segmented_dynar::at: index out of range");
            }
            return (*this)[index];
        }

        /** @brief Returns the first element. @return Mutable first-element reference. @pre The container is not empty. */
        T& front()
        {
            return (*this)[0];
        }

        /** @brief Returns the first element. @return Immutable first-element reference. @pre The container is not empty. */
        T const& front() const
        {
            return (*this)[0];
        }

        /** @brief Returns the final element. @return Mutable final-element reference. @pre The container is not empty. */
        T& back()
        {
            return (*this)[m_size - 1];
        }

        /** @brief Returns the final element. @return Immutable final-element reference. @pre The container is not empty. */
        T const& back() const
        {
            return (*this)[m_size - 1];
        }

        /** @brief Replaces the contents with repeated copies. @param count Number of elements. @param value Value copied into every element. */
        void assign(std::size_t count, const T& value)
        {
            clear();
            reserve(count);
            for (std::size_t i = 0; i < count; ++i)
            {
                push_back(value);
            }
        }

        /**
         * @brief Replaces the contents with an iterator range.
         * @tparam InputIt Input iterator type.
         * @param first First source element.
         * @param last One-past-last source element.
         * @pre The source range does not refer to elements of this container.
         */
        template < typename InputIt, typename = std::enable_if_t< !std::is_integral_v< InputIt > > >
        void assign(InputIt first, InputIt last)
        {
            clear();
            if constexpr (std::is_base_of_v< std::forward_iterator_tag, typename std::iterator_traits< InputIt >::iterator_category >)
            {
                reserve(std::distance(first, last));
            }
            for (; first != last; ++first)
            {
                push_back(*first);
            }
        }

        /** @brief Replaces the contents from an initializer list. @param ilist Elements to copy. */
        void assign(std::initializer_list< T > ilist)
        {
            assign(ilist.begin(), ilist.end());
        }

        /** @brief Destroys the final element. @pre The container is not empty. */
        void pop_back()
        {
            --m_size;
            std::size_t segment_index = index_segment(m_size);
            std::size_t sub_index = index_subindex(m_size);
            using element_allocator_type = typename std::allocator_traits< Alloc >::template rebind_alloc< T >;
            using element_alloc_traits = std::allocator_traits< element_allocator_type >;
            element_allocator_type elem_alloc(m_alloc);
            T*& segment = m_segments[segment_index];
            element_alloc_traits::destroy(elem_alloc, &segment[sub_index]);
        }

        /**
         * @brief Releases segments that are not needed for the current size.
         * @note Retained elements are not moved, so their references remain valid.
         */
        void shrink_to_fit()
        {
            std::size_t required_segment_count = capacity_segment_count(m_size);
            if (required_segment_count < capacity_segment_count(m_capacity))
            {
                using segment_allocator_type = typename std::allocator_traits< Alloc >::template rebind_alloc< T* >;
                using segment_alloc_traits = std::allocator_traits< segment_allocator_type >;
                segment_allocator_type segment_allocator(m_alloc);

                using element_allocator_type = typename std::allocator_traits< Alloc >::template rebind_alloc< T >;
                using element_alloc_traits = std::allocator_traits< element_allocator_type >;
                element_allocator_type elem_alloc(m_alloc);

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

        /** @brief Destroys all elements while retaining allocated segments. */
        void clear()
        {
            while (size() > 0)
            {
                pop_back();
            }
        }

        /** @brief Destroys all elements and releases all allocated storage. */
        void reset()
        {
            clear();
            shrink_to_fit();
        }

        /// Destroys all elements and releases all segments.
        ~segmented_dynar()
        {
            reset();
        }

        /**
         * @brief Constructs an element at the end of the container.
         * @tparam Args Constructor argument types.
         * @param args Arguments forwarded to `T`'s constructor.
         * @return Reference to the constructed element.
         * @note Existing element references remain valid.
         */
        template < typename... Args >
        T& emplace_back(Args&&... args)
        {
            if (size() >= capacity())
            {
                reserve(capacity() + 1);
            }
            std::size_t insertion_index = m_size;
            assert(capacity() > insertion_index);
            std::size_t segment_index = index_segment(insertion_index);
            std::size_t sub_index = index_subindex(insertion_index);
            using element_allocator_type = typename std::allocator_traits< Alloc >::template rebind_alloc< T >;
            using element_alloc_traits = std::allocator_traits< element_allocator_type >;
            element_allocator_type elem_alloc(m_alloc);
            T*& segment = m_segments[segment_index];
            assert(sub_index < segment_size(segment_index));
            assert(segment != nullptr);
            element_alloc_traits::construct(elem_alloc, &segment[sub_index], std::forward< Args >(args)...);
            ++m_size;
            return segment[sub_index];
        }

        /** @brief Move-constructs by taking ownership of all segments. @param other Source container, left empty. */
        segmented_dynar(segmented_dynar&& other) noexcept : m_size(other.m_size), m_capacity(other.m_capacity), m_segments(other.m_segments), m_alloc(std::move(other.m_alloc))
        {
            other.m_size = 0;
            other.m_capacity = 0;
            other.m_segments = nullptr;
        }

        /** @brief Move-assigns subject to allocator propagation rules. @param other Source container, left empty after successful assignment. @return Reference to this container. */
        segmented_dynar& operator=(segmented_dynar&& other) noexcept(std::allocator_traits< Alloc >::propagate_on_container_move_assignment::value ? std::is_nothrow_move_assignable_v< Alloc > : std::allocator_traits< Alloc >::is_always_equal::value)
        {
            if (this != &other)
            {
                if constexpr (std::allocator_traits< Alloc >::propagate_on_container_move_assignment::value)
                {
                    reset();
                    m_alloc = std::move(other.m_alloc);
                    m_size = other.m_size;
                    m_capacity = other.m_capacity;
                    m_segments = other.m_segments;
                    other.m_size = 0;
                    other.m_capacity = 0;
                    other.m_segments = nullptr;
                }
                else if constexpr (std::allocator_traits< Alloc >::is_always_equal::value)
                {
                    reset();
                    m_size = other.m_size;
                    m_capacity = other.m_capacity;
                    m_segments = other.m_segments;
                    other.m_size = 0;
                    other.m_capacity = 0;
                    other.m_segments = nullptr;
                }
                else if (m_alloc == other.m_alloc)
                {
                    reset();
                    m_size = other.m_size;
                    m_capacity = other.m_capacity;
                    m_segments = other.m_segments;
                    other.m_size = 0;
                    other.m_capacity = 0;
                    other.m_segments = nullptr;
                }
                else
                {
                    segmented_dynar replacement(m_alloc);
                    replacement.reserve(other.size());
                    for (std::size_t i = 0; i < other.size(); ++i)
                    {
                        replacement.emplace_back(std::move(other[i]));
                    }

                    reset();
                    m_size = replacement.m_size;
                    m_capacity = replacement.m_capacity;
                    m_segments = replacement.m_segments;
                    replacement.m_size = 0;
                    replacement.m_capacity = 0;
                    replacement.m_segments = nullptr;
                    other.clear();
                }
            }
            return *this;
        }

        /** @brief Copy-constructs every element. @param other Container to copy. */
        segmented_dynar(const segmented_dynar& other) : m_alloc(std::allocator_traits< Alloc >::select_on_container_copy_construction(other.m_alloc))
        {
            try
            {
                reserve(other.m_size);
                for (std::size_t i = 0; i < other.m_size; ++i)
                {
                    push_back(other[i]);
                }
            }
            catch (...)
            {
                reset();
                throw;
            }
        }

        /** @brief Copy-assigns every element subject to allocator propagation rules. @param other Container to copy. @return Reference to this container. */
        segmented_dynar& operator=(const segmented_dynar& other)
        {
            if (this != &other)
            {
                if constexpr (std::allocator_traits< Alloc >::propagate_on_container_copy_assignment::value)
                {
                    if (m_alloc != other.m_alloc)
                    {
                        reset();
                    }
                    m_alloc = other.m_alloc;
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

        /**
         * @brief Random-access iterator over the segmented logical sequence.
         * @tparam Const Whether dereference yields an immutable reference.
         *
         * The iterator is non-owning. It caches the current segment while using
         * a global logical index for comparison and cross-segment movement.
         */
        template < bool Const >
        class iterator_impl
        {
          public:
            /// Iterator category for legacy algorithms.
            using iterator_category = std::random_access_iterator_tag;
            /// Signed iterator-distance type.
            using difference_type = std::ptrdiff_t;
            /// Iterated element type.
            using value_type = T;
            /// Mutable or immutable element pointer type.
            using pointer = std::conditional_t< Const, T const*, T* >;
            /// Mutable or immutable element reference type.
            using reference = std::conditional_t< Const, T const&, T& >;
            /// Pointer to the mutable or immutable owning container.
            using container_ptr = std::conditional_t< Const, const segmented_dynar*, segmented_dynar* >;

          private:
            container_ptr m_container = nullptr;
            std::size_t m_global_index = 0;
            pointer m_ptr = nullptr;
            pointer m_seg_begin = nullptr;
            pointer m_seg_end = nullptr;

            void load_segment_cache()
            {
                if (m_global_index == m_container->size())
                {
                    m_ptr = nullptr;
                    return;
                }

                std::size_t seg_idx = index_segment(m_global_index);
                std::size_t sub_idx = index_subindex(m_global_index);

                pointer segment_base = m_container->m_segments[seg_idx];
                std::size_t seg_sz = segment_size(seg_idx);

                m_ptr = segment_base + sub_idx;
                m_seg_begin = segment_base;
                m_seg_end = segment_base + seg_sz;
            }

          public:
            /// Constructs a singular iterator.
            iterator_impl() = default;

            /** @brief Constructs an iterator at a logical index. @param container Container to reference. @param index Index in `[0, container->size()]`. */
            iterator_impl(container_ptr container, std::size_t index) : m_container(container), m_global_index(index)
            {
                if (container && index < container->size())
                {
                    load_segment_cache();
                }
            }

            /** @brief Converts a mutable iterator to an immutable iterator. @tparam Const2 Source constness, required to be false. @param other Mutable iterator to copy. */
            template < bool Const2, typename = std::enable_if_t< Const && !Const2 > >
            iterator_impl(const iterator_impl< Const2 >& other) : m_container(other.m_container), m_global_index(other.m_global_index), m_ptr(other.m_ptr), m_seg_begin(other.m_seg_begin), m_seg_end(other.m_seg_end)
            {
            }

            /** @brief Dereferences the current position. @return Element reference. @pre The iterator is dereferenceable. */
            reference operator*() const
            {
                return *m_ptr;
            }
            /** @brief Accesses the current element. @return Element pointer. @pre The iterator is dereferenceable. */
            pointer operator->() const
            {
                return m_ptr;
            }

            /** @brief Advances one element. @return Reference to this iterator. @pre The iterator is not at `end()`. */
            iterator_impl& operator++()
            {
                ++m_ptr;
                ++m_global_index;
                if (m_ptr == m_seg_end)
                {
                    load_segment_cache();
                }
                return *this;
            }

            /** @brief Advances one element. @return Copy of the iterator before increment. @pre The iterator is not at `end()`. */
            iterator_impl operator++(int)
            {
                iterator_impl temp = *this;
                ++(*this);
                return temp;
            }

            /** @brief Retreats one element. @return Reference to this iterator. @pre The iterator is not at `begin()`. */
            iterator_impl& operator--()
            {
                if (m_ptr == m_seg_begin || m_global_index == m_container->size())
                {
                    --m_global_index;
                    load_segment_cache();
                }
                else
                {
                    --m_ptr;
                    --m_global_index;
                }
                return *this;
            }

            /** @brief Retreats one element. @return Copy of the iterator before decrement. @pre The iterator is not at `begin()`. */
            iterator_impl operator--(int)
            {
                iterator_impl temp = *this;
                --(*this);
                return temp;
            }

            /** @brief Moves by a signed offset. @param n Offset in elements. @return Reference to this iterator. @pre The resulting position belongs to the same container range. */
            iterator_impl& operator+=(difference_type n)
            {
                if (n == 0)
                    return *this;

                if (m_ptr != nullptr)
                {
                    if (n > 0 && n < m_seg_end - m_ptr)
                    {
                        m_ptr += n;
                        m_global_index += static_cast< std::size_t >(n);
                        return *this;
                    }
                    if (n < 0 && n >= -(m_ptr - m_seg_begin))
                    {
                        m_ptr += n;
                        m_global_index -= static_cast< std::size_t >(-(n + 1)) + 1;
                        return *this;
                    }
                }

                if (n > 0)
                {
                    m_global_index += static_cast< std::size_t >(n);
                }
                else
                {
                    m_global_index -= static_cast< std::size_t >(-(n + 1)) + 1;
                }
                load_segment_cache();
                return *this;
            }

            /** @brief Moves backward by a signed offset. @param n Offset in elements. @return Reference to this iterator. @pre The resulting position belongs to the same container range. */
            iterator_impl& operator-=(difference_type n)
            {
                if (n > 0)
                {
                    m_global_index -= static_cast< std::size_t >(n);
                }
                else if (n < 0)
                {
                    m_global_index += static_cast< std::size_t >(-(n + 1)) + 1;
                }
                load_segment_cache();
                return *this;
            }

            /** @brief Returns an iterator moved by an offset. @param n Offset in elements. @return Shifted iterator. */
            iterator_impl operator+(difference_type n) const
            {
                iterator_impl temp = *this;
                temp += n;
                return temp;
            }

            /** @brief Returns an iterator moved by a leading offset. @param n Offset in elements. @param iterator Iterator to move. @return Shifted iterator. */
            friend iterator_impl operator+(difference_type n, iterator_impl iterator)
            {
                iterator += n;
                return iterator;
            }

            /** @brief Returns an iterator moved backward by an offset. @param n Offset in elements. @return Shifted iterator. */
            iterator_impl operator-(difference_type n) const
            {
                iterator_impl temp = *this;
                temp -= n;
                return temp;
            }

            /** @brief Computes distance from an immutable iterator. @param other Iterator in the same container. @return Signed index difference. */
            difference_type operator-(const iterator_impl< true >& other) const
            {
                return static_cast< difference_type >(m_global_index) - static_cast< difference_type >(other.m_global_index);
            }

            /** @brief Computes distance from a mutable iterator. @param other Iterator in the same container. @return Signed index difference. */
            difference_type operator-(const iterator_impl< false >& other) const
            {
                return static_cast< difference_type >(m_global_index) - static_cast< difference_type >(other.m_global_index);
            }

            /** @brief Dereferences an offset position. @param n Offset from this iterator. @return Element reference. */
            reference operator[](difference_type n) const
            {
                return *(*this + n);
            }

            /** @brief Compares with an immutable iterator for equality. @param other Iterator in the same container. @return Index comparison result. */
            bool operator==(const iterator_impl< true >& other) const
            {
                return m_global_index == other.m_global_index;
            }

            /** @brief Compares with a mutable iterator for equality. @param other Iterator in the same container. @return Index comparison result. */
            bool operator==(const iterator_impl< false >& other) const
            {
                return m_global_index == other.m_global_index;
            }

            /** @brief Compares with an immutable iterator for inequality. @param other Iterator in the same container. @return Index comparison result. */
            bool operator!=(const iterator_impl< true >& other) const
            {
                return !(*this == other);
            }
            /** @brief Compares with a mutable iterator for inequality. @param other Iterator in the same container. @return Index comparison result. */
            bool operator!=(const iterator_impl< false >& other) const
            {
                return !(*this == other);
            }
            /** @brief Tests whether this index precedes an immutable iterator. @param other Iterator in the same container. @return Index comparison result. */
            bool operator<(const iterator_impl< true >& other) const
            {
                return m_global_index < other.m_global_index;
            }
            /** @brief Tests whether this index precedes a mutable iterator. @param other Iterator in the same container. @return Index comparison result. */
            bool operator<(const iterator_impl< false >& other) const
            {
                return m_global_index < other.m_global_index;
            }
            /** @brief Tests whether this index follows an immutable iterator. @param other Iterator in the same container. @return Index comparison result. */
            bool operator>(const iterator_impl< true >& other) const
            {
                return m_global_index > other.m_global_index;
            }
            /** @brief Tests whether this index follows a mutable iterator. @param other Iterator in the same container. @return Index comparison result. */
            bool operator>(const iterator_impl< false >& other) const
            {
                return m_global_index > other.m_global_index;
            }
            /** @brief Tests whether this index does not follow an immutable iterator. @param other Iterator in the same container. @return Index comparison result. */
            bool operator<=(const iterator_impl< true >& other) const
            {
                return m_global_index <= other.m_global_index;
            }
            /** @brief Tests whether this index does not follow a mutable iterator. @param other Iterator in the same container. @return Index comparison result. */
            bool operator<=(const iterator_impl< false >& other) const
            {
                return m_global_index <= other.m_global_index;
            }
            /** @brief Tests whether this index does not precede an immutable iterator. @param other Iterator in the same container. @return Index comparison result. */
            bool operator>=(const iterator_impl< true >& other) const
            {
                return m_global_index >= other.m_global_index;
            }
            /** @brief Tests whether this index does not precede a mutable iterator. @param other Iterator in the same container. @return Index comparison result. */
            bool operator>=(const iterator_impl< false >& other) const
            {
                return m_global_index >= other.m_global_index;
            }

            friend class iterator_impl< !Const >;
            friend class segmented_dynar;
        };

        /// Mutable random-access iterator type.
        using iterator = iterator_impl< false >;
        /// Immutable random-access iterator type.
        using const_iterator = iterator_impl< true >;

        /** @brief Returns an iterator to the first element. @return Mutable beginning iterator. */
        iterator begin()
        {
            return iterator(this, 0);
        }
        /** @brief Returns an iterator one past the final element. @return Mutable end iterator. */
        iterator end()
        {
            return iterator(this, m_size);
        }
        /** @brief Returns an immutable iterator to the first element. @return Immutable beginning iterator. */
        const_iterator begin() const
        {
            return const_iterator(this, 0);
        }
        /** @brief Returns an immutable iterator one past the final element. @return Immutable end iterator. */
        const_iterator end() const
        {
            return const_iterator(this, m_size);
        }
        /** @brief Returns an immutable iterator to the first element. @return Immutable beginning iterator. */
        const_iterator cbegin() const
        {
            return begin();
        }
        /** @brief Returns an immutable iterator one past the final element. @return Immutable end iterator. */
        const_iterator cend() const
        {
            return end();
        }
    };
} // namespace rpnx
#endif
