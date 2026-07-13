// Copyright (c) 2026 Ryan P. Nicholl <rnicholl@protonmail.com>

#ifndef RPNXDATASTRUCTURES_DYNAR_HPP
#define RPNXDATASTRUCTURES_DYNAR_HPP

#include <compare>
#include <initializer_list>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

namespace rpnx
{
    /**
     * @brief Dynamic array container with size-first ordering.
     *
     * `rpnx::dynar` is currently implemented as a wrapper around `std::vector`
     * and exposes the usual vector member operations. Its relational comparisons
     * differ from `std::vector`: a dynar with fewer elements compares less than
     * a dynar with more elements, regardless of the stored values. Dynars with
     * equal sizes compare through the underlying `std::vector`.
     *
     * @tparam T Stored value type.
     * @tparam Allocator Allocator used by the underlying `std::vector`.
     */
    template < typename T, typename Allocator = std::allocator< T > >
    class dynar
    {
      public:
        using underlying_type = std::vector< T, Allocator >;
        using value_type = typename underlying_type::value_type;
        using allocator_type = typename underlying_type::allocator_type;
        using size_type = typename underlying_type::size_type;
        using difference_type = typename underlying_type::difference_type;
        using reference = typename underlying_type::reference;
        using const_reference = typename underlying_type::const_reference;
        using pointer = typename underlying_type::pointer;
        using const_pointer = typename underlying_type::const_pointer;
        using iterator = typename underlying_type::iterator;
        using const_iterator = typename underlying_type::const_iterator;
        using reverse_iterator = typename underlying_type::reverse_iterator;
        using const_reverse_iterator = typename underlying_type::const_reverse_iterator;

      private:
        underlying_type m_vector;

      public:
        /**
         * @brief Constructs an empty dynar.
         */
        dynar() = default;

        /**
         * @brief Constructs an empty dynar with an allocator.
         * @param alloc Allocator used to allocate elements.
         */
        explicit dynar(Allocator const& alloc) : m_vector(alloc)
        {
        }

        /**
         * @brief Constructs a dynar containing count default-inserted elements.
         * @param count Number of elements to create.
         * @param alloc Allocator used to allocate elements.
         */
        explicit dynar(size_type count, Allocator const& alloc = Allocator()) : m_vector(count, alloc)
        {
        }

        /**
         * @brief Constructs a dynar containing count copies of value.
         * @param count Number of elements to create.
         * @param value Value copied into each element.
         * @param alloc Allocator used to allocate elements.
         */
        dynar(size_type count, T const& value, Allocator const& alloc = Allocator()) : m_vector(count, value, alloc)
        {
        }

        /**
         * @brief Constructs a dynar from an iterator range.
         * @tparam InputIt Input iterator type.
         * @param first First element in the source range.
         * @param last One-past-last element in the source range.
         * @param alloc Allocator used to allocate elements.
         */
        template < typename InputIt >
        dynar(InputIt first, InputIt last, Allocator const& alloc = Allocator()) : m_vector(first, last, alloc)
        {
        }

        /**
         * @brief Copy-constructs a dynar.
         */
        dynar(dynar const&) = default;

        /**
         * @brief Move-constructs a dynar.
         */
        dynar(dynar&&) noexcept(std::is_nothrow_move_constructible_v< underlying_type >) = default;

        /**
         * @brief Copy-constructs a dynar using the specified allocator.
         * @param other Dynar to copy.
         * @param alloc Allocator used to allocate elements.
         */
        dynar(dynar const& other, Allocator const& alloc) : m_vector(other.m_vector, alloc)
        {
        }

        /**
         * @brief Move-constructs a dynar using the specified allocator.
         * @param other Dynar to move from.
         * @param alloc Allocator used to allocate elements.
         */
        dynar(dynar&& other, Allocator const& alloc) : m_vector(std::move(other.m_vector), alloc)
        {
        }

        /**
         * @brief Constructs a dynar from an initializer list.
         * @param init Values copied into the dynar.
         * @param alloc Allocator used to allocate elements.
         */
        dynar(std::initializer_list< T > init, Allocator const& alloc = Allocator()) : m_vector(init, alloc)
        {
        }

        /**
         * @brief Copy-assigns another dynar.
         */
        dynar& operator=(dynar const&) = default;

