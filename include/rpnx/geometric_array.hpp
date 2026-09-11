// Copyright (c) 2026 Ryan P. Nicholl <rnicholl@protonmail.com>
// SPDX-License-Identifier: Apache-2.0
#ifndef RPNX_GEOMETRIC_ARRAY_HPP
#define RPNX_GEOMETRIC_ARRAY_HPP

#include <algorithm>
#include <bit>
#include <cassert>
#include <compare>
#include <concepts>
#include <cstddef>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace rpnx
{
    /**
     * @brief Vector-style sequence stored in segments of 1, 2, 4, 8, ... elements.
     *
     * Random access is O(1). Growth never relocates existing elements; pointers
     * and references survive append. Allocated storage always holds 2^k - 1 slots.
     * A std::vector stores the segment allocation pointers. The supplied
     * allocator manages elements; the pointer table uses its default allocator.
     * Removal automatically releases trailing segments when at least two are
     * fully empty, retaining one empty segment. No segment is allocated merely
     * to provide spare capacity. The pointer table retains its allocation.
     * Iterators are random-access, not contiguous. Capacity growth invalidates
     * iterators; removal preserves iterators/references before the removed
     * position. Insert and erase invalidate iterators/references at and after
     * the modified position. Explicit shrink_to_fit() releases all empty segments
     * and may invalidate iterators. Swap transfers iterators with the elements.
     * There is no data() member and bool stores actual bool objects.
     *
     * Reserve and copy assignment provide the strong exception guarantee.
     * Resize rolls back newly constructed elements on failure. Insertion and
     * erasure provide the basic guarantee when element moves/assignments throw.
     * Appending costs O(1) except at segment boundaries, where pointer-table
     * maintenance is O(log n). Middle insert/erase are linear in affected size.
     * @tparam T Element type.
     * @tparam Allocator Allocator for element storage.
     */
    template < typename T, typename Allocator = std::allocator< T > >
    class geometric_array
    {
        using alloc_traits = std::allocator_traits< Allocator >;
        static_assert(std::is_same_v< T, typename alloc_traits::value_type >);

      public:
        using value_type = T;
        using allocator_type = Allocator;
        using size_type = std::size_t;
        using difference_type = std::ptrdiff_t;
        using reference = T&;
        using const_reference = T const&;
        using pointer = typename alloc_traits::pointer;
        using const_pointer = typename alloc_traits::const_pointer;

      private:
        [[no_unique_address]] Allocator m_allocator;
        std::vector< pointer > m_segments;
        size_type m_size = 0;

        /** @brief Returns the total number of allocated element slots. */
        size_type capacity() const noexcept
        {
            return (size_type{1} << m_segments.size()) - 1;
        }
        /** @brief Allocates complete segments to cover count slots without moving elements. */
        void reserve(size_type count)
        {
            with_capacity(count,
                          [](pointer const*)
                          {
                          });
        }
        /** @brief Returns an address in allocated storage, including unconstructed slots. */
        static T* slot(pointer const* segments, size_type index) noexcept
        {
            size_type segment = std::bit_width(index + 1) - 1;
            return std::to_address(segments[segment]) + (index + 1 - (size_type{1} << segment));
        }

        /** @brief Destroys elements and releases all element allocations. */
        void release() noexcept
        {
            clear();
            for (size_type segment = 0; segment < m_segments.size(); ++segment)
            {
                alloc_traits::deallocate(m_allocator, m_segments[segment], size_type{1} << segment);
            }
            m_segments.clear();
        }

        /**
         * @brief Destroys a suffix and releases excess empty segments, retaining one.
         * @pre count <= size().
         * The pointer table retains its allocation so removal never allocates
         * and iterators to retained elements keep their table address.
         */
        void truncate(size_type count) noexcept
        {
            while (m_size > count)
            {
                --m_size;
                alloc_traits::destroy(m_allocator, slot(m_segments.data(), m_size));
            }
            shrink_if_oversized();
        }

        /** @brief Releases trailing unoccupied segments without reallocating the pointer table. */
        void truncate_segments(size_type retained) noexcept
        {
            while (m_segments.size() > retained)
            {
                alloc_traits::deallocate(m_allocator, m_segments.back(), size_type{1} << (m_segments.size() - 1));
                m_segments.pop_back();
            }
        }

        /** @brief Reclaims excess empty segments while retaining one, without allocating spare storage. */
        void shrink_if_oversized() noexcept
        {
            truncate_segments(std::bit_width(m_size) + 1);
        }

        /** @brief Exchanges storage without changing allocator ownership. */
        void swap_storage(geometric_array& other) noexcept
        {
            m_segments.swap(other.m_segments);
            std::swap(m_size, other.m_size);
        }

        /** @brief Rejects growth that exceeds the supported layout. */
        void check_growth(size_type count) const
        {
            if (count > max_size() - m_size)
            {
                throw std::length_error("geometric_array size exceeds max_size");
            }
        }

        /**
         * @brief Performs an operation with sufficient storage, committing growth only on success.
         * @param operation Receives the candidate pointer table and must roll back
         * any newly constructed elements before propagating an exception.
         */
        template < typename Operation >
        void with_capacity(size_type count, Operation operation)
        {
            if (count <= capacity())
            {
                operation(m_segments.data());
                return;
            }
            if (count > max_size())
            {
                throw std::length_error("geometric_array::reserve exceeds max_size");
            }
            size_type required = std::bit_width(count);
            std::vector< pointer > segments;
            segments.reserve(required);
            segments.insert(segments.end(), m_segments.begin(), m_segments.end());
            try
            {
                while (segments.size() < required)
                {
                    pointer allocation = alloc_traits::allocate(m_allocator, size_type{1} << segments.size());
                    segments.push_back(allocation);
                }
                operation(segments.data());
            }
            catch (...)
            {
                for (size_type segment = m_segments.size(); segment < segments.size(); ++segment)
                {
                    alloc_traits::deallocate(m_allocator, segments[segment], size_type{1} << segment);
                }
                throw;
            }
            m_segments.swap(segments);
        }

      public:
        /** @brief Random-access iterator over the segment table and a logical index. */
        template < bool Const >
        class iterator_impl
        {
            friend class geometric_array;
            template < bool >
            friend class iterator_impl;
            typename geometric_array::pointer const* m_segments = nullptr;
            size_type m_index = 0;

            /** @brief Constructs an iterator into a segment table. */
            iterator_impl(typename geometric_array::pointer const* segments, size_type index) noexcept : m_segments(segments), m_index(index)
            {
            }

          public:
            using value_type = T;
            using difference_type = std::ptrdiff_t;
            using reference = std::conditional_t< Const, T const&, T& >;
            using pointer = std::conditional_t< Const, T const*, T* >;
            using iterator_category = std::random_access_iterator_tag;
            using iterator_concept = std::random_access_iterator_tag;

            /** @brief Constructs a singular iterator. */
            iterator_impl() = default;
            /** @brief Converts a mutable iterator to an immutable iterator. */
            template < bool Other >
                requires(Const && !Other)
            iterator_impl(iterator_impl< Other > other) noexcept : m_segments(other.m_segments), m_index(other.m_index)
            {
            }
            /** @brief Accesses the current element. */
            reference operator*() const noexcept
            {
                return *geometric_array::slot(m_segments, m_index);
            }
            /** @brief Returns the current element's address. */
            pointer operator->() const noexcept
            {
                return std::addressof(**this);
            }
            /** @brief Accesses an element at a relative offset. */
            reference operator[](difference_type offset) const noexcept
            {
                return *(*this + offset);
            }
            /** @brief Advances one position. */
            iterator_impl& operator++() noexcept
            {
                ++m_index;
                return *this;
            }
            /** @brief Advances and returns the previous position. */
            iterator_impl operator++(int) noexcept
            {
                iterator_impl old = *this;
                ++*this;
                return old;
            }
            /** @brief Retreats one position. */
            iterator_impl& operator--() noexcept
            {
                --m_index;
                return *this;
            }
            /** @brief Retreats and returns the previous position. */
            iterator_impl operator--(int) noexcept
            {
                iterator_impl old = *this;
                --*this;
                return old;
            }
            /** @brief Advances by a signed offset within the sequence. */
            iterator_impl& operator+=(difference_type offset) noexcept
            {
                m_index += static_cast< size_type >(offset);
                return *this;
            }
            /** @brief Retreats by a signed offset within the sequence. */
            iterator_impl& operator-=(difference_type offset) noexcept
            {
                m_index -= static_cast< size_type >(offset);
                return *this;
            }
            /** @brief Returns an advanced iterator. */
            iterator_impl operator+(difference_type offset) const noexcept
            {
                iterator_impl result = *this;
                result += offset;
                return result;
            }
            /** @brief Returns a retreated iterator. */
            iterator_impl operator-(difference_type offset) const noexcept
            {
                iterator_impl result = *this;
                result -= offset;
                return result;
            }
            /** @brief Returns an advanced iterator with offset-first syntax. */
            friend iterator_impl operator+(difference_type offset, iterator_impl iter) noexcept
            {
                return iter + offset;
            }
            /** @brief Returns the signed distance between positions in the same sequence. */
            template < bool Other >
            difference_type operator-(iterator_impl< Other > other) const noexcept
            {
                return static_cast< difference_type >(m_index) - static_cast< difference_type >(other.m_index);
            }
            /** @brief Compares iterator identity. */
            template < bool Other >
            bool operator==(iterator_impl< Other > other) const noexcept
            {
                return m_segments == other.m_segments && m_index == other.m_index;
            }
            /** @brief Orders positions in the same sequence. */
            template < bool Other >
            std::strong_ordering operator<=>(iterator_impl< Other > other) const noexcept
            {
                return m_index <=> other.m_index;
            }
        };

        using iterator = iterator_impl< false >;
        using const_iterator = iterator_impl< true >;
        using reverse_iterator = std::reverse_iterator< iterator >;
        using const_reverse_iterator = std::reverse_iterator< const_iterator >;

        /** @brief Constructs an empty sequence. */
        geometric_array() noexcept(std::is_nothrow_default_constructible_v< Allocator >) = default;
        /** @brief Constructs an empty sequence with the specified allocator. */
        explicit geometric_array(Allocator const& allocator) noexcept : m_allocator(allocator)
        {
        }
        /** @brief Constructs count default-inserted elements. */
        explicit geometric_array(size_type count, Allocator const& allocator = Allocator()) : geometric_array(allocator)
        {
            resize(count);
        }
        /** @brief Constructs count copies of value. */
        geometric_array(size_type count, T const& value, Allocator const& allocator = Allocator()) : geometric_array(allocator)
        {
            resize(count, value);
        }
        /** @brief Constructs elements from an input range. */
        template < std::input_iterator InputIt >
        geometric_array(InputIt first, InputIt last, Allocator const& allocator = Allocator()) : geometric_array(allocator)
        {
            if constexpr (std::forward_iterator< InputIt >)
            {
                reserve(static_cast< size_type >(std::distance(first, last)));
            }
            for (; first != last; ++first)
            {
                emplace_back(*first);
            }
        }
        /** @brief Constructs copies of initializer-list elements. */
        geometric_array(std::initializer_list< T > values, Allocator const& allocator = Allocator()) : geometric_array(values.begin(), values.end(), allocator)
        {
        }
        /** @brief Copies elements using the allocator's copy-construction policy. */
        geometric_array(geometric_array const& other) : geometric_array(other.begin(), other.end(), alloc_traits::select_on_container_copy_construction(other.m_allocator))
        {
        }
        /** @brief Copies elements using a specified allocator. */
        geometric_array(geometric_array const& other, Allocator const& allocator) : geometric_array(other.begin(), other.end(), allocator)
        {
        }
        /** @brief Transfers storage and allocator ownership. */
        geometric_array(geometric_array&& other) noexcept : m_allocator(std::move(other.m_allocator)), m_segments(std::move(other.m_segments)), m_size(std::exchange(other.m_size, 0))
        {
            other.m_segments.clear();
        }
        /** @brief Transfers storage when allocators match, otherwise moves elements. */
        geometric_array(geometric_array&& other, Allocator const& allocator) : geometric_array(allocator)
        {
            if (m_allocator == other.m_allocator)
            {
                swap_storage(other);
            }
            else
            {
                reserve(other.size());
                for (T& value : other)
                {
                    emplace_back(std::move(value));
                }
                other.clear();
            }
        }
        /** @brief Destroys all elements and releases storage. */
        ~geometric_array()
        {
            release();
        }
        /** @brief Copies elements and applies allocator propagation. */
        geometric_array& operator=(geometric_array const& other)
        {
            if (this == &other)
            {
                return *this;
            }
            if constexpr (alloc_traits::propagate_on_container_copy_assignment::value)
            {
                geometric_array replacement(other, other.m_allocator);
                release();
                m_allocator = other.m_allocator;
                swap_storage(replacement);
            }
            else
            {
                geometric_array replacement(other, m_allocator);
                swap_storage(replacement);
            }
            return *this;
        }
        /** @brief Moves elements or transfers storage according to allocator propagation. */
        geometric_array& operator=(geometric_array&& other) noexcept(alloc_traits::propagate_on_container_move_assignment::value || alloc_traits::is_always_equal::value)
        {
            if (this == &other)
            {
                return *this;
            }
            if constexpr (alloc_traits::propagate_on_container_move_assignment::value)
            {
                release();
                m_allocator = std::move(other.m_allocator);
                swap_storage(other);
            }
            else if (m_allocator == other.m_allocator)
            {
                release();
                swap_storage(other);
            }
            else
            {
                geometric_array replacement(std::move(other), m_allocator);
                swap_storage(replacement);
            }
            return *this;
        }
        /** @brief Replaces elements with initializer-list copies. */
        geometric_array& operator=(std::initializer_list< T > values)
        {
            assign(values);
            return *this;
        }
        /** @brief Returns the element allocator. */
        allocator_type get_allocator() const noexcept
        {
            return m_allocator;
        }
        /** @brief Returns the number of constructed elements. */
        size_type size() const noexcept
        {
            return m_size;
        }
        /** @brief Reports whether the sequence is empty. */
        bool empty() const noexcept
        {
            return m_size == 0;
        }
        /** @brief Returns the largest complete geometric layout supported by the allocator and iterator distance. */
        size_type max_size() const noexcept
        {
            size_type limit = std::min(static_cast< size_type >(alloc_traits::max_size(m_allocator)), static_cast< size_type >(std::numeric_limits< difference_type >::max()));
            return std::bit_floor(limit + 1) - 1;
        }
        /**
         * @brief Releases every fully empty segment and compacts the pointer table.
         * Retained element addresses remain valid, but iterators may be invalidated.
         * An empty array releases all element storage; an occupied final segment
         * retains its full size.
         */
        void shrink_to_fit()
        {
            truncate_segments(std::bit_width(m_size));
            m_segments.shrink_to_fit();
        }
        /** @brief Accesses an element; index must be less than size(). */
        reference operator[](size_type index) noexcept
        {
            return *slot(m_segments.data(), index);
        }
        /** @brief Accesses an immutable element; index must be less than size(). */
        const_reference operator[](size_type index) const noexcept
        {
            return *slot(m_segments.data(), index);
        }
        /** @brief Accesses an element or throws std::out_of_range. */
        reference at(size_type index)
        {
            if (index >= m_size)
            {
                throw std::out_of_range("geometric_array::at");
            }
            return (*this)[index];
        }
        /** @brief Accesses an immutable element or throws std::out_of_range. */
        const_reference at(size_type index) const
        {
            if (index >= m_size)
            {
                throw std::out_of_range("geometric_array::at");
            }
            return (*this)[index];
        }
        /** @brief Returns the first element of a nonempty sequence. */
        reference front() noexcept
        {
            return (*this)[0];
        }
        /** @brief Returns the first immutable element of a nonempty sequence. */
        const_reference front() const noexcept
        {
            return (*this)[0];
        }
        /** @brief Returns the last element of a nonempty sequence. */
        reference back() noexcept
        {
            return (*this)[m_size - 1];
        }
        /** @brief Returns the last immutable element of a nonempty sequence. */
        const_reference back() const noexcept
        {
            return (*this)[m_size - 1];
        }
        /** @brief Returns the beginning iterator. */
        iterator begin() noexcept
        {
            return iterator(m_segments.data(), 0);
        }
        /** @brief Returns the immutable beginning iterator. */
        const_iterator begin() const noexcept
        {
            return const_iterator(m_segments.data(), 0);
        }
        /** @brief Returns the immutable beginning iterator. */
        const_iterator cbegin() const noexcept
        {
            return begin();
        }
        /** @brief Returns the past-the-end iterator. */
        iterator end() noexcept
        {
            return iterator(m_segments.data(), m_size);
        }
        /** @brief Returns the immutable past-the-end iterator. */
        const_iterator end() const noexcept
        {
            return const_iterator(m_segments.data(), m_size);
        }
        /** @brief Returns the immutable past-the-end iterator. */
        const_iterator cend() const noexcept
        {
            return end();
        }
        /** @brief Returns the reverse beginning iterator. */
        reverse_iterator rbegin() noexcept
        {
            return reverse_iterator(end());
        }
        /** @brief Returns the immutable reverse beginning iterator. */
        const_reverse_iterator rbegin() const noexcept
        {
            return const_reverse_iterator(end());
        }
        /** @brief Returns the immutable reverse beginning iterator. */
        const_reverse_iterator crbegin() const noexcept
        {
            return rbegin();
        }
        /** @brief Returns the reverse end iterator. */
        reverse_iterator rend() noexcept
        {
            return reverse_iterator(begin());
        }
        /** @brief Returns the immutable reverse end iterator. */
        const_reverse_iterator rend() const noexcept
        {
            return const_reverse_iterator(begin());
        }
        /** @brief Returns the immutable reverse end iterator. */
        const_reverse_iterator crend() const noexcept
        {
            return rend();
        }
        /** @brief Constructs a new last element without relocating existing elements. */
        template < typename... Args >
        reference emplace_back(Args&&... args)
        {
            check_growth(1);
            with_capacity(m_size + 1,
                          [&](pointer const* segments)
                          {
                              alloc_traits::construct(m_allocator, slot(segments, m_size), std::forward< Args >(args)...);
                          });
            ++m_size;
            return back();
        }
        /** @brief Appends a copy of value. */
        void push_back(T const& value)
        {
            emplace_back(value);
        }
        /** @brief Appends a moved value. */
        void push_back(T&& value)
        {
            emplace_back(std::move(value));
        }
        /** @brief Destroys the final element of a nonempty sequence. */
        void pop_back() noexcept
        {
            assert(m_size != 0);
            truncate(m_size - 1);
        }
        /** @brief Destroys all elements, retaining at most the first single-element segment. */
        void clear() noexcept
        {
            truncate(0);
        }
        /** @brief Changes size, default-inserting new elements with rollback on failure. */
        void resize(size_type count)
        {
            size_type old_size = m_size;
            with_capacity(count,
                          [&](pointer const* segments)
                          {
                              try
                              {
                                  while (m_size < count)
                                  {
                                      alloc_traits::construct(m_allocator, slot(segments, m_size));
                                      ++m_size;
                                  }
                              }
                              catch (...)
                              {
                                  while (m_size > old_size)
                                  {
                                      --m_size;
                                      alloc_traits::destroy(m_allocator, slot(segments, m_size));
                                  }
                                  throw;
                              }
                          });
            if (m_size > count)
            {
                truncate(count);
            }
        }
        /** @brief Changes size, copying value into new elements with rollback on failure. */
        void resize(size_type count, T const& value)
        {
            size_type old_size = m_size;
            with_capacity(count,
                          [&](pointer const* segments)
                          {
                              try
                              {
                                  while (m_size < count)
                                  {
                                      alloc_traits::construct(m_allocator, slot(segments, m_size), value);
                                      ++m_size;
                                  }
                              }
                              catch (...)
                              {
                                  while (m_size > old_size)
                                  {
                                      --m_size;
                                      alloc_traits::destroy(m_allocator, slot(segments, m_size));
                                  }
                                  throw;
                              }
                          });
            if (m_size > count)
            {
                truncate(count);
            }
        }
        /** @brief Replaces elements with count copies, including when value aliases an element. */
        void assign(size_type count, T const& value)
        {
            geometric_array replacement(count, value, m_allocator);
            swap_storage(replacement);
        }
        /** @brief Replaces elements with an input range. */
        template < std::input_iterator InputIt >
        void assign(InputIt first, InputIt last)
        {
            geometric_array replacement(first, last, m_allocator);
            swap_storage(replacement);
        }
        /** @brief Replaces elements with initializer-list copies. */
        void assign(std::initializer_list< T > values)
        {
            assign(values.begin(), values.end());
        }
        /** @brief Constructs an element at position, shifting subsequent elements right. */
        template < typename... Args >
        iterator emplace(const_iterator position, Args&&... args)
        {
            size_type index = position.m_index;
            check_growth(1);
            emplace_back(std::forward< Args >(args)...);
            std::rotate(begin() + static_cast< difference_type >(index), end() - 1, end());
            return begin() + static_cast< difference_type >(index);
        }
        /** @brief Inserts a copy before position. */
        iterator insert(const_iterator position, T const& value)
        {
            return emplace(position, value);
        }
        /** @brief Inserts a moved value before position. */
        iterator insert(const_iterator position, T&& value)
        {
            return emplace(position, std::move(value));
        }
        /** @brief Inserts count copies before position in linear time. */
        iterator insert(const_iterator position, size_type count, T const& value)
        {
            size_type index = position.m_index;
            check_growth(count);
            size_type old_size = m_size;
            resize(m_size + count, value);
            if (count != 0)
            {
                std::rotate(begin() + static_cast< difference_type >(index), begin() + static_cast< difference_type >(old_size), end());
            }
            return begin() + static_cast< difference_type >(index);
        }
        /** @brief Inserts an input range before position; stages input before shifting elements. */
        template < std::input_iterator InputIt >
        iterator insert(const_iterator position, InputIt first, InputIt last)
        {
            size_type index = position.m_index;
            geometric_array incoming(first, last, m_allocator);
            check_growth(incoming.size());
            size_type old_size = m_size;
            with_capacity(m_size + incoming.size(),
                          [&](pointer const* segments)
                          {
                              try
                              {
                                  for (T& value : incoming)
                                  {
                                      alloc_traits::construct(m_allocator, slot(segments, m_size), std::move_if_noexcept(value));
                                      ++m_size;
                                  }
                              }
                              catch (...)
                              {
                                  while (m_size > old_size)
                                  {
                                      --m_size;
                                      alloc_traits::destroy(m_allocator, slot(segments, m_size));
                                  }
                                  throw;
                              }
                          });
            if (!incoming.empty())
            {
                std::rotate(begin() + static_cast< difference_type >(index), begin() + static_cast< difference_type >(old_size), end());
            }
            return begin() + static_cast< difference_type >(index);
        }
        /** @brief Inserts initializer-list copies before position. */
        iterator insert(const_iterator position, std::initializer_list< T > values)
        {
            return insert(position, values.begin(), values.end());
        }
        /** @brief Erases the element at position. */
        iterator erase(const_iterator position)
        {
            return erase(position, position + 1);
        }
        /** @brief Erases a range and shifts subsequent elements left. */
        iterator erase(const_iterator first, const_iterator last)
        {
            size_type index = first.m_index;
            size_type count = last.m_index - index;
            if (count != 0)
            {
                for (size_type source = last.m_index; source < m_size; ++source)
                {
                    (*this)[source - count] = std::move((*this)[source]);
                }
                truncate(m_size - count);
            }
            return begin() + static_cast< difference_type >(index);
        }
        /** @brief Swaps sequences; nonpropagating allocators must compare equal. */
        void swap(geometric_array& other) noexcept
        {
            if constexpr (alloc_traits::propagate_on_container_swap::value)
            {
                std::swap(m_allocator, other.m_allocator);
            }
            else
            {
                assert(m_allocator == other.m_allocator);
            }
            swap_storage(other);
        }
        /** @brief Swaps sequences using argument-dependent lookup. */
        friend void swap(geometric_array& left, geometric_array& right) noexcept
        {
            left.swap(right);
        }
        /** @brief Compares element sequences for equality. */
        friend bool operator==(geometric_array const& left, geometric_array const& right)
        {
            return left.size() == right.size() && std::equal(left.begin(), left.end(), right.begin());
        }
        /** @brief Compares sequences lexicographically, including types with only legacy ordering. */
        friend auto operator<=>(geometric_array const& left, geometric_array const& right)
        {
            return std::lexicographical_compare_three_way(left.begin(), left.end(), right.begin(), right.end(),
                                                          [](T const& a, T const& b)
                                                          {
                                                              if constexpr (std::three_way_comparable< T >)
                                                              {
                                                                  return a <=> b;
                                                              }
                                                              else
                                                              {
                                                                  if (a < b)
                                                                  {
                                                                      return std::weak_ordering::less;
                                                                  }
                                                                  if (b < a)
                                                                  {
                                                                      return std::weak_ordering::greater;
                                                                  }
                                                                  return std::weak_ordering::equivalent;
                                                              }
                                                          });
        }
    };

    /** @brief Deduces the element type from an iterator range. */
    template < std::input_iterator InputIt, typename Allocator = std::allocator< typename std::iterator_traits< InputIt >::value_type > >
    geometric_array(InputIt, InputIt, Allocator = Allocator()) -> geometric_array< typename std::iterator_traits< InputIt >::value_type, Allocator >;

    /** @brief Erases all elements equal to value and returns the number removed. */
    template < typename T, typename Allocator, typename U >
    typename geometric_array< T, Allocator >::size_type erase(geometric_array< T, Allocator >& values, U const& value)
    {
        typename geometric_array< T, Allocator >::iterator first = std::remove(values.begin(), values.end(), value);
        std::size_t removed = static_cast< std::size_t >(values.end() - first);
        values.erase(first, values.end());
        return removed;
    }

    /** @brief Erases all elements satisfying predicate and returns the number removed. */
    template < typename T, typename Allocator, typename Predicate >
    typename geometric_array< T, Allocator >::size_type erase_if(geometric_array< T, Allocator >& values, Predicate predicate)
    {
        typename geometric_array< T, Allocator >::iterator first = std::remove_if(values.begin(), values.end(), predicate);
        std::size_t removed = static_cast< std::size_t >(values.end() - first);
        values.erase(first, values.end());
        return removed;
    }
} // namespace rpnx
#endif
