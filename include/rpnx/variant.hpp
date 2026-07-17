// Copyright 2024-2026 Ryan P. Nicholl, rnicholl@protonmail.com

#ifndef RPNX_VARIANT_HPP
#define RPNX_VARIANT_HPP

#include <array>
#include <cassert>
#include <cinttypes>
#include <compare>
#include <cstdint>
#include <memory>
#include <tuple>
#include <typeindex>
#include <typeinfo>
#include <variant>

namespace rpnx
{

    template < typename T, typename... Ts >
    struct index_of;

    // Base case: T matches the first type in the list.
    template < typename T, typename... Ts >
    struct index_of< T, T, Ts... > : std::integral_constant< std::size_t, 0 >
    {
    };

    // Recursive case: T does not match the first type in the list.
    template < typename T, typename U, typename... Ts >
    struct index_of< T, U, Ts... > : std::integral_constant< std::size_t, 1 + index_of< T, Ts... >::value >
    {
    };

    template < typename Allocator, typename... Ts >
    class basic_variant;

    template < typename Allocator >
    class variant_detail
    {
      public:
        template < typename T >
        class is_supported_variant : public std::false_type
        {
        };

        template < typename Allocator2, typename... Ts >
        class is_supported_variant< basic_variant< Allocator2, Ts... > > : public std::true_type
        {
        };

        using default_new_func = void* (*)(Allocator&);
        using copy_func = void* (*)(Allocator&, void const*);
        using delete_func = void (*)(Allocator&, void*) noexcept;
        using less_func = bool (*)(void const*, void const*);
        using equals_func = bool (*)(void const*, void const*);
        using three_way_func = std::strong_ordering (*)(void const*, void const*);
        using new_move_from_func = void* (*)(Allocator&, void*);

        struct variant_info
        {
            copy_func m_copy = nullptr;
            less_func m_less = nullptr;
            equals_func m_equals = nullptr;
            three_way_func m_three_way = nullptr;
            delete_func m_destroy = nullptr;
            default_new_func m_default_new = nullptr;
            new_move_from_func m_new_move_from = nullptr;
            std::type_info const* m_type_info = nullptr;
        };

        // Function templates follow...

        // Helper function to create a variant_info for a specific type
        template < typename T >
        static constexpr variant_info make_variant_info()
        {
            return variant_info{.m_copy = &type_copy_func< T >, .m_less = &type_less_func< T >, .m_equals = &type_equals_func< T >, .m_three_way = &type_three_way_func< T >, .m_destroy = &type_delete_func< T >, .m_default_new = &type_default_new_func< T >, .m_new_move_from = &type_new_from_move_func< T >, .m_type_info = &typeid(T)};
        }

      private:
        template < typename T >
        static void* type_copy_func(Allocator& allocator, void const* source)
        {
            static_assert(!std::is_same_v< T, void >, "T must not be void");
            using alloc_triats = std::allocator_traits< Allocator >;
            using rebound_alloc_type = typename alloc_triats::template rebind_alloc< T >;
            using rebound_alloc_traits = std::allocator_traits< rebound_alloc_type >;

            rebound_alloc_type rebound_alloc(allocator);               // Rebound allocator for type T
            T* ptr = rebound_alloc_traits::allocate(rebound_alloc, 1); // Allocate space for one T

            try
            {
                rebound_alloc_traits::construct(rebound_alloc, ptr,
                                                *static_cast< T const* >(source)); // Construct T using the copy constructor
            }
            catch (...)
            {
                rebound_alloc_traits::deallocate(rebound_alloc, ptr, 1); // Ensure deallocation on exception
                throw;                                                   // Re-throw the exception
            }
            return ptr;
        }

        // Deallocation and destruction logic using Allocator
        template < typename T >
        static void type_delete_func(Allocator& allocator, void* object) noexcept
        {
            using rebound_allocator_type = typename std::allocator_traits< Allocator >::template rebind_alloc< T >;
            rebound_allocator_type typed_allocator(allocator); // Rebind the allocator to T
            T* obj_ptr = static_cast< T* >(object);
            std::allocator_traits< rebound_allocator_type >::destroy(typed_allocator, obj_ptr);       // Destroy the object
            std::allocator_traits< rebound_allocator_type >::deallocate(typed_allocator, obj_ptr, 1); // Deallocate memory
        }

        // Allocation and default construction logic using Allocator
        template < typename T >
        static void* type_default_new_func(Allocator& allocator)
        {
            using allocator_type = typename std::allocator_traits< Allocator >::template rebind_alloc< T >;
            using alloc_traits = std::allocator_traits< allocator_type >;

            allocator_type typed_allocator(allocator);           // Rebind the allocator to T
            T* ptr = alloc_traits::allocate(typed_allocator, 1); // Allocate space for one T

            try
            {
                alloc_traits::construct(typed_allocator, ptr); // Default-construct T
            }
            catch (...)
            {
                alloc_traits::deallocate(typed_allocator, ptr, 1); // Ensure deallocation on exception
                throw;                                             // Re-throw the exception
            }

            return ptr;
        }

        template < typename T >
        static void* type_new_from_move_func(Allocator& allocator, void* source)
        {
            using alloc_traits = std::allocator_traits< Allocator >;
            using rebound_alloc_type = typename alloc_traits::template rebind_alloc< T >;
            using rebound_alloc_traits = std::allocator_traits< rebound_alloc_type >;
            rebound_alloc_type reboundAlloc(allocator);                                      // Rebound allocator for type T
            T* ptr = std::allocator_traits< rebound_alloc_type >::allocate(reboundAlloc, 1); // Allocate space for one T

            try
            {
                rebound_alloc_traits::construct(reboundAlloc, ptr, std::move(*static_cast< T* >(source))); // Move-construct T
            }
            catch (...)
            {
                rebound_alloc_traits::deallocate(reboundAlloc, ptr, 1); // Ensure deallocation on exception
                throw;                                                  // Re-throw the exception
            }
            return ptr;
        }

