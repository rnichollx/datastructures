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
        using underlying_type = std::set< Key, Compare, Allocator >;
        using key_type = typename underlying_type::key_type;
        using value_type = typename underlying_type::value_type;
        using size_type = typename underlying_type::size_type;
        using difference_type = typename underlying_type::difference_type;
        using key_compare = typename underlying_type::key_compare;
        using value_compare = typename underlying_type::value_compare;
        using allocator_type = typename underlying_type::allocator_type;
        using reference = typename underlying_type::reference;
        using const_reference = typename underlying_type::const_reference;
        using pointer = typename underlying_type::pointer;
        using const_pointer = typename underlying_type::const_pointer;
        using iterator = typename underlying_type::iterator;
        using const_iterator = typename underlying_type::const_iterator;
        using reverse_iterator = typename underlying_type::reverse_iterator;
        using const_reverse_iterator = typename underlying_type::const_reverse_iterator;
        using node_type = typename underlying_type::node_type;
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
         */
        set& operator=(set const&) = default;

        /**
         * @brief Move-assigns another set.
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
         */
        allocator_type get_allocator() const noexcept
        {
            return m_set.get_allocator();
        }

        iterator begin() noexcept
        {
            return m_set.begin();
        }

        const_iterator begin() const noexcept
        {
            return m_set.begin();
        }

        const_iterator cbegin() const noexcept
        {
            return m_set.cbegin();
        }

        iterator end() noexcept
        {
            return m_set.end();
        }

        const_iterator end() const noexcept
        {
            return m_set.end();
        }

        const_iterator cend() const noexcept
        {
            return m_set.cend();
        }

        reverse_iterator rbegin() noexcept
        {
            return m_set.rbegin();
        }

        const_reverse_iterator rbegin() const noexcept
        {
            return m_set.rbegin();
        }

        const_reverse_iterator crbegin() const noexcept
        {
            return m_set.crbegin();
        }

        reverse_iterator rend() noexcept
        {
            return m_set.rend();
        }

        const_reverse_iterator rend() const noexcept
        {
            return m_set.rend();
        }

        const_reverse_iterator crend() const noexcept
        {
            return m_set.crend();
        }

        bool empty() const noexcept
        {
            return m_set.empty();
        }

        size_type size() const noexcept
        {
            return m_set.size();
        }

        size_type max_size() const noexcept
        {
            return m_set.max_size();
        }

        void clear() noexcept
        {
            m_set.clear();
        }

        std::pair< iterator, bool > insert(value_type const& value)
        {
            return m_set.insert(value);
        }

        std::pair< iterator, bool > insert(value_type&& value)
        {
            return m_set.insert(std::move(value));
        }

        iterator insert(const_iterator hint, value_type const& value)
        {
            return m_set.insert(hint, value);
        }

        iterator insert(const_iterator hint, value_type&& value)
        {
            return m_set.insert(hint, std::move(value));
        }

        template < typename InputIt >
        void insert(InputIt first, InputIt last)
        {
            m_set.insert(first, last);
        }

        void insert(std::initializer_list< value_type > init)
        {
            m_set.insert(init);
        }

        insert_return_type insert(node_type&& node)
        {
            return m_set.insert(std::move(node));
        }

        iterator insert(const_iterator hint, node_type&& node)
        {
            return m_set.insert(hint, std::move(node));
        }

        template < typename... Args >
        std::pair< iterator, bool > emplace(Args&&... args)
        {
            return m_set.emplace(std::forward< Args >(args)...);
        }

        template < typename... Args >
        iterator emplace_hint(const_iterator hint, Args&&... args)
        {
            return m_set.emplace_hint(hint, std::forward< Args >(args)...);
        }

        iterator erase(const_iterator pos)
        {
            return m_set.erase(pos);
        }

        iterator erase(const_iterator first, const_iterator last)
        {
            return m_set.erase(first, last);
        }

        size_type erase(key_type const& key)
        {
            return m_set.erase(key);
        }

        void swap(set& other) noexcept(noexcept(m_set.swap(other.m_set)))
        {
            m_set.swap(other.m_set);
        }

        node_type extract(const_iterator pos)
        {
            return m_set.extract(pos);
        }

        node_type extract(key_type const& key)
        {
            return m_set.extract(key);
        }

        template < typename C2 >
        void merge(set< Key, C2, Allocator >& source)
        {
            m_set.merge(source.m_set);
        }

        template < typename C2 >
        void merge(set< Key, C2, Allocator >&& source)
        {
            m_set.merge(source.m_set);
        }

        template < typename C2 >
        void merge(std::set< Key, C2, Allocator >& source)
        {
            m_set.merge(source);
        }

        template < typename C2 >
        void merge(std::set< Key, C2, Allocator >&& source)
        {
            m_set.merge(source);
        }

        size_type count(key_type const& key) const
        {
            return m_set.count(key);
        }

        iterator find(key_type const& key)
        {
            return m_set.find(key);
        }

        const_iterator find(key_type const& key) const
        {
            return m_set.find(key);
        }

        bool contains(key_type const& key) const
        {
            return m_set.contains(key);
        }

        std::pair< iterator, iterator > equal_range(key_type const& key)
        {
            return m_set.equal_range(key);
        }

        std::pair< const_iterator, const_iterator > equal_range(key_type const& key) const
        {
            return m_set.equal_range(key);
        }

        iterator lower_bound(key_type const& key)
        {
            return m_set.lower_bound(key);
        }

        const_iterator lower_bound(key_type const& key) const
        {
            return m_set.lower_bound(key);
        }

        iterator upper_bound(key_type const& key)
        {
            return m_set.upper_bound(key);
        }

        const_iterator upper_bound(key_type const& key) const
        {
            return m_set.upper_bound(key);
        }

        key_compare key_comp() const
        {
            return m_set.key_comp();
        }

        value_compare value_comp() const
        {
            return m_set.value_comp();
        }

        template < typename K, typename C, typename A >
        friend class set;

        template < typename K, typename C, typename A >
        friend bool operator==(set< K, C, A > const& lhs, set< K, C, A > const& rhs);

        template < typename K, typename C, typename A >
        friend bool operator<(set< K, C, A > const& lhs, set< K, C, A > const& rhs);

        template < typename K, typename C, typename A >
        friend auto operator<=>(set< K, C, A > const& lhs, set< K, C, A > const& rhs) -> decltype(std::declval< typename set< K, C, A >::underlying_type const& >() <=> std::declval< typename set< K, C, A >::underlying_type const& >());
    };

    /**
     * @brief Swaps two sets.
     */
    template < typename Key, typename Compare, typename Allocator >
    void swap(set< Key, Compare, Allocator >& lhs, set< Key, Compare, Allocator >& rhs) noexcept(noexcept(lhs.swap(rhs)))
    {
        lhs.swap(rhs);
    }

    /**
     * @brief Compares two sets for equality.
     */
    template < typename Key, typename Compare, typename Allocator >
    bool operator==(set< Key, Compare, Allocator > const& lhs, set< Key, Compare, Allocator > const& rhs)
    {
        return lhs.m_set == rhs.m_set;
    }

    /**
     * @brief Compares two sets for inequality.
     */
    template < typename Key, typename Compare, typename Allocator >
    bool operator!=(set< Key, Compare, Allocator > const& lhs, set< Key, Compare, Allocator > const& rhs)
    {
        return !(lhs == rhs);
    }

    /**
     * @brief Orders two sets by size first, then lexicographically.
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
     */
    template < typename Key, typename Compare, typename Allocator >
    bool operator>(set< Key, Compare, Allocator > const& lhs, set< Key, Compare, Allocator > const& rhs)
    {
        return rhs < lhs;
    }

    /**
     * @brief Orders two sets by size first, then lexicographically.
     */
    template < typename Key, typename Compare, typename Allocator >
    bool operator<=(set< Key, Compare, Allocator > const& lhs, set< Key, Compare, Allocator > const& rhs)
    {
        return !(rhs < lhs);
    }

    /**
     * @brief Orders two sets by size first, then lexicographically.
     */
    template < typename Key, typename Compare, typename Allocator >
    bool operator>=(set< Key, Compare, Allocator > const& lhs, set< Key, Compare, Allocator > const& rhs)
    {
        return !(lhs < rhs);
    }

    /**
     * @brief Three-way compares two sets by size first, then lexicographically.
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
