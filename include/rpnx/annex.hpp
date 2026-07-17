// Copyright (c) 2026 Ryan P. Nicholl <rnicholl@protonmail.com>

#ifndef RPNXDATASTRUCTURES_ANNEX_HPP
#define RPNXDATASTRUCTURES_ANNEX_HPP

#include <compare>
#include <initializer_list>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

namespace rpnx
{
    /**
     * @brief Optional-like value container that stores its payload out of line.
     *
     * `annex` provides an API modeled after `std::optional`, but it stores the
     * contained object through allocator-owned dynamic storage. Empty `annex`
     * objects therefore only contain a pointer and allocator state, which is
     * useful when `T` is large and the empty state is common.
     *
     * Move construction and move assignment transfer the backing pointer when
     * allocator rules allow it. This intentionally leaves the moved-from
     * `annex` empty instead of engaged with a moved-from `T`.
     *
     * @tparam T The type of value stored by the annex.
     * @tparam Alloc Allocator used for the contained `T`.
     */
    template < typename T, typename Alloc = std::allocator< T > >
    class annex
    {
        static_assert(!std::is_reference_v< T >, "rpnx::annex does not support reference types");
        static_assert(!std::is_void_v< T >, "rpnx::annex does not support void");

      public:
        /**
         * @brief The contained value type.
         */
        using value_type = T;

        /**
         * @brief Allocator rebound to `T`.
         */
        using allocator_type = typename std::allocator_traits< Alloc >::template rebind_alloc< T >;

      private:
        using alloc_traits = std::allocator_traits< allocator_type >;

        T* m_value = nullptr;
        [[no_unique_address]] allocator_type m_alloc;

        template < typename... Args >
        T* allocate_construct(Args&&... args)
        {
            T* ptr = alloc_traits::allocate(m_alloc, 1);

            try
            {
                alloc_traits::construct(m_alloc, ptr, std::forward< Args >(args)...);
            }
            catch (...)
            {
                alloc_traits::deallocate(m_alloc, ptr, 1);
                throw;
            }

            return ptr;
        }

        void destroy_deallocate() noexcept
        {
            if (m_value == nullptr)
            {
                return;
            }

            alloc_traits::destroy(m_alloc, m_value);
            alloc_traits::deallocate(m_alloc, m_value, 1);
            m_value = nullptr;
        }

        template < typename U >
        void assign_value(U&& value)
        {
            if (m_value != nullptr)
            {
                **this = std::forward< U >(value);
            }
            else
            {
                m_value = allocate_construct(std::forward< U >(value));
            }
        }

      public:
        /**
         * @brief Constructs an empty annex.
         */
        constexpr annex() noexcept(std::is_nothrow_default_constructible_v< allocator_type >) = default;

        /**
         * @brief Constructs an empty annex from `std::nullopt`.
         */
        constexpr annex(std::nullopt_t) noexcept(std::is_nothrow_default_constructible_v< allocator_type >)
        {
        }

        /**
         * @brief Constructs an empty annex using the specified allocator.
         * @param alloc Allocator used for future contained values.
         */
        explicit annex(allocator_type const& alloc) noexcept : m_alloc(alloc)
        {
        }

        /**
         * @brief Constructs an empty annex using allocator-argument syntax.
         * @param alloc Allocator used for future contained values.
         */
        annex(std::allocator_arg_t, allocator_type const& alloc) noexcept : m_alloc(alloc)
        {
        }

        /**
         * @brief Copy-constructs an annex.
         *
         * If `other` contains a value, the value is copy-constructed into new
         * storage selected by allocator copy-construction rules.
         *
         * @param other Annex to copy from.
         */
        annex(annex const& other) : m_alloc(alloc_traits::select_on_container_copy_construction(other.m_alloc))
        {
            if (other.has_value())
            {
                m_value = allocate_construct(*other);
            }
        }

        /**
         * @brief Move-constructs an annex by transferring its backing pointer.
         *
         * After this operation, `other` is empty.
         *
         * @param other Annex to move from.
         */
        annex(annex&& other) noexcept(std::is_nothrow_move_constructible_v< allocator_type >) : m_value(other.m_value), m_alloc(std::move(other.m_alloc))
        {
            other.m_value = nullptr;
        }