        // Function to compare two objects of type T for less-than
        template < typename T >
        static bool type_less_func(void const* lhs, void const* rhs)
        {
            return *static_cast< T const* >(lhs) < *static_cast< T const* >(rhs);
        }

        // Function to check equality of two objects of type T
        template < typename T >
        static constexpr bool type_equals_func(void const* lhs, void const* rhs)
        {
            // If
            if constexpr (std::equality_comparable_with< T, T >)
            {
                return *static_cast< T const* >(lhs) == *static_cast< T const* >(rhs);
            }
            else
            {
                // synthesize from <=>
                return *static_cast< T const* >(lhs) <=> *static_cast< T const* >(rhs) == std::strong_ordering::equal;
            }
        }

        // Function to perform a three-way comparison of two objects of type T
        template < typename T >
        static constexpr std::strong_ordering type_three_way_func(void const* lhs, void const* rhs)
        {
            if constexpr (std::three_way_comparable_with< T, T >)
            {
                // If T supports three-way comparison with itself
                return std::strong_order(*static_cast< const T* >(lhs), *static_cast< const T* >(rhs));
            }
            else
            {
                // Fallback for types without three-way comparison support.
                const T& l = *static_cast< const T* >(lhs);
                const T& r = *static_cast< const T* >(rhs);

                static_assert(std::is_same< decltype(l), decltype(r) >::value, "T must be the same type as T");
                if (l < r)
                    return std::strong_ordering::less;
                if (r < l)
                    return std::strong_ordering::greater;
                return std::strong_ordering::equal;
            }
        }
    };

    template < typename... Ts >
    using variant = basic_variant< std::allocator< void >, Ts... >;

    template < typename T, typename... Ts >
    inline constexpr auto& get_as(variant< Ts... >& v)
    {
        return v.template get_as< T >();
    }

    template < typename T, typename... Ts >
    inline constexpr auto const& get_as(variant< Ts... > const& v)
    {
        return v.template get_as< T >();
    }

    enum class call_type { required, optional, except_on_missing };

    enum class dispatch_type { automatic, branching, indirect };

    template < typename V, typename F, typename R, std::size_t N, call_type C >
    inline R apply_nth_visitor(V&& variant, F&& func);

    // Branched version might inline better
    template < std::size_t NBegin, std::size_t NEnd, typename V, typename F, typename R, call_type C >
    inline R apply_nth_visitor_branched(std::size_t index, V&& variant, F&& func)
    {
        if constexpr (NBegin == NEnd - 1)
        {
            return apply_nth_visitor< V&&, F&&, R, NBegin, C >(std::forward<V&&>(variant), std::forward< F &&>(func));
        }
        else
        {
            static constexpr std::size_t Range = (NEnd - NBegin);
            static_assert(Range >= 2, "Range should be at least 2");

            static constexpr std::size_t Mid = NBegin + (Range / 2);
            static_assert(NBegin < Mid && Mid < NEnd, "Mid should be between NBegin and NEnd");

            if (index < Mid)
            {
                return apply_nth_visitor_branched< NBegin, Mid, V&&, F&&, R, C >(index, std::forward< V&& >(variant), std::forward< F&& >(func));
            }
            else
            {
                return apply_nth_visitor_branched< Mid, NEnd, V, F, R, C >(index, std::forward< V&& >(variant), std::forward< F&& >(func));
            }
        }
    }

    template < typename V, typename F, typename R, std::size_t N, call_type C >
    inline R apply_nth_visitor(V&& variant, F&& func)
    {
        if constexpr (C == call_type::required)
        {
            if constexpr (std::is_same_v< R, void >)
            {
                func(variant.template get_n_unchecked< N >());
                return;
            }
            else
            {
                return func(variant.template get_n_unchecked< N >());
            }
        }
        else if constexpr (C == call_type::except_on_missing)
        {
            if constexpr (std::is_invocable_v< F, decltype(variant.template get_n_unchecked< N >()) >)
            {
                if constexpr (std::is_same_v< R, void >)
                {
                    func(variant.template get_n_unchecked< N >());
                    return;
                }
                else
                {
                    return func(variant.template get_n_unchecked< N >());
                }
            }
            else
            {
                throw std::bad_variant_access();
            }
        }
        else if constexpr (C == call_type::optional)
        {
            if constexpr (std::is_invocable_v< F, decltype(variant.template get_n_unchecked< N >()) >)
            {
                if constexpr (std::is_same_v< R, void >)
                {
                    func(variant.template get_n_unchecked< N >());
                    return;
                }
                else
                {
                    return func(variant.template get_n_unchecked< N >());
                }
            }
            else if constexpr (!std::is_same_v< R, void >)
            {
                return R{};
            }
        }
    }

    template < typename V, typename F, typename R >
    using variant_invoke_executor = R (*)(V&&, F&&);

    template < typename F, typename R, typename A, typename... Vs >
    auto consteval variant_invoke_table_gen()
    {
        using vexecptr = variant_invoke_executor< rpnx::basic_variant< A, Vs... >&, F, R >;
        std::array< vexecptr, std::tuple_size_v< std::tuple< Vs... > > > result{};

        update_variant_invoke_table_lvalue< 0, F, R, A, Vs... >(result);

        return result;
    }

    template < typename V, std::size_t N >
    class variant_nth_member;

    template < typename A, typename... Vs, std::size_t N >
    class variant_nth_member< rpnx::basic_variant< A, Vs... >, N >
    {
      public:
        using type = std::tuple_element_t< N, std::tuple< Vs... > >;
    };

    template < typename V >
    class variant_size;

    template < typename A, typename... Vs >
    class variant_size< rpnx::basic_variant< A, Vs... > >
    {
      public:
        using type = std::integral_constant< std::size_t, sizeof...(Vs) >;
        static constexpr std::size_t value = sizeof...(Vs);
    };

    template < typename V >
    static constexpr std::size_t variant_size_v = variant_size< V >::value;

    template < typename V, std::size_t N >
    using variant_nth_member_t = typename variant_nth_member< V, N >::type;

    template < std::size_t N, typename F, typename R, typename V, call_type C >
    constexpr void update_variant_invoke_table2(std::array< variant_invoke_executor< V, F, R >, variant_size_v< std::remove_cvref_t< V > > >& table);

