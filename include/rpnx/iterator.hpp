// Copyright (c) 2026 Ryan P. Nicholl

#ifndef RPNX_ITERATOR_HPP
#define RPNX_ITERATOR_HPP

#include <iterator>
#include <type_traits>
#include <stdexcept>
#include <utility>

namespace rpnx
{
    // A lightweight forward iterator wrapper that is bounded to [first, last).
    // Unlike rpnx::bounded_iterator, this only provides forward-iterator operations
    // and comparison via operator!= (sufficient for typical single-pass loops).
    template <class It>
    class forward_bounded_iterator
    {
        using traits = std::iterator_traits<It>;

        static_assert(std::is_base_of_v<std::forward_iterator_tag, typename traits::iterator_category>,
                      "rpnx::forward_bounded_iterator requires a forward iterator");

    public:
        using iterator_type = It;
        using iterator_category = typename traits::iterator_category;
        using value_type = typename traits::value_type;
        using difference_type = typename traits::difference_type;
        using pointer = typename traits::pointer;
        using reference = typename traits::reference;

        forward_bounded_iterator() = default;

        constexpr forward_bounded_iterator(It current, It last)
            : m_current(current), m_last(last)
        {
        }


        // Dereference (bounded: cannot dereference end sentinel)
        constexpr reference operator*() const
        {
            if (m_current == m_last)
            {
                throw std::out_of_range("rpnx::forward_bounded_iterator: dereference at end");
            }
            return *m_current;
        }

        constexpr pointer operator->() const
        {
            if (m_current == m_last)
            {
                throw std::out_of_range("rpnx::forward_bounded_iterator: dereference at end");
            }
            return std::addressof(*m_current);
        }

        // Increment (bounded: cannot increment at end)
        constexpr forward_bounded_iterator& operator++()
        {
            if (m_current == m_last)
            {
                throw std::out_of_range("rpnx::forward_bounded_iterator: ++ at end");
            }
            ++m_current;
            return *this;
        }

        constexpr forward_bounded_iterator operator++(int)
        {
            forward_bounded_iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        constexpr bool operator==(const forward_bounded_iterator& rhs) const
        {
            return m_current == rhs.m_current;
        }

        constexpr bool operator!=(const forward_bounded_iterator& rhs) const { return m_current != rhs.m_current; }

    private:
        It m_current{};
        It m_last{};
    };

    // A lightweight wrapper that bounds a random-access iterator `It` to the half-open range [first, last).
    // The iterator models the same category as `It` and forwards operations while asserting
    // that movements stay within the specified bounds in debug builds.
    template <class It>
    class bounded_iterator
    {
        using traits = std::iterator_traits<It>;

        static_assert(std::is_base_of_v<std::random_access_iterator_tag, typename traits::iterator_category>,
                      "rpnx::bounded_iterator requires a random-access iterator");

    public:
        using iterator_type = It;
        using iterator_category = typename traits::iterator_category;
        using value_type = typename traits::value_type;
        using difference_type = typename traits::difference_type;
        using pointer = typename traits::pointer;
        using reference = typename traits::reference;

        bounded_iterator() = default;

        // Construct from current, first, last — all must be from the same underlying range
        constexpr bounded_iterator(It current, It first, It last)
            : m_current(current), m_first(first), m_last(last)
        {
            if (!(m_first <= m_last))
            {
                throw std::out_of_range("rpnx::bounded_iterator: invalid bounds (first > last)");
            }
            if (!(m_current >= m_first && m_current <= m_last))
            {
                throw std::out_of_range("rpnx::bounded_iterator: current not within [first,last]");
            }
        }


        // Dereference
        constexpr reference operator*() const
        {
            if (!(m_current >= m_first && m_current < m_last))
            {
                throw std::out_of_range("rpnx::bounded_iterator: dereference out of range");
            }
            return *m_current;
        }

        constexpr pointer operator->() const
        {
            if (!(m_current >= m_first && m_current < m_last))
            {
                throw std::out_of_range("rpnx::bounded_iterator: dereference out of range");
            }
            return std::addressof(*m_current);
        }

        // Increment / Decrement
        constexpr bounded_iterator& operator++()
        {
            if (!(m_current < m_last))
            {
                throw std::out_of_range("rpnx::bounded_iterator: ++ past end");
            }
            ++m_current;
            return *this;
        }

        constexpr bounded_iterator operator++(int)
        {
            bounded_iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        constexpr bounded_iterator& operator--()
        {
            if (!(m_current > m_first))
            {
                throw std::out_of_range("rpnx::bounded_iterator: -- before begin");
            }
            --m_current;
            return *this;
        }

        constexpr bounded_iterator operator--(int)
        {
            bounded_iterator tmp = *this;
            --(*this);
            return tmp;
        }

        // Random access helpers
        constexpr bounded_iterator& operator+=(difference_type n)
        {
            if (!((n >= 0 && m_current + n <= m_last) || (n < 0 && m_current - (-n) >= m_first)))
            {
                throw std::out_of_range("rpnx::bounded_iterator: addition moves out of bounds");
            }
            m_current += n;
            return *this;
        }

        constexpr bounded_iterator& operator-=(difference_type n)
        {
            return (*this) += (-n);
        }

        constexpr bounded_iterator operator+(difference_type n) const
        {
            bounded_iterator tmp = *this;
            tmp += n;
            return tmp;
        }

        friend constexpr bounded_iterator operator+(difference_type n, const bounded_iterator& it)
        {
            return it + n;
        }

        constexpr bounded_iterator operator-(difference_type n) const
        {
            bounded_iterator tmp = *this;
            tmp -= n;
            return tmp;
        }

        constexpr reference operator[](difference_type n) const
        {
            return *(*this + n);
        }

        // Distance between two bounded iterators (must share the same bounds)
        constexpr difference_type operator-(const bounded_iterator& other) const
        {
            if (!(m_first == other.m_first && m_last == other.m_last))
            {
                throw std::out_of_range("rpnx::bounded_iterator: subtraction across different bounds");
            }
            return m_current - other.m_current;
        }

        // Comparisons compare the current positions. Bounds must match; otherwise throw.
        constexpr bool operator==(const bounded_iterator& other) const
        {
            if (!(m_first == other.m_first && m_last == other.m_last))
            {
                throw std::out_of_range("rpnx::bounded_iterator: comparison across different bounds");
            }
            return m_current == other.m_current;
        }

        constexpr bool operator!=(const bounded_iterator& other) const { return !(*this == other); }

        constexpr bool operator<(const bounded_iterator& other) const
        {
            if (!(m_first == other.m_first && m_last == other.m_last))
            {
                throw std::out_of_range("rpnx::bounded_iterator: comparison across different bounds");
            }
            return m_current < other.m_current;
        }

        constexpr bool operator>(const bounded_iterator& other) const { return other < *this; }
        constexpr bool operator<=(const bounded_iterator& other) const { return !(other < *this); }
        constexpr bool operator>=(const bounded_iterator& other) const { return !(*this < other); }

    private:
        It m_current{};
        It m_first{};
        It m_last{};
    };

    template <class It>
    constexpr bounded_iterator<It> make_bounded_iterator(It it, It first, It last) noexcept
    {
        return bounded_iterator<It>(it, first, last);
    }

    template <class It>
    constexpr forward_bounded_iterator<It> make_forward_bounded_iterator(It it, It last) noexcept
    {
        return forward_bounded_iterator<It>(it, last);
    }
}

#endif // RPNX_ITERATOR_HPP
