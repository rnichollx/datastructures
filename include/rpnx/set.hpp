// Copyright (c) 2026 Ryan P. Nicholl <rnicholl@protonmail.com>

#ifndef RPNXDATASTRUCTURES_SET_HPP
#define RPNXDATASTRUCTURES_SET_HPP

#include <compare>
#include <initializer_list>
#include <memory>
#include <set>
#include <type_traits>
#include <utility>

namespace rpnx
{
    /**
     * @brief Ordered unique-key container with size-first ordering.
     *
     * `rpnx::set` is currently implemented as a wrapper around `std::set` and
     * exposes the same common container operations. Its relational comparisons
     * differ from `std::set`: a set with fewer elements compares less than a set
     * with more elements, regardless of the stored values. Sets with equal sizes
     * compare lexicographically by iterator order.
     *
     * @tparam Key Stored key type.
     * @tparam Compare Ordering predicate used by the underlying `std::set`.
     * @tparam Allocator Allocator used by the underlying `std::set`.
     */
    template < typename Key, typename Compare = std::less< Key >, typename Allocator = std::allocator< Key > >
    class set
    {
      public:
        /// Underlying ordered unique-key container type.
        using underlying_type = std::set< Key, Compare, Allocator >;
        /// Key type used to order and identify elements.
        using key_type = typename underlying_type::key_type;
        /// Stored value type, identical to `key_type`.
        using value_type = typename underlying_type::value_type;
        /// Unsigned type used for element counts.
        using size_type = typename underlying_type::size_type;
        /// Signed type used for iterator distances.
        using difference_type = typename underlying_type::difference_type;
        /// Function object used to order keys.
        using key_compare = typename underlying_type::key_compare;
        /// Function object used to order stored values.
        using value_compare = typename underlying_type::value_compare;
        /// Allocator type used to manage nodes.
        using allocator_type = typename underlying_type::allocator_type;
        /// Stored-value reference type.
        using reference = typename underlying_type::reference;
        /// Immutable stored-value reference type.
        using const_reference = typename underlying_type::const_reference;
        /// Stored-value pointer type.
        using pointer = typename underlying_type::pointer;
        /// Immutable stored-value pointer type.
        using const_pointer = typename underlying_type::const_pointer;
        /// Bidirectional iterator type.
        using iterator = typename underlying_type::iterator;
        /// Immutable bidirectional iterator type.
        using const_iterator = typename underlying_type::const_iterator;
        /// Reverse-iterator type.
        using reverse_iterator = typename underlying_type::reverse_iterator;
        /// Immutable reverse-iterator type.
        using const_reverse_iterator = typename underlying_type::const_reverse_iterator;
        /// Owning handle for an extracted node.
        using node_type = typename underlying_type::node_type;
        /// Result returned when inserting a node handle without a hint.
        using insert_return_type = typename underlying_type::insert_return_type;

      private:
        underlying_type m_set;

      public:
        /**
         * @brief Constructs an empty set.
         */
        set() = default;

        /**
         * @brief Constructs an empty set with a comparator and allocator.
         * @param comp Comparator used to order elements.
         * @param alloc Allocator used to allocate elements.
         */
        explicit set(Compare const& comp, Allocator const& alloc = Allocator()) : m_set(comp, alloc)
        {
        }

        /**
         * @brief Constructs an empty set with an allocator.
         * @param alloc Allocator used to allocate elements.
         */
        explicit set(Allocator const& alloc) : m_set(alloc)
        {
        }

        /**
         * @brief Constructs a set from an iterator range.
         * @tparam InputIt Input iterator type.
         * @param first First element in the source range.
         * @param last One-past-last element in the source range.
         * @param comp Comparator used to order elements.
         * @param alloc Allocator used to allocate elements.
         */
        template < typename InputIt >
        set(InputIt first, InputIt last, Compare const& comp = Compare(), Allocator const& alloc = Allocator()) : m_set(first, last, comp, alloc)
        {
        }

        /**
         * @brief Constructs a set from an iterator range and allocator.
         * @tparam InputIt Input iterator type.
         * @param first First element in the source range.
         * @param last One-past-last element in the source range.
         * @param alloc Allocator used to allocate elements.
         */
        template < typename InputIt >
        set(InputIt first, InputIt last, Allocator const& alloc) : m_set(first, last, alloc)
        {
        }