        /**
         * @brief Constructs an annex containing a value.
         * @tparam U Type of the source value.
         * @param value Value used to construct the contained `T`.
         */
        template < typename U = T, typename = std::enable_if_t< std::is_constructible_v< T, U&& > && !std::is_same_v< std::decay_t< U >, annex > && !std::is_same_v< std::decay_t< U >, std::in_place_t > && !std::is_same_v< std::decay_t< U >, std::nullopt_t > && !std::is_same_v< std::decay_t< U >, allocator_type > > >
        annex(U&& value)
        {
            m_value = allocate_construct(std::forward< U >(value));
        }

        /**
         * @brief Constructs the contained value in place.
         * @tparam Args Constructor argument types for `T`.
         * @param args Arguments forwarded to `T`.
         */
        template < typename... Args >
        explicit annex(std::in_place_t, Args&&... args)
        {
            m_value = allocate_construct(std::forward< Args >(args)...);
        }

        /**
         * @brief Constructs the contained value in place with an initializer list.
         * @tparam U Initializer-list element type.
         * @tparam Args Additional constructor argument types for `T`.
         * @param init Initializer list forwarded to `T`.
         * @param args Additional arguments forwarded to `T`.
         */
        template < typename U, typename... Args >
        explicit annex(std::in_place_t, std::initializer_list< U > init, Args&&... args)
        {
            m_value = allocate_construct(init, std::forward< Args >(args)...);
        }

        /**
         * @brief Constructs the contained value in place using the specified allocator.
         * @tparam Args Constructor argument types for `T`.
         * @param alloc Allocator used for the contained value.
         * @param args Arguments forwarded to `T`.
         */
        template < typename... Args >
        annex(std::allocator_arg_t, allocator_type const& alloc, std::in_place_t, Args&&... args) : m_alloc(alloc)
        {
            m_value = allocate_construct(std::forward< Args >(args)...);
        }

        /**
         * @brief Constructs the contained value in place with an initializer list and allocator.
         * @tparam U Initializer-list element type.
         * @tparam Args Additional constructor argument types for `T`.
         * @param alloc Allocator used for the contained value.
         * @param init Initializer list forwarded to `T`.
         * @param args Additional arguments forwarded to `T`.
         */
        template < typename U, typename... Args >
        annex(std::allocator_arg_t, allocator_type const& alloc, std::in_place_t, std::initializer_list< U > init, Args&&... args) : m_alloc(alloc)
        {
            m_value = allocate_construct(init, std::forward< Args >(args)...);
        }

        /**
         * @brief Destroys the contained value, if any, and releases its storage.
         */
        ~annex()
        {
            destroy_deallocate();
        }

        /**
         * @brief Assigns the empty state.
         * @return `*this`.
         */
        annex& operator=(std::nullopt_t) noexcept
        {
            reset();
            return *this;
        }

        /**
         * @brief Copy-assigns another annex.
         * @param other Annex to copy from.
         * @return `*this`.
         */
        annex& operator=(annex const& other)
        {
            if (this == &other)
            {
                return *this;
            }

            if constexpr (alloc_traits::propagate_on_container_copy_assignment::value)
            {
                if (m_alloc != other.m_alloc)
                {
                    reset();
                }

                m_alloc = other.m_alloc;
            }

            if (other.has_value())
            {
                assign_value(*other);
            }
            else
            {
                reset();
            }

            return *this;
        }

        /**
         * @brief Move-assigns another annex.
         *
         * When allocator propagation or equality permits, this transfers the
         * backing pointer and leaves `other` empty. Otherwise, the contained
         * value is move-assigned or move-constructed.
         *
         * @param other Annex to move from.
         * @return `*this`.
         */
        annex& operator=(annex&& other) noexcept(alloc_traits::propagate_on_container_move_assignment::value && std::is_nothrow_move_assignable_v< allocator_type >)
        {
            if (this == &other)
            {
                return *this;
            }

            if constexpr (alloc_traits::propagate_on_container_move_assignment::value)
            {
                reset();
                m_alloc = std::move(other.m_alloc);
                m_value = other.m_value;
                other.m_value = nullptr;
            }
            else
            {
                if (m_alloc == other.m_alloc)
                {
                    reset();
                    m_value = other.m_value;
                    other.m_value = nullptr;
                }
                else if (other.has_value())
                {
                    assign_value(std::move(*other));
                }
                else
                {
                    reset();
                }
            }

            return *this;
        }