    template < typename F, typename R, typename V, call_type C >
    auto constexpr variant_invoke_table_gen2()
    {
        using vexecptr = variant_invoke_executor< V, F, R >;
        std::array< vexecptr, variant_size_v< std::remove_cvref_t< V > > > result{};

        update_variant_invoke_table2< 0, F, R, V, C >(result);

        return result;
    }

    template < std::size_t N, typename F, typename R, typename V, call_type C >
    constexpr void update_variant_invoke_table2(std::array< variant_invoke_executor< V, F, R >, variant_size_v< std::remove_cvref_t< V > > >& table)
    {
        using invoke_ptr = variant_invoke_executor< V, F, R >;

        if constexpr (N < variant_size_v< std::remove_cvref_t< V > >)
        {
            invoke_ptr ptr = &apply_nth_visitor< V, F, R, N, C >;
            table[N] = ptr;
            update_variant_invoke_table2< N + 1, F, R, V, C >(table);
        }
    }

    template < typename F, typename R, typename V, call_type C >
    inline constexpr auto variant_invoke_table2 = variant_invoke_table_gen2< F, R, V, C >();

    template < typename R, dispatch_type D = dispatch_type::automatic, typename V, typename F >
    inline R apply_visitor(V&& variant, F&& func)
    {
        auto index = variant.index();
        if constexpr (D == dispatch_type::branching || (D == dispatch_type::automatic && variant_size_v< std::remove_cvref_t<V> > <= 8))
        {
            return apply_nth_visitor_branched<0, variant_size_v< std::remove_cvref_t<V> >, V&&, F&&, R, call_type::required>(index, std::forward<V&&>(variant), std::forward<F&&>(func));
        }
        else
        {
            return variant_invoke_table2< F, R, V&&, call_type::required >[index](std::forward< V >(variant), std::forward< F >(func));
        }
        //
    }

    template < typename R, typename V, typename F >
    inline R apply_visitor_checked(V&& variant, F&& func)
    {
        return variant_invoke_table2< F, R, V&&, call_type::except_on_missing >[variant.index()](std::forward< V >(variant), std::forward< F >(func));
    }

    template < typename R, typename V, typename F >
    inline R try_apply_visitor(V&& variant, F&& func)
    {
        return variant_invoke_table2< F, R, V&&, call_type::optional >[variant.index()](std::forward< V >(variant), std::forward< F >(func));
    }

    template < typename A, typename... Ts >
    class variant_convert_to
    {
        basic_variant< A, Ts... >& m_val;

      public:
        variant_convert_to(basic_variant< A, Ts... >& val) : m_val(val)
        {
        }
        template < typename T2 >
        bool operator()(T2&& other) const
        {
            m_val = std::forward< T2 >(other);
            return true;
        }
    };

    /**
     * @brief Heap-backed tagged union with allocator-aware storage.
     *
     * The active alternative is tracked by a runtime index and each stored value
     * is allocated through @p Allocator rebound to the concrete held type.
     *
     * @tparam Allocator Allocator used to allocate and destroy held values.
     * @tparam Ts Variant alternative types.
     */
    template < typename Allocator, typename... Ts >
    class basic_variant
    {
        struct variant_impl_info
        {
            /// @brief Type-erased operations for one alternative type.
            typename variant_detail< Allocator >::variant_info m_general_info;
            /// @brief Zero-based index of this alternative in `Ts...`.
            std::size_t m_index = 0;
        };

        /**
         * @brief Builds compile-time metadata for alternative @p N.
         * @tparam N Alternative index.
         * @return Metadata record for the selected alternative.
         */
        template < std::size_t N >
        static consteval variant_impl_info calc_info()
        {
            using type = typename std::tuple_element< N, std::tuple< Ts... > >::type;
            variant_impl_info result{};
            auto v_info = variant_detail< Allocator >::template make_variant_info< type >();

            result.m_general_info = v_info;
            result.m_index = N;
            return result;
        }

        template < std::size_t N >
        static constexpr variant_impl_info s_v_info_for = calc_info< N >();

        /// @brief Pointer to currently-held object storage, or `nullptr` when valueless.
        void* m_data = nullptr;
        /// @brief Pointer to metadata for the active alternative, or `nullptr` when valueless.
        variant_impl_info const* m_vinf = nullptr;
        /// @brief Allocator instance used for all allocations/deallocations.
        [[no_unique_address]] Allocator m_alloc;

        /**
         * @brief Checks whether `T2` is exactly one of `Ts...` after cv/ref removal.
         * @tparam T2 Type to inspect.
         * @return `true` if a matching alternative exists.
         */
        template < typename T2 >
        static constexpr bool has_cvref_removed_identical_type()
        {
            // If T2 is the same as any of the types in Ts..., return true
            return (std::is_same_v< std::remove_cvref_t< T2 >, Ts > || ...);
        }

        /**
         * @brief Internal helper indicating whether no value is currently held.
         * @return `true` when storage is empty.
         */
        bool valueless() const
        {
            return m_data == nullptr;
        }

        /**
         * @brief Validates internal state invariants.
         * @return `true` if pointer/index invariants are satisfied.
         */
        bool valid() const
        {
            if (m_vinf == nullptr && m_data == nullptr)
            {
                return true;
            }

            if (m_vinf == nullptr || m_data == nullptr)
            {
                return false;
            }

            if (m_vinf->m_index >= std::tuple_size_v< std::tuple< Ts... > >)
            {
                return false;
            }

            return true;
        }

      public:
        /// @brief Allocator type used by this variant.
        using allocator_type = Allocator;

        /**
         * @brief Default-constructs the first alternative.
         * @param alloc Allocator instance used for internal allocations.
         * @throws Any exception thrown by allocating or constructing `Ts[0]`.
         */
        constexpr basic_variant(const allocator_type& alloc = allocator_type()) : m_alloc(alloc)
        {
            assert((m_vinf == nullptr) == (m_data == nullptr));
            m_vinf = &s_v_info_for< 0 >;
            try
            {
                m_data = m_vinf->m_general_info.m_default_new(m_alloc);
            }
            catch (...)
            {
                m_vinf = nullptr;
                throw;
            }

            assert(valid());
        }