        /**
         * @brief Constructs a set from an initializer list.
         * @param init Values to insert.
         * @param comp Comparator used to order elements.
         * @param alloc Allocator used to allocate elements.
         */
        set(std::initializer_list< value_type > init, Compare const& comp = Compare(), Allocator const& alloc = Allocator()) : m_set(init, comp, alloc)
        {
        }

        /**
         * @brief Constructs a set from an initializer list and allocator.
         * @param init Values to insert.
         * @param alloc Allocator used to allocate elements.
         */
        set(std::initializer_list< value_type > init, Allocator const& alloc) : m_set(init, alloc)
        {
        }

        /**
         * @brief Copy-constructs a set.
         */
        set(set const&) = default;

        /**
         * @brief Move-constructs a set.
         */
        set(set&&) noexcept(std::is_nothrow_move_constructible_v< underlying_type >) = default;

        /**
         * @brief Copy-constructs a set using the specified allocator.
         * @param other Set to copy.
         * @param alloc Allocator used to allocate elements.
         */
        set(set const& other, Allocator const& alloc) : m_set(other.m_set, alloc)
        {
        }

        /**
         * @brief Move-constructs a set using the specified allocator.
         * @param other Set to move from.
         * @param alloc Allocator used to allocate elements.
         */
        set(set&& other, Allocator const& alloc) : m_set(std::move(other.m_set), alloc)
        {
        }

        /**
         * @brief Copy-assigns another set.
         * @return Reference to this set.
         */
        set& operator=(set const&) = default;

        /**
         * @brief Move-assigns another set.
         * @return Reference to this set.
         */
        set& operator=(set&&) noexcept(std::is_nothrow_move_assignable_v< underlying_type >) = default;

        /**
         * @brief Replaces the contents with an initializer list.
         * @param init Values to insert.
         * @return Reference to this set.
         */
        set& operator=(std::initializer_list< value_type > init)
        {
            m_set = init;
            return *this;
        }

        /**
         * @brief Returns the allocator used by this set.
         * @return Allocator associated with the set.
         */
        allocator_type get_allocator() const noexcept
        {
            return m_set.get_allocator();
        }

        /** @brief Returns an iterator to the first element. @return Beginning iterator. */
        iterator begin() noexcept
        {
            return m_set.begin();
        }

        /** @brief Returns an immutable iterator to the first element. @return Beginning iterator. */
        const_iterator begin() const noexcept
        {
            return m_set.begin();
        }

        /** @brief Returns an immutable iterator to the first element. @return Beginning iterator. */
        const_iterator cbegin() const noexcept
        {
            return m_set.cbegin();
        }

        /** @brief Returns an iterator one past the final element. @return Ending iterator. */
        iterator end() noexcept
        {
            return m_set.end();
        }

        /** @brief Returns an immutable iterator one past the final element. @return Ending iterator. */
        const_iterator end() const noexcept
        {
            return m_set.end();
        }

        /** @brief Returns an immutable iterator one past the final element. @return Ending iterator. */
        const_iterator cend() const noexcept
        {
            return m_set.cend();
        }

        /** @brief Returns a reverse iterator to the final element. @return Beginning reverse iterator. */
        reverse_iterator rbegin() noexcept
        {
            return m_set.rbegin();
        }

        /** @brief Returns an immutable reverse iterator to the final element. @return Beginning reverse iterator. */
        const_reverse_iterator rbegin() const noexcept
        {
            return m_set.rbegin();
        }

        /** @brief Returns an immutable reverse iterator to the final element. @return Beginning reverse iterator. */
        const_reverse_iterator crbegin() const noexcept
        {
            return m_set.crbegin();
        }

        /** @brief Returns a reverse iterator preceding the first element. @return Ending reverse iterator. */
        reverse_iterator rend() noexcept
        {
            return m_set.rend();
        }

        /** @brief Returns an immutable reverse iterator preceding the first element. @return Ending reverse iterator. */
        const_reverse_iterator rend() const noexcept
        {
            return m_set.rend();
        }

        /** @brief Returns an immutable reverse iterator preceding the first element. @return Ending reverse iterator. */
        const_reverse_iterator crend() const noexcept
        {
            return m_set.crend();
        }