        /**
         * @brief Assigns a value to the annex.
         *
         * If a value is already present, it is assigned to. Otherwise, a new
         * contained value is constructed.
         *
         * @tparam U Type of the source value.
         * @param value Value to assign.
         * @return `*this`.
         */
        template < typename U = T, typename = std::enable_if_t< std::is_constructible_v< T, U&& > && std::is_assignable_v< T&, U&& > && !std::is_same_v< std::decay_t< U >, annex > && !std::is_same_v< std::decay_t< U >, std::nullopt_t > > >
        annex& operator=(U&& value)
        {
            assign_value(std::forward< U >(value));
            return *this;
        }

        /**
         * @brief Returns a pointer to the contained value.
         * @pre `has_value()` is true.
         */
        constexpr T const* operator->() const noexcept
        {
            return m_value;
        }

        /**
         * @brief Returns a pointer to the contained value.
         * @pre `has_value()` is true.
         */
        constexpr T* operator->() noexcept
        {
            return m_value;
        }

        /**
         * @brief Returns a const lvalue reference to the contained value.
         * @pre `has_value()` is true.
         */
        constexpr T const& operator*() const& noexcept
        {
            return *m_value;
        }

        /**
         * @brief Returns an lvalue reference to the contained value.
         * @pre `has_value()` is true.
         */
        constexpr T& operator*() & noexcept
        {
            return *m_value;
        }

        /**
         * @brief Returns a const rvalue reference to the contained value.
         * @pre `has_value()` is true.
         */
        constexpr T const&& operator*() const&& noexcept
        {
            return std::move(*m_value);
        }

        /**
         * @brief Returns an rvalue reference to the contained value.
         * @pre `has_value()` is true.
         */
        constexpr T&& operator*() && noexcept
        {
            return std::move(*m_value);
        }

        /**
         * @brief Tests whether the annex contains a value.
         */
        constexpr explicit operator bool() const noexcept
        {
            return has_value();
        }

        /**
         * @brief Tests whether the annex contains a value.
         * @return `true` if a value is present.
         */
        constexpr bool has_value() const noexcept
        {
            return m_value != nullptr;
        }

        /**
         * @brief Returns the contained value.
         * @throws std::bad_optional_access if no value is present.
         */
        T& value() &
        {
            if (!has_value())
            {
                throw std::bad_optional_access();
            }

            return **this;
        }

        /**
         * @brief Returns the contained value.
         * @throws std::bad_optional_access if no value is present.
         */
        T const& value() const&
        {
            if (!has_value())
            {
                throw std::bad_optional_access();
            }

            return **this;
        }

        /**
         * @brief Returns the contained value as an rvalue reference.
         * @throws std::bad_optional_access if no value is present.
         */
        T&& value() &&
        {
            if (!has_value())
            {
                throw std::bad_optional_access();
            }

            return std::move(**this);
        }

        /**
         * @brief Returns the contained value as a const rvalue reference.
         * @throws std::bad_optional_access if no value is present.
         */
        T const&& value() const&&
        {
            if (!has_value())
            {
                throw std::bad_optional_access();
            }

            return std::move(**this);
        }

        /**
         * @brief Returns the contained value or a fallback.
         * @tparam U Fallback value type.
         * @param default_value Value returned when the annex is empty.
         * @return A copy of the contained value, or `default_value` converted to `T`.
         */
        template < typename U >
        constexpr T value_or(U&& default_value) const&
        {
            static_assert(std::is_copy_constructible_v< T >);
            static_assert(std::is_convertible_v< U&&, T >);
            return has_value() ? **this : static_cast< T >(std::forward< U >(default_value));
        }

        /**
         * @brief Returns the contained value or a fallback.
         * @tparam U Fallback value type.
         * @param default_value Value returned when the annex is empty.
         * @return The moved contained value, or `default_value` converted to `T`.
         */
        template < typename U >
        constexpr T value_or(U&& default_value) &&
        {
            static_assert(std::is_move_constructible_v< T >);
            static_assert(std::is_convertible_v< U&&, T >);
            return has_value() ? std::move(**this) : static_cast< T >(std::forward< U >(default_value));
        }

