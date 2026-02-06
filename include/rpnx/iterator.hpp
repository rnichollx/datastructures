// Copyright (c) 2026 Ryan P. Nicholl

#ifndef RPNX_ITERATOR_HPP
#define RPNX_ITERATOR_HPP

#include <iterator>
#include <type_traits>
#include <stdexcept>
#include <utility>

namespace rpnx
{
    // A lightweight forward iterator wrapper has security range checking.
    // It will throw an exception if used illegally, instead of undefined behavior.
    template <class It>
    class bounded_iterator
    {
        using traits = std::iterator_traits<It>;

        static_assert(std::is_base_of_v<std::forward_iterator_tag, typename traits::iterator_category>,
                      "rpnx::bounded_iterator requires a forward iterator");

    public:
        using iterator_type = It;
        using iterator_category = typename traits::iterator_category;
        using value_type = typename traits::value_type;
        using difference_type = typename traits::difference_type;
        using pointer = typename traits::pointer;
        using reference = typename traits::reference;

        bounded_iterator() = default;

        constexpr bounded_iterator(It current, It last)
            : m_current(current), m_last(last)
        {
        }


        // Dereference (bounded: cannot dereference end sentinel)
        constexpr reference operator*() const
        {
            if (m_current == m_last) [[unlikely]]
            {
                throw std::out_of_range("rpnx::bounded_iterator: dereference at end");
            }
            return *m_current;
        }

        constexpr pointer operator->() const
        {
            if (m_current == m_last) [[unlikely]]
            {
                throw std::out_of_range("rpnx::bounded_iterator: dereference at end");
            }
            return std::addressof(*m_current);
        }

        // Increment (bounded: cannot increment at end)
        constexpr bounded_iterator& operator++()
        {
            if (m_current == m_last) [[unlikely]]
            {
                throw std::out_of_range("rpnx::bounded_iterator: ++ at end");
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

        constexpr bool operator==(const bounded_iterator& rhs) const
        {
            return m_current == rhs.m_current;
        }

        constexpr bool operator!=(const bounded_iterator& rhs) const { return !(*this == rhs); }

    private:
        It m_current{};
        It m_last{};
    };

    //
    template <class It>
    class bidirectional_bounded_iterator
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

        bidirectional_bounded_iterator() = default;

        // Construct from current, first, last — all must be from the same underlying range
        constexpr bidirectional_bounded_iterator(It current, It first, It last)
            : m_current(current), m_first(first), m_last(last)
        {
        }


        // Dereference
        constexpr reference operator*() const
        {
            if (!(m_current >= m_first && m_current < m_last)) [[unlikely]]
            {
                throw std::out_of_range("rpnx::bounded_iterator: dereference out of range");
            }
            return *m_current;
        }

        constexpr pointer operator->() const
        {
            if (!(m_current >= m_first && m_current < m_last)) [[unlikely]]
            {
                throw std::out_of_range("rpnx::bounded_iterator: dereference out of range");
            }
            return std::addressof(*m_current);
        }

        // Increment / Decrement
        constexpr bidirectional_bounded_iterator& operator++()
        {
            if (!(m_current < m_last)) [[unlikely]]
            {
                throw std::out_of_range("rpnx::bounded_iterator: ++ past end");
            }
            ++m_current;
            return *this;
        }

        constexpr bidirectional_bounded_iterator operator++(int)
        {
            bidirectional_bounded_iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        constexpr bidirectional_bounded_iterator& operator--()
        {
            if (!(m_current > m_first)) [[unlikely]]
            {
                throw std::out_of_range("rpnx::bounded_iterator: -- before begin");
            }
            --m_current;
            return *this;
        }

        constexpr bidirectional_bounded_iterator operator--(int)
        {
            bidirectional_bounded_iterator tmp = *this;
            --(*this);
            return tmp;
        }

        // Random access helpers
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
                auto max = std::distance(m_current, m_first);
                if (max < n) [[unlikely]]
                {
                    throw std::out_of_range("rpnx::bounded_iterator: += before begin");
                }
                m_current += n;
                return *this;
            }
        }

        constexpr bidirectional_bounded_iterator& operator-=(difference_type n)
        {
            return (*this) += (-n);
        }

        constexpr bidirectional_bounded_iterator operator+(difference_type n) const
        {
            bidirectional_bounded_iterator tmp = *this;
            tmp += n;
            return tmp;
        }

        friend constexpr bidirectional_bounded_iterator operator+(difference_type n, const bidirectional_bounded_iterator& it)
        {
            return it + n;
        }

        constexpr bidirectional_bounded_iterator operator-(difference_type n) const
        {
            bidirectional_bounded_iterator tmp = *this;
            tmp -= n;
            return tmp;
        }

        constexpr reference operator[](difference_type n) const
        {
            return *(*this + n);
        }


        constexpr difference_type operator-(const bidirectional_bounded_iterator& other) const
        {
            return m_current - other.m_current;
        }

        constexpr bool operator==(const bidirectional_bounded_iterator& other) const
        {
            return m_current == other.m_current;
        }

        constexpr bool operator!=(const bidirectional_bounded_iterator& other) const { return !(*this == other); }

        constexpr bool operator<(const bidirectional_bounded_iterator& other) const
        {
            return m_current < other.m_current;
        }

        constexpr bool operator>(const bidirectional_bounded_iterator& other) const { return other < *this; }
        constexpr bool operator<=(const bidirectional_bounded_iterator& other) const { return !(other < *this); }
        constexpr bool operator>=(const bidirectional_bounded_iterator& other) const { return !(*this < other); }

    private:
        It m_current{};
        It m_first{};
        It m_last{};
    };

    template <class It>
    constexpr bidirectional_bounded_iterator<It> make_bounded_iterator(It it, It first, It last) noexcept
    {
        return bidirectional_bounded_iterator<It>(it, first, last);
    }

    template <class It>
    constexpr bounded_iterator<It> make_bounded_iterator(It it, It last) noexcept
    {
        return bounded_iterator<It>(it, last);
    }
}

#endif // RPNX_ITERATOR_HPP