        /** @brief Returns whether the set has no elements. @return `true` when empty. */
        bool empty() const noexcept
        {
            return m_set.empty();
        }

        /** @brief Returns the number of stored elements. @return Element count. */
        size_type size() const noexcept
        {
            return m_set.size();
        }

        /** @brief Returns the maximum number of elements supported by the implementation. @return Maximum element count. */
        size_type max_size() const noexcept
        {
            return m_set.max_size();
        }

        /** @brief Removes all elements. */
        void clear() noexcept
        {
            m_set.clear();
        }

        /** @brief Inserts a copied value if absent. @param value Value to copy. @return Iterator to the matching element and whether insertion occurred. */
        std::pair< iterator, bool > insert(value_type const& value)
        {
            return m_set.insert(value);
        }

        /** @brief Inserts a moved value if absent. @param value Value to move. @return Iterator to the matching element and whether insertion occurred. */
        std::pair< iterator, bool > insert(value_type&& value)
        {
            return m_set.insert(std::move(value));
        }

        /** @brief Inserts a copied value using an ordering hint. @param hint Position immediately before which insertion may be efficient. @param value Value to copy. @return Iterator to the matching element. */
        iterator insert(const_iterator hint, value_type const& value)
        {
            return m_set.insert(hint, value);
        }

        /** @brief Inserts a moved value using an ordering hint. @param hint Position immediately before which insertion may be efficient. @param value Value to move. @return Iterator to the matching element. */
        iterator insert(const_iterator hint, value_type&& value)
        {
            return m_set.insert(hint, std::move(value));
        }

        /** @brief Inserts an iterator range. @tparam InputIt Input iterator type. @param first First source element. @param last One-past-last source element. */
        template < typename InputIt >
        void insert(InputIt first, InputIt last)
        {
            m_set.insert(first, last);
        }

        /** @brief Inserts values from an initializer list. @param init Values to insert. */
        void insert(std::initializer_list< value_type > init)
        {
            m_set.insert(init);
        }

        /** @brief Inserts an extracted node if absent. @param node Owning node handle to insert. @return Insertion result containing the position, insertion state, and any uninserted node. @pre `node.empty()` or `get_allocator() == node.get_allocator()`. */
        insert_return_type insert(node_type&& node)
        {
            return m_set.insert(std::move(node));
        }

        /** @brief Inserts an extracted node using an ordering hint. @param hint Position immediately before which insertion may be efficient. @param node Owning node handle to insert. @return Iterator to the matching element. @pre `node.empty()` or `get_allocator() == node.get_allocator()`. */
        iterator insert(const_iterator hint, node_type&& node)
        {
            return m_set.insert(hint, std::move(node));
        }

        /** @brief Constructs a value in place if absent. @tparam Args Value constructor argument types. @param args Arguments forwarded to the value constructor. @return Iterator to the matching element and whether insertion occurred. */
        template < typename... Args >
        std::pair< iterator, bool > emplace(Args&&... args)
        {
            return m_set.emplace(std::forward< Args >(args)...);
        }

        /** @brief Constructs a value in place using an ordering hint. @tparam Args Value constructor argument types. @param hint Position immediately before which insertion may be efficient. @param args Arguments forwarded to the value constructor. @return Iterator to the matching element. */
        template < typename... Args >
        iterator emplace_hint(const_iterator hint, Args&&... args)
        {
            return m_set.emplace_hint(hint, std::forward< Args >(args)...);
        }

        /** @brief Erases one element. @param pos Element to erase. @return Iterator following the erased element. */
        iterator erase(const_iterator pos)
        {
            return m_set.erase(pos);
        }

        /** @brief Erases a range. @param first First element to erase. @param last One-past-last element to erase. @return Iterator following the erased range. */
        iterator erase(const_iterator first, const_iterator last)
        {
            return m_set.erase(first, last);
        }

        /** @brief Erases an element by key. @param key Key to erase. @return One if erased, otherwise zero. */
        size_type erase(key_type const& key)
        {
            return m_set.erase(key);
        }

        /** @brief Exchanges contents with another set. @param other Set to exchange with. @pre Allocator propagation on swap is enabled, or `get_allocator() == other.get_allocator()`. */
        void swap(set& other) noexcept(noexcept(m_set.swap(other.m_set)))
        {
            m_set.swap(other.m_set);
        }