        /**
         * @brief Move-constructs from another variant of the same type.
         * @param other Source variant to move from.
         */
        constexpr basic_variant(basic_variant< Allocator, Ts... >&& other) noexcept(std::is_nothrow_move_constructible_v<Allocator>) : m_alloc(std::move(other.m_alloc))
        {
            assert((m_vinf == nullptr) == (m_data == nullptr));

            m_vinf = nullptr;
            m_data = nullptr;

            std::swap(m_vinf, other.m_vinf);
            std::swap(m_data, other.m_data);

            assert((m_vinf == nullptr) == (m_data == nullptr));

            assert(valid());
        }

        /**
         * @brief Copy-constructs from another variant of the same type.
         * @param other Source variant to copy from.
         * @throws Any exception thrown by allocation or copy construction of the held value.
         */
        constexpr basic_variant(basic_variant< Allocator, Ts... > const& other) : m_alloc(std::allocator_traits< Allocator >::select_on_container_copy_construction(other.m_alloc))
        {

            m_vinf = nullptr;
            m_data = nullptr;
            assert((other.m_vinf == nullptr) == (other.m_data == nullptr));

            if (other.m_vinf == nullptr)
            {
                return;
            }
            m_vinf = other.m_vinf;
            try
            {
                m_data = m_vinf->m_general_info.m_copy(m_alloc, other.m_data);
            }
            catch (...)
            {
                m_vinf = nullptr;
                throw;
            }

            assert(valid());
        }

        /**
         * @brief Destroys the currently-held value if present.
         */
        ~basic_variant()
        {
            assert((m_vinf == nullptr) == (m_data == nullptr));
            reset();
            assert((m_vinf == nullptr) == (m_data == nullptr));
        }

        /**
         * @brief Resets the variant to the valueless state.
         */
        void reset()
        {
            assert(valid());
            if (m_vinf != nullptr)
            {
                assert(m_data != nullptr);
                m_vinf->m_general_info.m_destroy(m_alloc, m_data);
                m_data = nullptr;
                m_vinf = nullptr;
            }
            assert(valid());
        }

        /**
         * @brief Converting copy constructor from another compatible variant type.
         * @tparam Ts2 Source variant alternatives.
         * @param other Source variant.
         * @throws Any exception thrown while assigning the converted held value.
         */
        template < typename... Ts2 >
        basic_variant(basic_variant< Allocator, Ts2... > const& other, std::enable_if_t< !std::is_same_v< basic_variant< Allocator, Ts... >, basic_variant< Allocator, Ts2... > > && !has_cvref_removed_identical_type< basic_variant< Allocator, Ts2... > >(), int > = 0) : basic_variant()
        {
            assert(valid());
            // reset();
            assert((m_vinf == nullptr) == (m_data == nullptr));
            rpnx::apply_visitor< bool >(other, variant_convert_to< Allocator, Ts... >(*this));
            assert(valid());
        }

        /**
         * @brief Indicates whether this variant can be constructed from @p T.
         * @tparam T Candidate source type.
         * @return `true` when `T` can initialize one of `Ts...`, excluding self-type conversion.
         */
        template < typename T >
        static consteval bool can_construct_subtype_with()
        {
            // Don't construct members using a reference to selftype, even if this looks possible
            // because this is usually not what was intended.
            // This can occur for example,  expression = variant<plus, negate>, struct negate { expression expr; }
            // In this case, a negate can be constructed using a single expression argument, which can ab
            if constexpr (std::is_same_v< std::remove_cvref_t< T >, basic_variant< Allocator, Ts... > >)
            {
                return false;
            }
            else if constexpr (requires { typename std::remove_cvref_t< T >::value_type; })
            {
                if constexpr (std::is_same_v< typename std::remove_cvref_t< T >::value_type, basic_variant< Allocator, Ts... > >)
                {
                    return false;
                }
                else
                {
                    return (std::is_convertible_v< T, Ts > || ...);
                }
            }
            else
            {
                // And there must be some constructible member type.
                return (std::is_convertible_v< T, Ts > || ...);
            }
        }

        /**
         * @brief Constructs the variant from a value convertible to one of the alternatives.
         * @tparam T2 Source type.
         * @param value Value used to initialize the held alternative.
         * @param alloc Allocator instance used for internal allocations.
         * @throws Any exception thrown by allocation or construction of the selected alternative.
         */
        template < typename T2 >
        constexpr basic_variant(T2&& value, const allocator_type& alloc = allocator_type(), std::enable_if_t< rpnx::basic_variant< Allocator, Ts... >::can_construct_subtype_with< T2 >(), int > = 0) : m_alloc(alloc)
        {
            constexpr std::size_t index = constructor_index< T2 >();
            using selected_type = std::tuple_element_t< index, std::tuple< Ts... > >;
            using rebound_alloc_type = typename std::allocator_traits< allocator_type >::template rebind_alloc< selected_type >;
            rebound_alloc_type rebound_alloc(m_alloc);

            m_vinf = &s_v_info_for< index >;
            assert(m_vinf->m_index == index);
            assert(m_vinf->m_index < (std::tuple_size_v< std::tuple< Ts... > >));
            try
            {
                // Rebind allocator to allocate memory for the selected alternative type.
                // Rebound allocator
                m_data = std::allocator_traits< rebound_alloc_type >::allocate(rebound_alloc,
                                                                               1); // Allocate memory for the selected alternative.

                // Construct the value in the allocated memory
                std::allocator_traits< rebound_alloc_type >::construct(rebound_alloc, static_cast< selected_type* >(m_data), std::forward< T2 >(value));
            }
            catch (...)
            {
                if (m_data != nullptr)
                {
                    std::allocator_traits< rebound_alloc_type >::deallocate(rebound_alloc, static_cast< selected_type* >(m_data), 1);
                }
                m_vinf = nullptr;

                throw;
            }
            assert(valid());
        }