        /**
         * @brief Swaps this annex with another annex.
         * @param other Annex to swap with.
         */
        void swap(annex& other) noexcept(alloc_traits::propagate_on_container_swap::value && std::is_nothrow_swappable_v< allocator_type > && std::is_nothrow_swappable_v< T >)
        {
            using std::swap;

            if constexpr (alloc_traits::propagate_on_container_swap::value)
            {
                swap(m_alloc, other.m_alloc);
                swap(m_value, other.m_value);
            }
            else if (has_value() && other.has_value())
            {
                swap(**this, *other);
            }
            else if (has_value())
            {
                other.m_value = other.allocate_construct(std::move(**this));
                reset();
            }
            else if (other.has_value())
            {
                m_value = allocate_construct(std::move(*other));
                other.reset();
            }
        }

        /**
         * @brief Destroys the contained value and makes the annex empty.
         */
        void reset() noexcept
        {
            destroy_deallocate();
        }

        /**
         * @brief Replaces the contained value by constructing a new value in place.
         * @tparam Args Constructor argument types for `T`.
         * @param args Arguments forwarded to `T`.
         * @return Reference to the newly constructed value.
         */
        template < typename... Args >
        T& emplace(Args&&... args)
        {
            reset();
            m_value = allocate_construct(std::forward< Args >(args)...);
            return **this;
        }

        /**
         * @brief Replaces the contained value using an initializer list.
         * @tparam U Initializer-list element type.
         * @tparam Args Additional constructor argument types for `T`.
         * @param init Initializer list forwarded to `T`.
         * @param args Additional arguments forwarded to `T`.
         * @return Reference to the newly constructed value.
         */
        template < typename U, typename... Args >
        T& emplace(std::initializer_list< U > init, Args&&... args)
        {
            reset();
            m_value = allocate_construct(init, std::forward< Args >(args)...);
            return **this;
        }

        /**
         * @brief Returns a copy of the allocator.
         */
        allocator_type get_allocator() const
        {
            return m_alloc;
        }
    };

    /**
     * @brief Swaps two annex objects.
     */
    template < typename T, typename Alloc >
    void swap(annex< T, Alloc >& lhs, annex< T, Alloc >& rhs) noexcept(noexcept(lhs.swap(rhs)))
    {
        lhs.swap(rhs);
    }

    /**
     * @name Annex-to-annex comparisons
     *
     * Empty annex values compare equal to each other and less than any annex
     * containing a value. Engaged annex values compare through their contained
     * values.
     */
    ///@{

    /**
     * @brief Compares two annex objects for equality.
     */
    template < typename T, typename Alloc >
    bool operator==(annex< T, Alloc > const& lhs, annex< T, Alloc > const& rhs)
    {
        if (lhs.has_value() != rhs.has_value())
        {
            return false;
        }

        return !lhs.has_value() || *lhs == *rhs;
    }

    /**
     * @brief Compares two annex objects for inequality.
     */
    template < typename T, typename Alloc >
    bool operator!=(annex< T, Alloc > const& lhs, annex< T, Alloc > const& rhs)
    {
        return !(lhs == rhs);
    }

    /**
     * @brief Orders two annex objects.
     */
    template < typename T, typename Alloc >
    bool operator<(annex< T, Alloc > const& lhs, annex< T, Alloc > const& rhs)
    {
        return rhs.has_value() && (!lhs.has_value() || *lhs < *rhs);
    }

    /**
     * @brief Orders two annex objects.
     */
    template < typename T, typename Alloc >
    bool operator>(annex< T, Alloc > const& lhs, annex< T, Alloc > const& rhs)
    {
        return rhs < lhs;
    }

    /**
     * @brief Orders two annex objects.
     */
    template < typename T, typename Alloc >
    bool operator<=(annex< T, Alloc > const& lhs, annex< T, Alloc > const& rhs)
    {
        return !(rhs < lhs);
    }