        /** @brief Removes an element without destroying it. @param pos Element to extract. @return Owning handle for the extracted node. */
        node_type extract(const_iterator pos)
        {
            return m_set.extract(pos);
        }

        /** @brief Removes an element by key without destroying it. @param key Key to extract. @return Owning handle, empty if absent. */
        node_type extract(key_type const& key)
        {
            return m_set.extract(key);
        }

        /** @brief Transfers non-duplicate nodes from another RPNX set. @tparam C2 Source comparator type. @param source Set from which nodes are transferred. @pre `get_allocator() == source.get_allocator()`. */
        template < typename C2 >
        void merge(set< Key, C2, Allocator >& source)
        {
            m_set.merge(source.m_set);
        }

        /** @brief Transfers non-duplicate nodes from another RPNX set. @tparam C2 Source comparator type. @param source Set from which nodes are transferred. @pre `get_allocator() == source.get_allocator()`. */
        template < typename C2 >
        void merge(set< Key, C2, Allocator >&& source)
        {
            m_set.merge(source.m_set);
        }

        /** @brief Transfers non-duplicate nodes from a standard set. @tparam C2 Source comparator type. @param source Set from which nodes are transferred. @pre `get_allocator() == source.get_allocator()`. */
        template < typename C2 >
        void merge(std::set< Key, C2, Allocator >& source)
        {
            m_set.merge(source);
        }

        /** @brief Transfers non-duplicate nodes from a standard set. @tparam C2 Source comparator type. @param source Set from which nodes are transferred. @pre `get_allocator() == source.get_allocator()`. */
        template < typename C2 >
        void merge(std::set< Key, C2, Allocator >&& source)
        {
            m_set.merge(source);
        }

        /** @brief Counts elements matching a key. @param key Key to find. @return One when present, otherwise zero. */
        size_type count(key_type const& key) const
        {
            return m_set.count(key);
        }

        /** @brief Finds an element by key. @param key Key to find. @return Iterator to the element, or `end()`. */
        iterator find(key_type const& key)
        {
            return m_set.find(key);
        }

        /** @copydoc find(key_type const&) */
        const_iterator find(key_type const& key) const
        {
            return m_set.find(key);
        }

        /** @brief Tests whether a key is present. @param key Key to find. @return `true` if present. */
        bool contains(key_type const& key) const
        {
            return m_set.contains(key);
        }

        /** @brief Finds the range matching a key. @param key Key to find. @return Pair of lower and upper bounds. */
        std::pair< iterator, iterator > equal_range(key_type const& key)
        {
            return m_set.equal_range(key);
        }

        /** @copydoc equal_range(key_type const&) */
        std::pair< const_iterator, const_iterator > equal_range(key_type const& key) const
        {
            return m_set.equal_range(key);
        }

        /** @brief Finds the first element not ordered before a key. @param key Boundary key. @return Boundary iterator. */
        iterator lower_bound(key_type const& key)
        {
            return m_set.lower_bound(key);
        }

        /** @copydoc lower_bound(key_type const&) */
        const_iterator lower_bound(key_type const& key) const
        {
            return m_set.lower_bound(key);
        }

        /** @brief Finds the first element ordered after a key. @param key Boundary key. @return Boundary iterator. */
        iterator upper_bound(key_type const& key)
        {
            return m_set.upper_bound(key);
        }

        /** @copydoc upper_bound(key_type const&) */
        const_iterator upper_bound(key_type const& key) const
        {
            return m_set.upper_bound(key);
        }

        /** @brief Returns the key-ordering predicate. @return Copy of the comparator. */
        key_compare key_comp() const
        {
            return m_set.key_comp();
        }

        /** @brief Returns the stored-value ordering predicate. @return Copy of the value comparator. */
        value_compare value_comp() const
        {
            return m_set.value_comp();
        }

        template < typename K, typename C, typename A >
        friend class set;

        /** @brief Grants equality comparison access to the underlying set. */
        template < typename K, typename C, typename A >
        friend bool operator==(set< K, C, A > const& lhs, set< K, C, A > const& rhs);

        /** @brief Grants less-than comparison access to the underlying set. */
        template < typename K, typename C, typename A >
        friend bool operator<(set< K, C, A > const& lhs, set< K, C, A > const& rhs);