        /**
         * @brief Move-assigns another dynar.
         */
        dynar& operator=(dynar&&) noexcept(std::is_nothrow_move_assignable_v< underlying_type >) = default;

        /**
         * @brief Replaces the contents with an initializer list.
         * @param init Values copied into the dynar.
         * @return Reference to this dynar.
         */
        dynar& operator=(std::initializer_list< T > init)
        {
            m_vector = init;
            return *this;
        }

        /**
         * @brief Assigns count copies of value.
         * @param count Number of elements to assign.
         * @param value Value copied into each element.
         */
        void assign(size_type count, T const& value)
        {
            m_vector.assign(count, value);
        }

        /**
         * @brief Assigns an iterator range.
         * @tparam InputIt Input iterator type.
         * @param first First element in the source range.
         * @param last One-past-last element in the source range.
         */
        template < typename InputIt >
        void assign(InputIt first, InputIt last)
        {
            m_vector.assign(first, last);
        }

        /**
         * @brief Assigns an initializer list.
         * @param init Values copied into the dynar.
         */
        void assign(std::initializer_list< T > init)
        {
            m_vector.assign(init);
        }

        allocator_type get_allocator() const noexcept
        {
            return m_vector.get_allocator();
        }

        reference at(size_type pos)
        {
            return m_vector.at(pos);
        }

        const_reference at(size_type pos) const
        {
            return m_vector.at(pos);
        }

        reference operator[](size_type pos)
        {
            return m_vector[pos];
        }

        const_reference operator[](size_type pos) const
        {
            return m_vector[pos];
        }

        reference front()
        {
            return m_vector.front();
        }

        const_reference front() const
        {
            return m_vector.front();
        }

        reference back()
        {
            return m_vector.back();
        }

        const_reference back() const
        {
            return m_vector.back();
        }

        T* data() noexcept
        {
            return m_vector.data();
        }

        T const* data() const noexcept
        {
            return m_vector.data();
        }

        iterator begin() noexcept
        {
            return m_vector.begin();
        }

        const_iterator begin() const noexcept
        {
            return m_vector.begin();
        }

        const_iterator cbegin() const noexcept
        {
            return m_vector.cbegin();
        }

        iterator end() noexcept
        {
            return m_vector.end();
        }

        const_iterator end() const noexcept
        {
            return m_vector.end();
        }

        const_iterator cend() const noexcept
        {
            return m_vector.cend();
        }

        reverse_iterator rbegin() noexcept
        {
            return m_vector.rbegin();
        }

        const_reverse_iterator rbegin() const noexcept
        {
            return m_vector.rbegin();
        }

        const_reverse_iterator crbegin() const noexcept
        {
            return m_vector.crbegin();
        }

        reverse_iterator rend() noexcept
        {
            return m_vector.rend();
        }

        const_reverse_iterator rend() const noexcept
        {
            return m_vector.rend();
        }

        const_reverse_iterator crend() const noexcept
        {
            return m_vector.crend();
        }

        bool empty() const noexcept
        {
            return m_vector.empty();
        }

        size_type size() const noexcept
        {
            return m_vector.size();
        }

        size_type max_size() const noexcept
        {
            return m_vector.max_size();
        }

        void reserve(size_type new_cap)
        {
            m_vector.reserve(new_cap);
        }

        size_type capacity() const noexcept
        {
            return m_vector.capacity();
        }

        void shrink_to_fit()
        {
            m_vector.shrink_to_fit();
        }

        void clear() noexcept
        {
            m_vector.clear();
        }

        iterator insert(const_iterator pos, T const& value)
        {
            return m_vector.insert(pos, value);
        }

        iterator insert(const_iterator pos, T&& value)
        {
            return m_vector.insert(pos, std::move(value));
        }

        iterator insert(const_iterator pos, size_type count, T const& value)
        {
            return m_vector.insert(pos, count, value);
        }

        template < typename InputIt >
        iterator insert(const_iterator pos, InputIt first, InputIt last)
        {
            return m_vector.insert(pos, first, last);
        }

        iterator insert(const_iterator pos, std::initializer_list< T > init)
        {
            return m_vector.insert(pos, init);
        }

        template < typename... Args >
        iterator emplace(const_iterator pos, Args&&... args)
        {
            return m_vector.emplace(pos, std::forward< Args >(args)...);
        }