    /**
     * @brief Orders two annex objects.
     */
    template < typename T, typename Alloc >
    bool operator>=(annex< T, Alloc > const& lhs, annex< T, Alloc > const& rhs)
    {
        return !(lhs < rhs);
    }

    /**
     * @brief Three-way compares two annex objects when `T` supports `<=>`.
     */
    template < typename T, typename Alloc >
        requires requires(T const& lhs, T const& rhs) { lhs <=> rhs; }
    auto operator<=>(annex< T, Alloc > const& lhs, annex< T, Alloc > const& rhs) -> decltype(*lhs <=> *rhs)
    {
        if (lhs.has_value() && rhs.has_value())
        {
            return *lhs <=> *rhs;
        }

        if (lhs.has_value())
        {
            return std::strong_ordering::greater;
        }

        if (rhs.has_value())
        {
            return std::strong_ordering::less;
        }

        return std::strong_ordering::equal;
    }

    ///@}

    /**
     * @name Annex-to-nullopt comparisons
     *
     * Empty annex values compare equal to `std::nullopt`; engaged annex values
     * compare greater than `std::nullopt`.
     */
    ///@{

    /**
     * @brief Tests whether an annex is empty.
     */
    template < typename T, typename Alloc >
    bool operator==(annex< T, Alloc > const& lhs, std::nullopt_t) noexcept
    {
        return !lhs.has_value();
    }

    /**
     * @brief Tests whether an annex is empty.
     */
    template < typename T, typename Alloc >
    bool operator==(std::nullopt_t, annex< T, Alloc > const& rhs) noexcept
    {
        return !rhs.has_value();
    }

    /**
     * @brief Tests whether an annex contains a value.
     */
    template < typename T, typename Alloc >
    bool operator!=(annex< T, Alloc > const& lhs, std::nullopt_t) noexcept
    {
        return lhs.has_value();
    }

    /**
     * @brief Tests whether an annex contains a value.
     */
    template < typename T, typename Alloc >
    bool operator!=(std::nullopt_t, annex< T, Alloc > const& rhs) noexcept
    {
        return rhs.has_value();
    }

    /**
     * @brief Returns false because an annex is never less than `std::nullopt`.
     */
    template < typename T, typename Alloc >
    bool operator<(annex< T, Alloc > const&, std::nullopt_t) noexcept
    {
        return false;
    }

    /**
     * @brief Tests whether `std::nullopt` is less than an annex.
     */
    template < typename T, typename Alloc >
    bool operator<(std::nullopt_t, annex< T, Alloc > const& rhs) noexcept
    {
        return rhs.has_value();
    }

    /**
     * @brief Tests whether an annex is less than or equal to `std::nullopt`.
     */
    template < typename T, typename Alloc >
    bool operator<=(annex< T, Alloc > const& lhs, std::nullopt_t) noexcept
    {
        return !lhs.has_value();
    }

    /**
     * @brief Returns true because `std::nullopt` is less than or equal to any annex.
     */
    template < typename T, typename Alloc >
    bool operator<=(std::nullopt_t, annex< T, Alloc > const&) noexcept
    {
        return true;
    }

    /**
     * @brief Tests whether an annex is greater than `std::nullopt`.
     */
    template < typename T, typename Alloc >
    bool operator>(annex< T, Alloc > const& lhs, std::nullopt_t) noexcept
    {
        return lhs.has_value();
    }

    /**
     * @brief Returns false because `std::nullopt` is never greater than an annex.
     */
    template < typename T, typename Alloc >
    bool operator>(std::nullopt_t, annex< T, Alloc > const&) noexcept
    {
        return false;
    }

    /**
     * @brief Returns true because any annex is greater than or equal to `std::nullopt`.
     */
    template < typename T, typename Alloc >
    bool operator>=(annex< T, Alloc > const&, std::nullopt_t) noexcept
    {
        return true;
    }

    /**
     * @brief Tests whether `std::nullopt` is greater than or equal to an annex.
     */
    template < typename T, typename Alloc >
    bool operator>=(std::nullopt_t, annex< T, Alloc > const& rhs) noexcept
    {
        return !rhs.has_value();
    }