        /**
         * @brief Assigns from a value convertible to one of the alternatives.
         * @tparam T Source type.
         * @param value Value to store.
         * @return Reference to `*this`.
         * @throws Any exception thrown by allocation or construction of the new value.
         */
        template < typename T, std::enable_if_t< can_construct_subtype_with< T >(), int > = 0 >
        constexpr basic_variant< Allocator, Ts... >& operator=(T&& value)
        {
            assert((m_vinf == nullptr) == (m_data == nullptr));

            if (m_vinf != nullptr)
            {
                assert(m_vinf->m_index < std::tuple_size_v< std::tuple< Ts... > >);
            }
            auto old_vinf = m_vinf;
            auto old_data = m_data;

            m_vinf = nullptr;
            m_data = nullptr;

            constexpr std::size_t index = constructor_index< T >();
            using selected_type = std::tuple_element_t< index, std::tuple< Ts... > >;
            m_vinf = &s_v_info_for< index >;
            assert(m_vinf->m_index == index);
            assert(m_vinf->m_index < (std::tuple_size_v< std::tuple< Ts... > >));

            // Rebind allocator to allocate memory for the selected alternative type.
            using rebound_alloc_type = typename std::allocator_traits< allocator_type >::template rebind_alloc< selected_type >;
            rebound_alloc_type value_alloc(m_alloc); // Rebound allocator
            try
            {
                m_data = value_alloc.allocate(1); // Allocate memory for the selected alternative type
                // Construct the value in the allocated memory
                std::allocator_traits< rebound_alloc_type >::construct(value_alloc, static_cast< selected_type* >(m_data), std::forward< T >(value));
            }
            catch (...)
            {
                if (m_data != nullptr)
                {
                    std::allocator_traits< rebound_alloc_type >::deallocate(value_alloc, static_cast< selected_type* >(m_data), 1);
                }
                m_vinf = nullptr;

                m_vinf = old_vinf;
                m_data = old_data;
                old_vinf = nullptr;
                old_data = nullptr;
                throw;
            }

            if (old_vinf != nullptr)
            {
                old_vinf->m_general_info.m_destroy(m_alloc, old_data);
            }
            assert(valid());
            return *this;
        }

        /**
         * @brief Copy-assigns by value using swap semantics.
         * @param other Source variant.
         * @return Reference to `*this`.
         */
        basic_variant< Allocator, Ts... >& operator=(basic_variant< Allocator, Ts... > other)
        {
            assert(valid());

            m_alloc = other.m_alloc;

            std::swap(m_vinf, other.m_vinf);
            std::swap(m_data, other.m_data);

            assert(valid());

            other.reset();

            return *this;
        }

        /**
         * @brief Returns the held value cast to @p T& via visitor dispatch.
         * @tparam T Target reference type.
         * @return Reference to the held value as `T&`.
         * @throws std::bad_variant_access If conversion is not valid for the active alternative.
         */
        template < typename T >
        T& static_cast_as()
        {
            return this->template apply_visitor< T& >(
                [](auto& arg) -> T&
                {
                    if constexpr (std::is_convertible_v<decltype(arg), T&>) {
                        return static_cast< T& >(arg);
                    } else {
                        // This branch should never be taken at runtime if the variant holds the correct type.
                        // However, apply_visitor instantiates the lambda for all possible types in the variant.
                        // We throw to satisfy the return type and indicate an internal error if it ever happened.
                        throw std::bad_variant_access();
                    }
                });
        }

        /**
         * @brief Returns the held value cast to @p T const& via visitor dispatch.
         * @tparam T Target referenced type.
         * @return Reference to the held value as `T const&`.
         * @throws std::bad_variant_access If conversion is not valid for the active alternative.
         */
        template < typename T >
        T const& static_cast_as() const
        {
            return this->template apply_visitor< T const& >(
                [](auto& arg) -> T const&
                {
                    if constexpr (std::is_convertible_v<decltype(arg), T const&>) {
                        return static_cast< T const& >(arg);
                    } else {
                        throw std::bad_variant_access();
                    }
                });
        }

        /**
         * @brief Retrieves the held value as `T&` with runtime type checking.
         * @tparam T Expected held type.
         * @return Mutable reference to the held value.
         * @throws std::bad_variant_access If the active alternative is not @p T.
         */
        template < typename T >
        T& get_as()
        {
            assert(valid());
            // static_assert(has_cvref_removed_identical_type<T>(), "Must be in type list");
            //  Check if the variant is currently holding a value of type T
            if (m_vinf == nullptr || m_vinf->m_index != index_of< T, Ts... >::value)
            {
                // If it is not, throw an exception
                throw std::bad_variant_access();
            }
            // If it is, return a reference to the value, casted to T
            return *static_cast< T* >(m_data);
        }

        /**
         * @brief Alias of get_as().
         * @tparam T Expected held type.
         * @return Mutable reference to the held value.
         * @throws std::bad_variant_access If the active alternative is not @p T.
         */
        template < typename T >
        T& as()
        {
            assert(valid());
            // static_assert(has_cvref_removed_identical_type<T>(), "Must be in type list");
            //  Check if the variant is currently holding a value of type T
            if (m_vinf == nullptr || m_vinf->m_index != index_of< T, Ts... >::value)
            {
                // If it is not, throw an exception
                throw std::bad_variant_access();
            }
            // If it is, return a reference to the value, casted to T
            return *static_cast< T* >(m_data);
        }

        /**
         * @brief Alias of get_as().
         * @tparam T Expected held type.
         * @return Mutable reference to the held value.
         * @throws std::bad_variant_access If the active alternative is not @p T.
         */
        template < typename T >
        T& unwrap()
        {
            assert(valid());
            // static_assert(has_cvref_removed_identical_type<T>(), "Must be in type list");
            //  Check if the variant is currently holding a value of type T
            if (m_vinf == nullptr || m_vinf->m_index != index_of< T, Ts... >::value)
            {
                // If it is not, throw an exception
                throw std::bad_variant_access();
            }
            // If it is, return a reference to the value, casted to T
            return *static_cast< T* >(m_data);
        }

