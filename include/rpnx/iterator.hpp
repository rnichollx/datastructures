// Copyright (c) 2026 Ryan P. Nicholl

#ifndef RPNX_ITERATOR_HPP
#define RPNX_ITERATOR_HPP

#include <iterator>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace rpnx
{
    /**
     * @brief Forward iterator adapter that checks access against an end sentinel.
     *
     * The adapter turns dereferencing or incrementing an end iterator into a
     * deterministic `std::out_of_range` exception. The wrapped iterator and
     * sentinel must belong to the same underlying range and must outlive this
     * non-owning adapter.
     *
     * @tparam It Forward iterator type to wrap.
     */
    template < class It >
    class bounded_iterator
    {
        using traits = std::iterator_traits< It >;

        static_assert(std::is_base_of_v< std::forward_iterator_tag, typename traits::iterator_category >, "rpnx::bounded_iterator requires a forward iterator");

      public:
        /// Wrapped iterator type.
        using iterator_type = It;
        /// Iterator category exposed by the wrapped iterator.
        using iterator_category = typename traits::iterator_category;
        /// Iterated value type.
        using value_type = typename traits::value_type;
        /// Signed iterator-distance type.
        using difference_type = typename traits::difference_type;
        /// Pointer type returned by member access.
        using pointer = typename traits::pointer;
        /// Reference type returned by dereference.
        using reference = typename traits::reference;

        /// Constructs a value-initialized iterator and sentinel.
        bounded_iterator() = default;

        /**
         * @brief Constructs an adapter for a current position and end sentinel.
         * @param current Current iterator position.
         * @param last End sentinel for the same range.
         */
        constexpr bounded_iterator(It current, It last) : m_current(current), m_last(last)
        {
        }

        /** @brief Dereferences the current position. @return Referenced element. @throws std::out_of_range if positioned at the end sentinel. */
        constexpr reference operator*() const
        {
            if (m_current == m_last) [[unlikely]]
            {
                throw std::out_of_range("rpnx::bounded_iterator: dereference at end");
            }
            return *m_current;
        }

        /** @brief Accesses the current element. @return Pointer to the current element. @throws std::out_of_range if positioned at the end sentinel. */
        constexpr pointer operator->() const
        {
            if (m_current == m_last) [[unlikely]]
            {
                throw std::out_of_range("rpnx::bounded_iterator: dereference at end");
            }
            return std::addressof(*m_current);
        }

        /** @brief Advances to the next position. @return Reference to this iterator. @throws std::out_of_range if already at the end sentinel. */
        constexpr bounded_iterator& operator++()
        {
            if (m_current == m_last) [[unlikely]]
            {
                throw std::out_of_range("rpnx::bounded_iterator: ++ at end");
            }
            ++m_current;
            return *this;
        }

        /** @brief Advances to the next position. @return Copy of the iterator before increment. @throws std::out_of_range if already at the end sentinel. */
        constexpr bounded_iterator operator++(int)
        {
            bounded_iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        /** @brief Compares current positions. @param rhs Iterator to compare. @return `true` when both wrap equal current iterators. */
        constexpr bool operator==(const bounded_iterator& rhs) const
        {
            return m_current == rhs.m_current;
        }

        /** @brief Compares current positions for inequality. @param rhs Iterator to compare. @return Negation of equality. */
        constexpr bool operator!=(const bounded_iterator& rhs) const
        {
            return !(*this == rhs);
        }

      private:
        It m_current{};
        It m_last{};
    };

    /**
     * @brief Random-access iterator adapter with bidirectional range checks.
     *
     * Dereference and movement operations validate the closed position range
     * `[first, last]`, with `last` retained as a non-dereferenceable end
     * position. All three wrapped iterators must belong to the same range and
     * must outlive this non-owning adapter.
     *
     * @tparam It Random-access iterator type to wrap.
     */
    template < class It >
    class bidirectional_bounded_iterator
    {
        using traits = std::iterator_traits< It >;

        static_assert(std::is_base_of_v< std::random_access_iterator_tag, typename traits::iterator_category >, "rpnx::bounded_iterator requires a random-access iterator");

      public:
        /// Wrapped iterator type.
        using iterator_type = It;
        /// Iterator category exposed by the wrapped iterator.
        using iterator_category = typename traits::iterator_category;
        /// Iterated value type.
        using value_type = typename traits::value_type;
        /// Signed iterator-distance type.
        using difference_type = typename traits::difference_type;
        /// Pointer type returned by member access.
        using pointer = typename traits::pointer;
        /// Reference type returned by dereference.
        using reference = typename traits::reference;

        /// Constructs value-initialized current, first, and last iterators.
        bidirectional_bounded_iterator() = default;

        /**
         * @brief Constructs a bounded adapter over an underlying range.
         * @param current Current position in `[first, last]`.
         * @param first First dereferenceable position.
         * @param last Non-dereferenceable end position.
         * @pre All iterators refer to the same range.
         */
        constexpr bidirectional_bounded_iterator(It current, It first, It last) : m_current(current), m_first(first), m_last(last)
        {
        }

        /** @brief Dereferences the current position. @return Referenced element. @throws std::out_of_range if the current position is outside `[first, last)`. */
        constexpr reference operator*() const
        {
            if (!(m_current >= m_first && m_current < m_last)) [[unlikely]]
            {
                throw std::out_of_range("rpnx::bounded_iterator: dereference out of range");
            }
            return *m_current;
        }

        /** @brief Accesses the current element. @return Pointer to the current element. @throws std::out_of_range if the current position is outside `[first, last)`. */
        constexpr pointer operator->() const
        {
            if (!(m_current >= m_first && m_current < m_last)) [[unlikely]]
            {
                throw std::out_of_range("rpnx::bounded_iterator: dereference out of range");
            }
            return std::addressof(*m_current);
        }

        /** @brief Advances one position. @return Reference to this iterator. @throws std::out_of_range if already at or beyond `last`. */
        constexpr bidirectional_bounded_iterator& operator++()
        {
            if (!(m_current < m_last)) [[unlikely]]
            {
                throw std::out_of_range("rpnx::bounded_iterator: ++ past end");
            }
            ++m_current;
            return *this;
        }

        /** @brief Advances one position. @return Copy of the iterator before increment. @throws std::out_of_range if already at or beyond `last`. */
        constexpr bidirectional_bounded_iterator operator++(int)
        {
            bidirectional_bounded_iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        /** @brief Retreats one position. @return Reference to this iterator. @throws std::out_of_range if already at or before `first`. */
        constexpr bidirectional_bounded_iterator& operator--()
        {
            if (!(m_current > m_first)) [[unlikely]]
            {
                throw std::out_of_range("rpnx::bounded_iterator: -- before begin");
            }
            --m_current;
            return *this;
        }

        /** @brief Retreats one position. @return Copy of the iterator before decrement. @throws std::out_of_range if already at or before `first`. */
        constexpr bidirectional_bounded_iterator operator--(int)
        {
            bidirectional_bounded_iterator tmp = *this;
            --(*this);
            return tmp;
        }

        /** @brief Moves by an offset with range checking. @param n Signed offset. @return Reference to this iterator. @throws std::out_of_range if the result is outside `[first, last]`. */
        constexpr bidirectional_bounded_iterator& operator+=(difference_type n)
        {
            if (n >= 0)
            {
                auto max = std::distance(m_current, m_last);
                if (n > max) [[unlikely]]
                {
                    throw std::out_of_range("rpnx::bounded_iterator: += past end");
                }
                m_current += n;
                return *this;
            }
            else
            {
                auto max = std::distance(m_first, m_current);
                if (n < -max) [[unlikely]]
                {
                    throw std::out_of_range("rpnx::bounded_iterator: += before begin");
                }
                m_current += n;
                return *this;
            }
        }

        /** @brief Moves backward by an offset with range checking. @param n Signed offset. @return Reference to this iterator. @throws std::out_of_range if the result is outside `[first, last]`. */
        constexpr bidirectional_bounded_iterator& operator-=(difference_type n)
        {
            return (*this) += (-n);
        }

        /** @brief Returns an iterator moved by an offset. @param n Signed offset. @return Shifted iterator. @throws std::out_of_range if the result is outside `[first, last]`. */
        constexpr bidirectional_bounded_iterator operator+(difference_type n) const
        {
            bidirectional_bounded_iterator tmp = *this;
            tmp += n;
            return tmp;
        }

        /** @brief Returns an iterator moved by an offset. @param n Signed offset. @param it Iterator to shift. @return Shifted iterator. */
        friend constexpr bidirectional_bounded_iterator operator+(difference_type n, const bidirectional_bounded_iterator& it)
        {
            return it + n;
        }

        /** @brief Returns an iterator moved backward by an offset. @param n Signed offset. @return Shifted iterator. */
        constexpr bidirectional_bounded_iterator operator-(difference_type n) const
        {
            bidirectional_bounded_iterator tmp = *this;
            tmp -= n;
            return tmp;
        }

        /** @brief Dereferences an offset position. @param n Signed offset. @return Referenced element. @throws std::out_of_range if the offset position is not dereferenceable. */
        constexpr reference operator[](difference_type n) const
        {
            return *(*this + n);
        }

        /** @brief Computes the distance from another iterator. @param other Iterator in the same range. @return Signed positional difference. */
        constexpr difference_type operator-(const bidirectional_bounded_iterator& other) const
        {
            return m_current - other.m_current;
        }

        /** @brief Compares positions for equality. @param other Iterator to compare. @return `true` when positions are equal. */
        constexpr bool operator==(const bidirectional_bounded_iterator& other) const
        {
            return m_current == other.m_current;
        }

        /** @brief Compares positions for inequality. @param other Iterator to compare. @return `true` when positions differ. */
        constexpr bool operator!=(const bidirectional_bounded_iterator& other) const
        {
            return !(*this == other);
        }

        /** @brief Tests whether this position precedes another. @param other Iterator in the same range. @return Ordering result. */
        constexpr bool operator<(const bidirectional_bounded_iterator& other) const
        {
            return m_current < other.m_current;
        }

        /** @brief Tests whether this position follows another. @param other Iterator in the same range. @return Ordering result. */
        constexpr bool operator>(const bidirectional_bounded_iterator& other) const
        {
            return other < *this;
        }
        /** @brief Tests whether this position does not follow another. @param other Iterator in the same range. @return Ordering result. */
        constexpr bool operator<=(const bidirectional_bounded_iterator& other) const
        {
            return !(other < *this);
        }
        /** @brief Tests whether this position does not precede another. @param other Iterator in the same range. @return Ordering result. */
        constexpr bool operator>=(const bidirectional_bounded_iterator& other) const
        {
            return !(*this < other);
        }

      private:
        It m_current{};
        It m_first{};
        It m_last{};
    };

    /** @brief Creates a two-sided bounded random-access iterator. @tparam It Underlying iterator type. @param it Current position. @param first First position. @param last One-past-last position. @return Bounded adapter. */
    template < class It >
    constexpr bidirectional_bounded_iterator< It > make_bounded_iterator(It it, It first, It last) noexcept
    {
        return bidirectional_bounded_iterator< It >(it, first, last);
    }

    /** @brief Creates an upper-bounded forward iterator. @tparam It Underlying iterator type. @param it Current position. @param last One-past-last position. @return Bounded adapter. */
    template < class It >
    constexpr bounded_iterator< It > make_bounded_iterator(It it, It last) noexcept
    {
        return bounded_iterator< It >(it, last);
    }
} // namespace rpnx

#endif // RPNX_ITERATOR_HPP