    /**
     * @brief Three-way compares an annex with `std::nullopt`.
     */
    template < typename T, typename Alloc >
    std::strong_ordering operator<=>(annex< T, Alloc > const& lhs, std::nullopt_t) noexcept
    {
        return lhs.has_value() ? std::strong_ordering::greater : std::strong_ordering::equal;
    }

    /**
     * @brief Three-way compares `std::nullopt` with an annex.
     */
    template < typename T, typename Alloc >
    std::strong_ordering operator<=>(std::nullopt_t, annex< T, Alloc > const& rhs) noexcept
    {
        return rhs.has_value() ? std::strong_ordering::less : std::strong_ordering::equal;
    }

    ///@}

    /**
     * @name Annex-to-value comparisons
     *
     * Empty annex values compare less than any raw value. Engaged annex values
     * compare through their contained values.
     */
    ///@{

    /**
     * @brief Compares an annex with a raw value for equality.
     */
    template < typename T, typename Alloc, typename U >
        requires(!requires { typename U::value_type; typename U::allocator_type; } || !std::is_same_v< U, annex< typename U::value_type, typename U::allocator_type > >)
    bool operator==(annex< T, Alloc > const& lhs, U const& rhs)
    {
        return lhs.has_value() ? *lhs == rhs : false;
    }

    /**
     * @brief Compares a raw value with an annex for equality.
     */
    template < typename T, typename Alloc, typename U >
        requires(!requires { typename U::value_type; typename U::allocator_type; } || !std::is_same_v< U, annex< typename U::value_type, typename U::allocator_type > >)
    bool operator==(U const& lhs, annex< T, Alloc > const& rhs)
    {
        return rhs.has_value() ? lhs == *rhs : false;
    }

    /**
     * @brief Compares an annex with a raw value for inequality.
     */
    template < typename T, typename Alloc, typename U >
        requires(!requires { typename U::value_type; typename U::allocator_type; } || !std::is_same_v< U, annex< typename U::value_type, typename U::allocator_type > >)
    bool operator!=(annex< T, Alloc > const& lhs, U const& rhs)
    {
        return !(lhs == rhs);
    }

    /**
     * @brief Compares a raw value with an annex for inequality.
     */
    template < typename T, typename Alloc, typename U >
        requires(!requires { typename U::value_type; typename U::allocator_type; } || !std::is_same_v< U, annex< typename U::value_type, typename U::allocator_type > >)
    bool operator!=(U const& lhs, annex< T, Alloc > const& rhs)
    {
        return !(lhs == rhs);
    }

    /**
     * @brief Orders an annex and a raw value.
     */
    template < typename T, typename Alloc, typename U >
        requires(!requires { typename U::value_type; typename U::allocator_type; } || !std::is_same_v< U, annex< typename U::value_type, typename U::allocator_type > >)
    bool operator<(annex< T, Alloc > const& lhs, U const& rhs)
    {
        return lhs.has_value() ? *lhs < rhs : true;
    }

    /**
     * @brief Orders a raw value and an annex.
     */
    template < typename T, typename Alloc, typename U >
        requires(!requires { typename U::value_type; typename U::allocator_type; } || !std::is_same_v< U, annex< typename U::value_type, typename U::allocator_type > >)
    bool operator<(U const& lhs, annex< T, Alloc > const& rhs)
    {
        return rhs.has_value() ? lhs < *rhs : false;
    }

    /**
     * @brief Orders an annex and a raw value.
     */
    template < typename T, typename Alloc, typename U >
        requires(!requires { typename U::value_type; typename U::allocator_type; } || !std::is_same_v< U, annex< typename U::value_type, typename U::allocator_type > >)
    bool operator<=(annex< T, Alloc > const& lhs, U const& rhs)
    {
        return lhs.has_value() ? *lhs <= rhs : true;
    }

    /**
     * @brief Orders a raw value and an annex.
     */
    template < typename T, typename Alloc, typename U >
        requires(!requires { typename U::value_type; typename U::allocator_type; } || !std::is_same_v< U, annex< typename U::value_type, typename U::allocator_type > >)
    bool operator<=(U const& lhs, annex< T, Alloc > const& rhs)
    {
        return rhs.has_value() ? lhs <= *rhs : false;
    }