        /**
         * @brief Retrieves the held value as `T&` without runtime checks.
         * @tparam T Expected held type.
         * @return Mutable reference to the held value.
         * @note Uses assertions in debug builds for validation.
         */
        template < typename T >
        T& unwrap_unchecked()
        {
            assert(valid());
            // static_assert(has_cvref_removed_identical_type<T>(), "Must be in type list");
            //  Check if the variant is currently holding a value of type T
            assert(!(m_vinf == nullptr || m_vinf->m_index != index_of< T, Ts... >::value));
            // If it is, return a reference to the value, casted to T
            return *static_cast< T* >(m_data);
        }

        /**
         * @brief Retrieves the held value as `T const&` without runtime checks.
         * @tparam T Expected held type.
         * @return Const reference to the held value.
         * @note Uses assertions in debug builds for validation.
         */
        template < typename T >
        T const& unwrap_unchecked() const
        {
            assert(valid());
            // static_assert(has_cvref_removed_identical_type<T>(), "Must be in type list");
            //  Check if the variant is currently holding a value of type T
            assert(!(m_vinf == nullptr || m_vinf->m_index != index_of< T, Ts... >::value));
            // If it is, return a reference to the value, casted to T
            return *static_cast< T const* >(m_data);
        }

        /**
         * @brief Retrieves the held value as `T const&` with runtime type checking.
         * @tparam T Expected held type.
         * @return Const reference to the held value.
         * @throws std::bad_variant_access If the active alternative is not @p T.
         */
        template < typename T >
        T const& get_as() const
        {
            assert(valid());
            static_assert(has_cvref_removed_identical_type< T >(), "Must be in type list");

            // Check if the variant is currently holding a value of type T
            if (m_vinf == nullptr || m_vinf->m_index != index_of< T, Ts... >::value)
            {
                // If it is not, throw an exception
                throw std::bad_variant_access();
            }
            assert(valid());
            // If it is, return a reference to the value, casted to T
            return *static_cast< T const* >(m_data);
        }

        /**
         * @brief Const alias of get_as().
         * @tparam T Expected held type.
         * @return Const reference to the held value.
         * @throws std::bad_variant_access If the active alternative is not @p T.
         */
        template < typename T >
        T const& as() const
        {
            assert(valid());
            static_assert(has_cvref_removed_identical_type< T >(), "Must be in type list");

            // Check if the variant is currently holding a value of type T
            if (m_vinf == nullptr || m_vinf->m_index != index_of< T, Ts... >::value)
            {
                // If it is not, throw an exception
                throw std::bad_variant_access();
            }
            assert(valid());
            // If it is, return a reference to the value, casted to T
            return *static_cast< T const* >(m_data);
        }

        /**
         * @brief Const alias of get_as().
         * @tparam T Expected held type.
         * @return Const reference to the held value.
         * @throws std::bad_variant_access If the active alternative is not @p T.
         */
        template < typename T >
        T const& unwrap() const
        {
            assert(valid());
            static_assert(has_cvref_removed_identical_type< T >(), "Must be in type list");

            // Check if the variant is currently holding a value of type T
            if (m_vinf == nullptr || m_vinf->m_index != index_of< T, Ts... >::value)
            {
                // If it is not, throw an exception
                throw std::bad_variant_access();
            }
            assert(valid());
            // If it is, return a reference to the value, casted to T
            return *static_cast< T const* >(m_data);
        }

        /**
         * @brief Retrieves the held value by alternative index with runtime checking.
         * @tparam N Alternative index.
         * @return Const reference to the held value.
         * @throws std::bad_variant_access If the active index is not @p N.
         */
        template < std::size_t N >
        auto const& get_n() const
        {
            assert(valid());

            // Check if the variant is currently holding a value of type T
            if (m_vinf == nullptr || m_vinf->m_index != N)
            {
                // If it is not, throw an exception
                throw std::bad_variant_access();
            }

            // If it is, return a reference to the value, casted to T
            assert(valid());
            return *static_cast< typename std::tuple_element< N, std::tuple< Ts... > >::type const* >(m_data);
        }

        /**
         * @brief Retrieves the held value by alternative index without runtime checking.
         * @tparam N Alternative index.
         * @return Const reference to the held value.
         * @note Uses assertions in debug builds for validation.
         */
        template < std::size_t N >
        auto const& get_n_unchecked() const
        {
            assert(valid());

            return *static_cast< typename std::tuple_element< N, std::tuple< Ts... > >::type const* >(m_data);
        }

        /**
         * @brief Equality comparison.
         * @param other Variant to compare against.
         * @return `true` if both active alternative and value are equal.
         * @throws std::bad_variant_access If either variant is valueless.
         */
        bool operator==(basic_variant< Allocator, Ts... > const& other) const
        {
            assert(valid());
            if (m_vinf == nullptr || other.m_vinf == nullptr)
            {
                throw std::bad_variant_access();
            }
            if (m_vinf->m_index != other.m_vinf->m_index)
            {
                return false;
            }
            assert(valid());
            return m_vinf->m_general_info.m_equals(m_data, other.m_data);
        }

        /**
         * @brief Inequality comparison.
         * @param other Variant to compare against.
         * @return `true` if variants are not equal.
         * @throws std::bad_variant_access If either variant is valueless.
         */
        bool operator!=(basic_variant< Allocator, Ts... > const& other) const
        {
            assert(valid());
            return !(*this == other);
        }

      public:
        /**
         * @brief Strict-weak ordering comparison.
         * @param other Variant to compare against.
         * @return `true` if `*this` is ordered before @p other.
         * @throws std::bad_variant_access If either variant is valueless.
         */
        bool operator<(basic_variant< Allocator, Ts... > const& other) const
        {
            assert(valid());
            if (m_vinf == nullptr || other.m_vinf == nullptr)
            {
                throw std::bad_variant_access();
            }
            if (m_vinf->m_index != other.m_vinf->m_index)
            {
                return m_vinf->m_index < other.m_vinf->m_index;
            }
            assert(valid());
            return m_vinf->m_general_info.m_less(m_data, other.m_data);
        }