        /** @brief Grants three-way comparison access to the underlying set. */
        template < typename K, typename C, typename A >
        friend auto operator<=>(set< K, C, A > const& lhs, set< K, C, A > const& rhs) -> decltype(std::declval< typename set< K, C, A >::underlying_type const& >() <=> std::declval< typename set< K, C, A >::underlying_type const& >());
    };

    /**
     * @brief Swaps two sets.
     * @param lhs First set.
     * @param rhs Second set.
     */
    template < typename Key, typename Compare, typename Allocator >
    void swap(set< Key, Compare, Allocator >& lhs, set< Key, Compare, Allocator >& rhs) noexcept(noexcept(lhs.swap(rhs)))
    {
        lhs.swap(rhs);
    }

    /**
     * @brief Compares two sets for equality.
     * @param lhs Left operand.
     * @param rhs Right operand.
     * @return `true` when the contents are equal.
     */
    template < typename Key, typename Compare, typename Allocator >
    bool operator==(set< Key, Compare, Allocator > const& lhs, set< Key, Compare, Allocator > const& rhs)
    {
        return lhs.m_set == rhs.m_set;
    }

    /**
     * @brief Compares two sets for inequality.
     * @param lhs Left operand.
     * @param rhs Right operand.
     * @return `true` when the contents differ.
     */
    template < typename Key, typename Compare, typename Allocator >
    bool operator!=(set< Key, Compare, Allocator > const& lhs, set< Key, Compare, Allocator > const& rhs)
    {
        return !(lhs == rhs);
    }

    /**
     * @brief Orders two sets by size first, then lexicographically.
     * @param lhs Left operand.
     * @param rhs Right operand.
     * @return `true` when `lhs` orders before `rhs`.
     */
    template < typename Key, typename Compare, typename Allocator >
    bool operator<(set< Key, Compare, Allocator > const& lhs, set< Key, Compare, Allocator > const& rhs)
    {
        if (lhs.size() != rhs.size())
        {
            return lhs.size() < rhs.size();
        }

        return lhs.m_set < rhs.m_set;
    }

    /**
     * @brief Orders two sets by size first, then lexicographically.
     * @param lhs Left operand.
     * @param rhs Right operand.
     * @return `true` when `lhs` orders after `rhs`.
     */
    template < typename Key, typename Compare, typename Allocator >
    bool operator>(set< Key, Compare, Allocator > const& lhs, set< Key, Compare, Allocator > const& rhs)
    {
        return rhs < lhs;
    }

    /**
     * @brief Orders two sets by size first, then lexicographically.
     * @param lhs Left operand.
     * @param rhs Right operand.
     * @return `true` when `lhs` does not order after `rhs`.
     */
    template < typename Key, typename Compare, typename Allocator >
    bool operator<=(set< Key, Compare, Allocator > const& lhs, set< Key, Compare, Allocator > const& rhs)
    {
        return !(rhs < lhs);
    }

    /**
     * @brief Orders two sets by size first, then lexicographically.
     * @param lhs Left operand.
     * @param rhs Right operand.
     * @return `true` when `lhs` does not order before `rhs`.
     */
    template < typename Key, typename Compare, typename Allocator >
    bool operator>=(set< Key, Compare, Allocator > const& lhs, set< Key, Compare, Allocator > const& rhs)
    {
        return !(lhs < rhs);
    }

    /**
     * @brief Three-way compares two sets by size first, then lexicographically.
     * @param lhs Left operand.
     * @param rhs Right operand.
     * @return Ordering category result.
     */
    template < typename Key, typename Compare, typename Allocator >
    auto operator<=>(set< Key, Compare, Allocator > const& lhs, set< Key, Compare, Allocator > const& rhs) -> decltype(std::declval< typename set< Key, Compare, Allocator >::underlying_type const& >() <=> std::declval< typename set< Key, Compare, Allocator >::underlying_type const& >())
    {
        using ordering_type = decltype(lhs.m_set <=> rhs.m_set);

        if (lhs.size() < rhs.size())
        {
            return ordering_type::less;
        }

        if (rhs.size() < lhs.size())
        {
            return ordering_type::greater;
        }

        return lhs.m_set <=> rhs.m_set;
    }
} // namespace rpnx

#endif // RPNXDATASTRUCTURES_SET_HPP
