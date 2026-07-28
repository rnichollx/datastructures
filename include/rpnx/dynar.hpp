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
        /// Underlying contiguous container type.
        using underlying_type = std::vector< T, Allocator >;
        /// Stored element type.
        using value_type = typename underlying_type::value_type;
        /// Allocator type used to manage storage.
        using allocator_type = typename underlying_type::allocator_type;
        /// Unsigned type used for sizes and indices.
        using size_type = typename underlying_type::size_type;
        /// Signed type used for iterator distances.
        using difference_type = typename underlying_type::difference_type;
        /// Mutable element reference type.
        using reference = typename underlying_type::reference;
        /// Immutable element reference type.
        using const_reference = typename underlying_type::const_reference;
        /// Mutable element pointer type.
        using pointer = typename underlying_type::pointer;
        /// Immutable element pointer type.
        using const_pointer = typename underlying_type::const_pointer;
        /// Mutable random-access iterator type.
        using iterator = typename underlying_type::iterator;
        /// Immutable random-access iterator type.
        using const_iterator = typename underlying_type::const_iterator;
        /// Mutable reverse-iterator type.
        using reverse_iterator = typename underlying_type::reverse_iterator;
        /// Immutable reverse-iterator type.
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
         * @return Reference to this dynar.
         */
        dynar& operator=(dynar const&) = default;

        /**
         * @brief Move-assigns another dynar.
         * @return Reference to this dynar.
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

        /**
         * @brief Returns a copy of the allocator associated with the container.
         * @return The container allocator.
         */
        allocator_type get_allocator() const noexcept
        {
            return m_vector.get_allocator();
        }

        /**
         * @brief Returns the element at an index with bounds checking.
         * @param pos Zero-based element index.
         * @return A mutable reference to the selected element.
         * @throws std::out_of_range if `pos >= size()`.
         */
        reference at(size_type pos)
        {
            return m_vector.at(pos);
        }

        /** @copydoc at(size_type) */
        const_reference at(size_type pos) const
        {
            return m_vector.at(pos);
        }

        /**
         * @brief Returns the element at an index without bounds checking.
         * @param pos Zero-based element index.
         * @return A mutable reference to the selected element.
         * @pre `pos < size()`.
         */
        reference operator[](size_type pos)
        {
            return m_vector[pos];
        }

        /** @copydoc operator[](size_type) */
        const_reference operator[](size_type pos) const
        {
            return m_vector[pos];
        }

        /**
         * @brief Returns the first element.
         * @return A mutable reference to the first element.
         * @pre The container is not empty.
         */
        reference front()
        {
            return m_vector.front();
        }

        /** @copydoc front() */
        const_reference front() const
        {
            return m_vector.front();
        }

        /**
         * @brief Returns the last element.
         * @return A mutable reference to the last element.
         * @pre The container is not empty.
         */
        reference back()
        {
            return m_vector.back();
        }

        /** @copydoc back() */
        const_reference back() const
        {
            return m_vector.back();
        }

        /**
         * @brief Returns a pointer to the contiguous element storage.
         * @return A pointer to the first element, or an implementation-defined
         * non-dereferenceable pointer when the container is empty.
         */
        T* data() noexcept
        {
            return m_vector.data();
        }

        /** @copydoc data() */
        T const* data() const noexcept
        {
            return m_vector.data();
        }

        /** @brief Returns an iterator to the first element. @return Beginning iterator. */
        iterator begin() noexcept
        {
            return m_vector.begin();
        }

        /** @brief Returns an immutable iterator to the first element. @return Beginning iterator. */
        const_iterator begin() const noexcept
        {
            return m_vector.begin();
        }

        /** @brief Returns an immutable iterator to the first element. @return Beginning iterator. */
        const_iterator cbegin() const noexcept
        {
            return m_vector.cbegin();
        }

        /** @brief Returns an iterator one past the last element. @return Ending iterator. */
        iterator end() noexcept
        {
            return m_vector.end();
        }

        /** @brief Returns an immutable iterator one past the last element. @return Ending iterator. */
        const_iterator end() const noexcept
        {
            return m_vector.end();
        }

        /** @brief Returns an immutable iterator one past the last element. @return Ending iterator. */
        const_iterator cend() const noexcept
        {
            return m_vector.cend();
        }

        /** @brief Returns a reverse iterator to the last element. @return Beginning reverse iterator. */
        reverse_iterator rbegin() noexcept
        {
            return m_vector.rbegin();
        }

        /** @brief Returns an immutable reverse iterator to the last element. @return Beginning reverse iterator. */
        const_reverse_iterator rbegin() const noexcept
        {
            return m_vector.rbegin();
        }

        /** @brief Returns an immutable reverse iterator to the last element. @return Beginning reverse iterator. */
        const_reverse_iterator crbegin() const noexcept
        {
            return m_vector.crbegin();
        }

        /** @brief Returns a reverse iterator preceding the first element. @return Ending reverse iterator. */
        reverse_iterator rend() noexcept
        {
            return m_vector.rend();
        }

        /** @brief Returns an immutable reverse iterator preceding the first element. @return Ending reverse iterator. */
        const_reverse_iterator rend() const noexcept
        {
            return m_vector.rend();
        }

        /** @brief Returns an immutable reverse iterator preceding the first element. @return Ending reverse iterator. */
        const_reverse_iterator crend() const noexcept
        {
            return m_vector.crend();
        }

        /** @brief Returns whether the container has no elements. @return `true` when empty. */
        bool empty() const noexcept
        {
            return m_vector.empty();
        }

        /** @brief Returns the number of stored elements. @return Element count. */
        size_type size() const noexcept
        {
            return m_vector.size();
        }

        /** @brief Returns the maximum number of elements supported by the implementation. @return Maximum element count. */
        size_type max_size() const noexcept
        {
            return m_vector.max_size();
        }

        /**
         * @brief Ensures storage is available for at least a requested number of elements.
         * @param new_cap Requested minimum capacity.
         * @throws std::length_error if `new_cap` exceeds `max_size()`.
         * @note A reallocation invalidates all references, pointers, and iterators.
         */
        void reserve(size_type new_cap)
        {
            m_vector.reserve(new_cap);
        }

        /** @brief Returns the number of elements that fit without reallocating. @return Current capacity. */
        size_type capacity() const noexcept
        {
            return m_vector.capacity();
        }

        /**
         * @brief Requests that unused capacity be released.
         * @note The request is non-binding. A performed reallocation invalidates
         * all references, pointers, and iterators.
         */
        void shrink_to_fit()
        {
            m_vector.shrink_to_fit();
        }

        /** @brief Removes all elements without reducing capacity. */
        void clear() noexcept
        {
            m_vector.clear();
        }

        /**
         * @brief Inserts a copy of an element before a position.
         * @param pos Insertion position.
         * @param value Element to copy.
         * @return An iterator to the inserted element.
         */
        iterator insert(const_iterator pos, T const& value)
        {
            return m_vector.insert(pos, value);
        }

        /**
         * @brief Inserts an element by moving it before a position.
         * @param pos Insertion position.
         * @param value Element to move.
         * @return An iterator to the inserted element.
         */
        iterator insert(const_iterator pos, T&& value)
        {
            return m_vector.insert(pos, std::move(value));
        }

        /**
         * @brief Inserts repeated copies of an element before a position.
         * @param pos Insertion position.
         * @param count Number of copies to insert.
         * @param value Element to copy.
         * @return An iterator to the first inserted element, or `pos` when `count` is zero.
         */
        iterator insert(const_iterator pos, size_type count, T const& value)
        {
            return m_vector.insert(pos, count, value);
        }

        /**
         * @brief Inserts an iterator range before a position.
         * @tparam InputIt Input iterator type.
         * @param pos Insertion position.
         * @param first First source element.
         * @param last One-past-last source element.
         * @return An iterator to the first inserted element, or `pos` for an empty range.
         */
        template < typename InputIt >
        iterator insert(const_iterator pos, InputIt first, InputIt last)
        {
            return m_vector.insert(pos, first, last);
        }

        /**
         * @brief Inserts an initializer list before a position.
         * @param pos Insertion position.
         * @param init Elements to copy.
         * @return An iterator to the first inserted element, or `pos` when `init` is empty.
         */
        iterator insert(const_iterator pos, std::initializer_list< T > init)
        {
            return m_vector.insert(pos, init);
        }

        /**
         * @brief Constructs an element in place before a position.
         * @tparam Args Constructor argument types.
         * @param pos Insertion position.
         * @param args Arguments forwarded to `T`'s constructor.
         * @return An iterator to the constructed element.
         */
        template < typename... Args >
        iterator emplace(const_iterator pos, Args&&... args)
        {
            return m_vector.emplace(pos, std::forward< Args >(args)...);
        }

        /**
         * @brief Erases the element at a position.
         * @param pos Iterator to the element to erase.
         * @return An iterator following the erased element.
         */
        iterator erase(const_iterator pos)
        {
            return m_vector.erase(pos);
        }

        /**
         * @brief Erases an iterator range.
         * @param first First element to erase.
         * @param last One-past-last element to erase.
         * @return An iterator following the erased range.
         */
        iterator erase(const_iterator first, const_iterator last)
        {
            return m_vector.erase(first, last);
        }

        /** @brief Appends a copy of an element. @param value Element to copy. */
        void push_back(T const& value)
        {
            m_vector.push_back(value);
        }

        /** @brief Appends an element by moving it. @param value Element to move. */
        void push_back(T&& value)
        {
            m_vector.push_back(std::move(value));
        }

        /**
         * @brief Constructs an element at the end of the container.
         * @tparam Args Constructor argument types.
         * @param args Arguments forwarded to `T`'s constructor.
         * @return A reference to the constructed element.
         */
        template < typename... Args >
        reference emplace_back(Args&&... args)
        {
            return m_vector.emplace_back(std::forward< Args >(args)...);
        }

        /** @brief Removes the last element. @pre The container is not empty. */
        void pop_back()
        {
            m_vector.pop_back();
        }

        /**
         * @brief Changes the number of elements, value-initializing new elements.
         * @param count Requested number of elements.
         */
        void resize(size_type count)
        {
            m_vector.resize(count);
        }

        /**
         * @brief Changes the number of elements, copying a value for new elements.
         * @param count Requested number of elements.
         * @param value Value copied into each new element.
         */
        void resize(size_type count, T const& value)
        {
            m_vector.resize(count, value);
        }

        /** @brief Exchanges contents with another dynar. @param other Container to exchange with. */
        void swap(dynar& other) noexcept(noexcept(m_vector.swap(other.m_vector)))
        {
            m_vector.swap(other.m_vector);
        }

        /** @brief Grants the equality comparison access to the underlying container. */
        template < typename U, typename A >
        friend bool operator==(dynar< U, A > const& lhs, dynar< U, A > const& rhs);

        /** @brief Grants the less-than comparison access to the underlying container. */
        template < typename U, typename A >
        friend bool operator<(dynar< U, A > const& lhs, dynar< U, A > const& rhs);

        /** @brief Grants the three-way comparison access to the underlying container. */
        template < typename U, typename A >
        friend auto operator<=>(dynar< U, A > const& lhs, dynar< U, A > const& rhs) -> decltype(std::declval< typename dynar< U, A >::underlying_type const& >() <=> std::declval< typename dynar< U, A >::underlying_type const& >());
    };

    /**
     * @brief Swaps two dynars.
     * @param lhs First container.
     * @param rhs Second container.
     */
    template < typename T, typename Allocator >
    void swap(dynar< T, Allocator >& lhs, dynar< T, Allocator >& rhs) noexcept(noexcept(lhs.swap(rhs)))
    {
        lhs.swap(rhs);
    }

    /**
     * @brief Compares two dynars for equality.
     * @param lhs Left operand.
     * @param rhs Right operand.
     * @return `true` when the contents are equal.
     */
    template < typename T, typename Allocator >
    bool operator==(dynar< T, Allocator > const& lhs, dynar< T, Allocator > const& rhs)
    {
        return lhs.m_vector == rhs.m_vector;
    }

    /**
     * @brief Compares two dynars for inequality.
     * @param lhs Left operand.
     * @param rhs Right operand.
     * @return `true` when the contents differ.
     */
    template < typename T, typename Allocator >
    bool operator!=(dynar< T, Allocator > const& lhs, dynar< T, Allocator > const& rhs)
    {
        return !(lhs == rhs);
    }

    /**
     * @brief Orders two dynars by size first, then by the underlying vector ordering.
     * @param lhs Left operand.
     * @param rhs Right operand.
     * @return `true` when `lhs` orders before `rhs`.
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
     * @param lhs Left operand.
     * @param rhs Right operand.
     * @return `true` when `lhs` orders after `rhs`.
     */
    template < typename T, typename Allocator >
    bool operator>(dynar< T, Allocator > const& lhs, dynar< T, Allocator > const& rhs)
    {
        return rhs < lhs;
    }

    /**
     * @brief Orders two dynars by size first, then by the underlying vector ordering.
     * @param lhs Left operand.
     * @param rhs Right operand.
     * @return `true` when `lhs` does not order after `rhs`.
     */
    template < typename T, typename Allocator >
    bool operator<=(dynar< T, Allocator > const& lhs, dynar< T, Allocator > const& rhs)
    {
        return !(rhs < lhs);
    }

    /**
     * @brief Orders two dynars by size first, then by the underlying vector ordering.
     * @param lhs Left operand.
     * @param rhs Right operand.
     * @return `true` when `lhs` does not order before `rhs`.
     */
    template < typename T, typename Allocator >
    bool operator>=(dynar< T, Allocator > const& lhs, dynar< T, Allocator > const& rhs)
    {
        return !(lhs < rhs);
    }

    /**
     * @brief Three-way compares two dynars by size first, then by the underlying vector ordering.
     * @param lhs Left operand.
     * @param rhs Right operand.
     * @return Ordering category result.
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