        iterator erase(const_iterator pos)
        {
            return m_vector.erase(pos);
        }

        iterator erase(const_iterator first, const_iterator last)
        {
            return m_vector.erase(first, last);
        }

        void push_back(T const& value)
        {
            m_vector.push_back(value);
        }

        void push_back(T&& value)
        {
            m_vector.push_back(std::move(value));
        }

        template < typename... Args >
        reference emplace_back(Args&&... args)
        {
            return m_vector.emplace_back(std::forward< Args >(args)...);
        }

        void pop_back()
        {
            m_vector.pop_back();
        }

        void resize(size_type count)
        {
            m_vector.resize(count);
        }

        void resize(size_type count, T const& value)
        {
            m_vector.resize(count, value);
        }

        void swap(dynar& other) noexcept(noexcept(m_vector.swap(other.m_vector)))
        {
            m_vector.swap(other.m_vector);
        }

        template < typename U, typename A >
        friend bool operator==(dynar< U, A > const& lhs, dynar< U, A > const& rhs);

        template < typename U, typename A >
        friend bool operator<(dynar< U, A > const& lhs, dynar< U, A > const& rhs);

        template < typename U, typename A >
        friend auto operator<=>(dynar< U, A > const& lhs, dynar< U, A > const& rhs) -> decltype(std::declval< typename dynar< U, A >::underlying_type const& >() <=> std::declval< typename dynar< U, A >::underlying_type const& >());
    };

    /**
     * @brief Swaps two dynars.
     */
    template < typename T, typename Allocator >
    void swap(dynar< T, Allocator >& lhs, dynar< T, Allocator >& rhs) noexcept(noexcept(lhs.swap(rhs)))
    {
        lhs.swap(rhs);
    }

    /**
     * @brief Compares two dynars for equality.
     */
    template < typename T, typename Allocator >
    bool operator==(dynar< T, Allocator > const& lhs, dynar< T, Allocator > const& rhs)
    {
        return lhs.m_vector == rhs.m_vector;
    }

    /**
     * @brief Compares two dynars for inequality.
     */
    template < typename T, typename Allocator >
    bool operator!=(dynar< T, Allocator > const& lhs, dynar< T, Allocator > const& rhs)
    {
        return !(lhs == rhs);
    }

    /**
     * @brief Orders two dynars by size first, then by the underlying vector ordering.
     */
    template < typename T, typename Allocator >
    bool operator<(dynar< T, Allocator > const& lhs, dynar< T, Allocator > const& rhs)
    {
        if (lhs.size() != rhs.size())
        {
            return lhs.size() < rhs.size();
        }

        return lhs.m_vector < rhs.m_vector;
    }

    /**
     * @brief Orders two dynars by size first, then by the underlying vector ordering.
     */
    template < typename T, typename Allocator >
    bool operator>(dynar< T, Allocator > const& lhs, dynar< T, Allocator > const& rhs)
    {
        return rhs < lhs;
    }

    /**
     * @brief Orders two dynars by size first, then by the underlying vector ordering.
     */
    template < typename T, typename Allocator >
    bool operator<=(dynar< T, Allocator > const& lhs, dynar< T, Allocator > const& rhs)
    {
        return !(rhs < lhs);
    }

    /**
     * @brief Orders two dynars by size first, then by the underlying vector ordering.
     */
    template < typename T, typename Allocator >
    bool operator>=(dynar< T, Allocator > const& lhs, dynar< T, Allocator > const& rhs)
    {
        return !(lhs < rhs);
    }

    /**
     * @brief Three-way compares two dynars by size first, then by the underlying vector ordering.
     */
    template < typename T, typename Allocator >
    auto operator<=>(dynar< T, Allocator > const& lhs, dynar< T, Allocator > const& rhs) -> decltype(std::declval< typename dynar< T, Allocator >::underlying_type const& >() <=> std::declval< typename dynar< T, Allocator >::underlying_type const& >())
    {
        using ordering_type = decltype(lhs.m_vector <=> rhs.m_vector);

        if (lhs.size() < rhs.size())
        {
            return ordering_type::less;
        }

        if (rhs.size() < lhs.size())
        {
            return ordering_type::greater;
        }

        return lhs.m_vector <=> rhs.m_vector;
    }
} // namespace rpnx

#endif // RPNXDATASTRUCTURES_DYNAR_HPP