      public:
        /**
         * @brief Three-way comparison.
         * @param other Variant to compare against.
         * @return Strong ordering result between the two variants.
         * @throws std::bad_variant_access If either variant is valueless.
         */
        std::strong_ordering operator<=>(basic_variant< Allocator, Ts... > const& other) const
        {
            assert(valid());
            if (m_vinf == nullptr || other.m_vinf == nullptr)
            {
                throw std::bad_variant_access();
            }

            if (m_vinf->m_index != other.m_vinf->m_index)
            {
                return m_vinf->m_index <=> other.m_vinf->m_index;
            }
            return m_vinf->m_general_info.m_three_way(m_data, other.m_data);
        }

        /**
         * @brief Checks whether the active alternative is exactly @p T.
         * @tparam T Type to test.
         * @return `true` if @p T is the active alternative.
         */
        template < typename T >
        bool type_is() const
        {
            assert(valid());
            // Check if the variant is currently holding a value of type T
            return m_vinf != nullptr && m_vinf->m_index == index_of< T, Ts... >::value;
        }

        /**
         * @brief Checks whether the active alternative is any of @p Ts2.
         * @tparam Ts2 Types to test.
         * @return `true` if active type is in `Ts2...`.
         */
        template < typename... Ts2 >
        bool type_any_of() const
        {
            assert(valid());
            if (m_vinf == nullptr)
            {
                return false;
            }
            return (type_is< Ts2 >() || ...);
        }

        /**
         * @brief Applies a visitor to the active alternative.
         * @tparam R Visitor return type.
         * @tparam D Dispatch policy.
         * @tparam F Visitor type.
         * @param func Visitor callable.
         * @return Visitor result.
         */
        template < typename R, dispatch_type D = dispatch_type::automatic, typename F >
        R apply_visitor(F&& func) &
        {
            return rpnx::apply_visitor< R, D >(*this, std::forward< F >(func));
        }

        /**
         * @brief Const lvalue overload of apply_visitor().
         * @tparam R Visitor return type.
         * @tparam D Dispatch policy.
         * @tparam F Visitor type.
         * @param func Visitor callable.
         * @return Visitor result.
         */
        template < typename R, dispatch_type D = dispatch_type::automatic, typename F >
        R apply_visitor(F&& func) const&
        {
            return rpnx::apply_visitor< R, D >(*this, std::forward< F >(func));
        }

        /**
         * @brief Rvalue overload of apply_visitor().
         * @tparam R Visitor return type.
         * @tparam D Dispatch policy.
         * @tparam F Visitor type.
         * @param func Visitor callable.
         * @return Visitor result.
         */
        template < typename R, dispatch_type D = dispatch_type::automatic, typename F >
        R apply_visitor(F&& func) &&
        {
            return rpnx::apply_visitor< R, D >(std::move(*this), std::forward< F >(func));
        }

        /**
         * @brief Applies a visitor and throws if not invocable for the active alternative.
         * @tparam R Visitor return type.
         * @tparam F Visitor type.
         * @param func Visitor callable.
         * @return Visitor result.
         * @throws std::bad_variant_access If @p func cannot be called for the active type.
         */
        template < typename R, typename F >
        R apply_visitor_checked(F&& func) &
        {
            return rpnx::apply_visitor_checked< R >(*this, std::forward< F >(func));
        }

        /**
         * @brief Const lvalue overload of apply_visitor_checked().
         * @tparam R Visitor return type.
         * @tparam F Visitor type.
         * @param func Visitor callable.
         * @return Visitor result.
         * @throws std::bad_variant_access If @p func cannot be called for the active type.
         */
        template < typename R, typename F >
        R apply_visitor_checked(F&& func) const&
        {
            return rpnx::apply_visitor_checked< R >(*this, std::forward< F >(func));
        }

        /**
         * @brief Rvalue overload of apply_visitor_checked().
         * @tparam R Visitor return type.
         * @tparam F Visitor type.
         * @param func Visitor callable.
         * @return Visitor result.
         * @throws std::bad_variant_access If @p func cannot be called for the active type.
         */
        template < typename R, typename F >
        R apply_visitor_checked(F&& func) &&
        {
            return rpnx::apply_visitor_checked< R >(std::move(*this), std::forward< F >(func));
        }


        /**
         * @brief Applies a visitor and returns default `R{}` when not invocable for the active type.
         * @tparam R Visitor return type.
         * @tparam F Visitor type.
         * @param func Visitor callable.
         * @return Visitor result or default-constructed `R`.
         */
        template < typename R, typename F >
        R try_apply_visitor(F&& func) &
        {
            return rpnx::try_apply_visitor< R >(*this, std::forward< F >(func));
        }

        /**
         * @brief Const lvalue overload of try_apply_visitor().
         * @tparam R Visitor return type.
         * @tparam F Visitor type.
         * @param func Visitor callable.
         * @return Visitor result or default-constructed `R`.
         */
        template < typename R, typename F >
        R try_apply_visitor(F&& func) const&
        {
            return rpnx::try_apply_visitor< R >(*this, std::forward< F >(func));
        }

        /**
         * @brief Rvalue overload of try_apply_visitor().
         * @tparam R Visitor return type.
         * @tparam F Visitor type.
         * @param func Visitor callable.
         * @return Visitor result or default-constructed `R`.
         */
        template < typename R, typename F >
        R try_apply_visitor(F&& func) &&
        {
            return rpnx::try_apply_visitor< R >(std::move(*this), std::forward< F >(func));
        }
        /**
         * @brief Invokes @p func with the held value if it is of type @p T.
         * @tparam T Type to match.
         * @tparam F Callable type.
         * @param func Callable invoked on match.
         * @return `true` if the type matched and callable was invoked.
         */
        template < typename T, typename F >
        bool match(F&& func)
        {
            if (type_is< T >())
            {
                func(get_as< T >());
                return true;
            }

            return false;
        }
        /**
         * @brief Invokes @p func with the held value if it is of type @p T and returns predicate result.
         * @tparam T Type to test.
         * @tparam F Callable type.
         * @param func Predicate callable.
         * @return Predicate result on match, otherwise `false`.
         */
        template < typename T, typename F >
        bool test(F&& func)
        {
            if (type_is< T >())
            {
                return func(get_as< T >());
            }
            return false;
        }