    /**
     * @brief Orders an annex and a raw value.
     */
    template < typename T, typename Alloc, typename U >
        requires(!requires { typename U::value_type; typename U::allocator_type; } || !std::is_same_v< U, annex< typename U::value_type, typename U::allocator_type > >)
    bool operator>(annex< T, Alloc > const& lhs, U const& rhs)
    {
        return lhs.has_value() ? *lhs > rhs : false;
    }

    /**
     * @brief Orders a raw value and an annex.
     */
    template < typename T, typename Alloc, typename U >
        requires(!requires { typename U::value_type; typename U::allocator_type; } || !std::is_same_v< U, annex< typename U::value_type, typename U::allocator_type > >)
    bool operator>(U const& lhs, annex< T, Alloc > const& rhs)
    {
        return rhs.has_value() ? lhs > *rhs : true;
    }

    /**
     * @brief Orders an annex and a raw value.
     */
    template < typename T, typename Alloc, typename U >
        requires(!requires { typename U::value_type; typename U::allocator_type; } || !std::is_same_v< U, annex< typename U::value_type, typename U::allocator_type > >)
    bool operator>=(annex< T, Alloc > const& lhs, U const& rhs)
    {
        return lhs.has_value() ? *lhs >= rhs : false;
    }

    /**
     * @brief Orders a raw value and an annex.
     */
    template < typename T, typename Alloc, typename U >
        requires(!requires { typename U::value_type; typename U::allocator_type; } || !std::is_same_v< U, annex< typename U::value_type, typename U::allocator_type > >)
    bool operator>=(U const& lhs, annex< T, Alloc > const& rhs)
    {
        return rhs.has_value() ? lhs >= *rhs : true;
    }

    /**
     * @brief Three-way compares an annex with a raw value when supported.
     */
    template < typename T, typename Alloc, typename U >
        requires(!requires { typename U::value_type; typename U::allocator_type; } || !std::is_same_v< U, annex< typename U::value_type, typename U::allocator_type > >) && requires(T const& lhs, U const& rhs) { lhs <=> rhs; }
    auto operator<=>(annex< T, Alloc > const& lhs, U const& rhs) -> decltype(*lhs <=> rhs)
    {
        if (lhs.has_value())
        {
            return *lhs <=> rhs;
        }

        return std::strong_ordering::less;
    }

    /**
     * @brief Three-way compares a raw value with an annex when supported.
     */
    template < typename T, typename Alloc, typename U >
        requires(!requires { typename U::value_type; typename U::allocator_type; } || !std::is_same_v< U, annex< typename U::value_type, typename U::allocator_type > >) && requires(U const& lhs, T const& rhs) { lhs <=> rhs; }
    auto operator<=>(U const& lhs, annex< T, Alloc > const& rhs) -> decltype(lhs <=> *rhs)
    {
        if (rhs.has_value())
        {
            return lhs <=> *rhs;
        }

        return std::strong_ordering::greater;
    }

    ///@}

    /**
     * @brief Constructs an annex containing a decayed copy of a value.
     * @tparam T Source value type.
     * @param value Value used to construct the annex.
     */
    template < typename T >
    annex< std::decay_t< T > > make_annex(T&& value)
    {
        return annex< std::decay_t< T > >(std::forward< T >(value));
    }

    /**
     * @brief Constructs an annex containing a `T` constructed in place.
     * @tparam T Contained value type.
     * @tparam Args Constructor argument types for `T`.
     * @param args Arguments forwarded to `T`.
     */
    template < typename T, typename... Args >
    annex< T > make_annex(Args&&... args)
    {
        return annex< T >(std::in_place, std::forward< Args >(args)...);
    }

    /**
     * @brief Constructs an annex containing a `T` from an initializer list.
     * @tparam T Contained value type.
     * @tparam U Initializer-list element type.
     * @tparam Args Additional constructor argument types for `T`.
     * @param init Initializer list forwarded to `T`.
     * @param args Additional arguments forwarded to `T`.
     */
    template < typename T, typename U, typename... Args >
    annex< T > make_annex(std::initializer_list< U > init, Args&&... args)
    {
        return annex< T >(std::in_place, init, std::forward< Args >(args)...);
    }
} // namespace rpnx

#endif // RPNXDATASTRUCTURES_ANNEX_HPP