        /**
         * @brief Const overload of match().
         * @tparam T Type to match.
         * @tparam F Callable type.
         * @param func Callable invoked on match.
         * @return `true` if the type matched and callable was invoked.
         */
        template < typename T, typename F >
        bool match(F&& func) const
        {
            if (type_is< T >())
            {
                func(get_as< T >());
                return true;
            }

            return false;
        }

        /**
         * @brief Const overload of test().
         * @tparam T Type to test.
         * @tparam F Callable type.
         * @param func Predicate callable.
         * @return Predicate result on match, otherwise `false`.
         */
        template < typename T, typename F >
        bool test(F&& func) const
        {
            if (type_is< T >())
            {
                return func(get_as< T >());
            }

            return false;
        }

        /**
         * @brief Returns pointer to held value if active type is @p T.
         * @tparam T Type to retrieve.
         * @return Pointer to held value, or `nullptr` if type does not match.
         */
        template < typename T >
        T* cast_ptr()
        {
            assert(valid());
            if (m_vinf == nullptr || m_vinf->m_index != index_of< T, Ts... >::value)
            {
                return nullptr;
            }
            return static_cast< T* >(m_data);
        }

        /**
         * @brief Const overload of cast_ptr().
         * @tparam T Type to retrieve.
         * @return Pointer to held value, or `nullptr` if type does not match.
         */
        template < typename T >
        T const* cast_ptr() const
        {
            assert(valid());
            if (m_vinf == nullptr || m_vinf->m_index != index_of< T, Ts... >::value)
            {
                return nullptr;
            }
            return static_cast< T const* >(m_data);
        }

        /**
         * @brief Returns RTTI for the active alternative.
         * @return Reference to `std::type_info` for the active type.
         * @throws std::bad_variant_access If the variant is valueless.
         */
        std::type_info const& type() const
        {
            assert(valid());
            if (m_vinf == nullptr)
            {
                throw std::bad_variant_access();
            }
            return *m_vinf->m_general_info.m_type_info;
        }

        /**
         * @brief Returns `std::type_index` for the active alternative.
         * @return Type index of active alternative.
         * @throws std::bad_variant_access If the variant is valueless.
         */
        std::type_index type_index() const
        {
            assert(valid());
            return std::type_index(type());
        }

        /**
         * @brief Retrieves the held value by alternative index with runtime checking.
         * @tparam N Alternative index.
         * @return Mutable reference to the held value.
         * @throws std::bad_variant_access If the active index is not @p N.
         */
        template < std::size_t N >
        auto& get_n()
        {
            assert(valid());
            if (m_vinf == nullptr || m_vinf->m_index != N)
            {
                throw std::bad_variant_access();
            }

            return *static_cast< std::tuple_element_t< N, std::tuple< Ts... > >* >(m_data);
        }

        /**
         * @brief Retrieves the held value by alternative index without runtime checking.
         * @tparam N Alternative index.
         * @return Mutable reference to the held value.
         * @note Uses assertions in debug builds for validation.
         */
        template < std::size_t N >
        auto& get_n_unchecked()
        {
            assert(valid());

            return *static_cast< std::tuple_element_t< N, std::tuple< Ts... > >* >(m_data);
        }

        /**
         * @brief Returns the active alternative index.
         * @return Zero-based index into `Ts...`.
         * @throws std::bad_variant_access If the variant is valueless.
         */
        std::size_t index() const
        {
            assert(valid());
            if (m_vinf == nullptr) [[unlikely]]
            {
                throw std::bad_variant_access();
            }
            return m_vinf->m_index;
        }

      private:
        /**
         * @brief Finds the first index where `remove_cvref_t<T>` exactly matches an alternative.
         * @tparam T Type to inspect.
         * @tparam N Starting index.
         * @return Matching index, or `sizeof...(Ts)` if not found.
         */
        template < typename T, std::size_t N >
        static consteval std::size_t cvref_removed_identical_index()
        {
            if constexpr (N >= sizeof...(Ts))
            {
                return N;
            }
            else
            {
                if constexpr (std::is_same_v< std::remove_cvref_t< T >, std::tuple_element_t< N, std::tuple< Ts... > > >)
                {
                    return N;
                }
                else
                {
                    return cvref_removed_identical_index< T, N + 1 >();
                }
            }
        }

        /**
         * @brief Resolves which alternative index should be used for construction from @p T.
         * @tparam T Source type.
         * @return Index of exact match (if convertible) or first convertible alternative.
         */
        template < typename T >
        static constexpr std::size_t constructor_index()
        {
            static_assert(can_construct_subtype_with< T >());

            // If T is the same as any of the types in Ts..., return the index of the first match assuming it is convertible
            // otherwise, return the index of the first type in Ts... that T is convertible to

            if constexpr (has_cvref_removed_identical_type< T >())
            {
                constexpr auto exact_index = cvref_removed_identical_index< T, 0 >();
                if constexpr (std::is_convertible_v< T, std::tuple_element_t< exact_index, std::tuple< Ts... > > >)
                {
                    return exact_index;
                }
                else
                {
                    return convertible_index< T, 0 >();
                }
            }
            else
            {
                return convertible_index< T, 0 >();
            }
        }

        /**
         * @brief Finds the first alternative index constructible from @p T.
         * @tparam T Source type.
         * @tparam N Starting index.
         * @return Index of the first convertible alternative.
         */
        template < typename T, std::size_t N >
        static constexpr std::size_t convertible_index()
        {
            static_assert(can_construct_subtype_with< T >());
            if constexpr (std::is_convertible_v< T, std::tuple_element_t< N, std::tuple< Ts... > > >)
            {
                return N;
            }
            else
            {
                return convertible_index< T, N + 1 >();
            }
        }
    };

} // namespace rpnx

//#include <rpnx/demangle.hpp>

#endif // QUXLANG_VARIANT_HPP
