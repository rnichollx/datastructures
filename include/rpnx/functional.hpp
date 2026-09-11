// Copyright (c) 2026 Ryan P. Nicholl <rnicholl@protonmail.com>

#ifndef RPNXDATASTRUCTURES_FUNCTIONAL_HPP
#define RPNXDATASTRUCTURES_FUNCTIONAL_HPP

#include "rpnx/memory.hpp"
#include <array>
#include <cstddef>
#include <cstring>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

namespace rpnx
{
    /**
     * rpnx::function is intended as a faster version of std::function, although it currently lacks allocator support and some other functions.
     * @tparam F The function signature, e.g. R(Args...)
     */
    template < typename F >
    class function;

    template < typename F >
    class const_function;

    /**
     * @brief Owning, copyable type-erased callable with small-buffer optimization.
     *
     * Empty functions compare equal to `nullptr` and throw
     * `std::bad_function_call` when invoked. Small callables with non-throwing
     * copy and move operations are stored inline; other callables are heap
     * allocated.
     *
     * @tparam R Callable return type.
     * @tparam Args Callable argument types.
     */
    template < typename R, typename... Args >
    class function< R(Args...) >
    {
        using callable_f = R (*)(function< R(Args...) >*, Args...);
        using copy_ctor_f = void (*)(function< R(Args...) >*, function< R(Args...) > const*);
        using copy_assign_f = void (*)(function< R(Args...) >*, function< R(Args...) > const*);
        using move_ctor_f = void (*)(function< R(Args...) >*, function< R(Args...) >*) noexcept;
        using move_assign_f = void (*)(function< R(Args...) >*, function< R(Args...) >*) noexcept;
        using reset_f = void (*)(function< R(Args...) >*) noexcept;
        using destroy_f = void (*)(function< R(Args...) >*) noexcept;

        struct impl_tbl
        {
            copy_ctor_f const m_copy_ctor;
            copy_assign_f const m_copy_assign;
            move_ctor_f const m_move_ctor;
            move_assign_f const m_move_assign;
            reset_f const m_reset;
            destroy_f const m_destroy;
            callable_f const m_call;
        };

        /// @brief The size of the storage allocated for SBO optimization.
        static std::size_t constexpr sbo_size = 16;
        /// @brief The alignment of the storage allocated for SBO optimization.
        static std::size_t constexpr sbo_align = 16;

        callable_f m_callable;
        impl_tbl const* m_impl_tbl;
        alignas(sbo_align) std::array< std::byte, sbo_size > m_storage;

        template < typename Functor >
        static constexpr bool use_sbo()
        {
            return sizeof(Functor) <= sbo_size && alignof(Functor) <= sbo_align && std::is_nothrow_copy_constructible_v< Functor > && std::is_nothrow_copy_assignable_v< Functor > && std::is_nothrow_move_constructible_v< Functor >;
        }

        /**
         * A function which returns the address of the stored functor, whether it's stored in SBO or on the heap.
         * @tparam Functor
         * @param f
         * @pre If the functor is a non-SBO type, then the storage must already be initialized to contain a pointer to the heap-allocated functor, otherwise in such cases, the behavior is undefined.
         * @return
         */
        template < typename Functor >
        static Functor* get_storage_address(function< R(Args...) >* f)
        {
            if constexpr (use_sbo< Functor >())
            {
                return std::launder< Functor >(reinterpret_cast< Functor* >(f->m_storage.data()));
            }
            else
            {
                static_assert(alignof(Functor*) <= sbo_align, "Functor pointer must fit in storage for non-SBO case");
                static_assert(sizeof(Functor*) <= sbo_size, "Functor pointer must fit in storage for non-SBO case");
                return std::launder< Functor >(*reinterpret_cast< Functor** >(f->m_storage.data()));
            }
        }

        template < typename Functor >
        static Functor const* get_storage_address(function< R(Args...) > const* f)
        {
            if constexpr (use_sbo< Functor >())
            {
                return std::launder< Functor const >(reinterpret_cast< Functor const* >(f->m_storage.data()));
            }
            else
            {
                static_assert(alignof(Functor*) <= sbo_align, "Functor pointer must fit in storage for non-SBO case");
                static_assert(sizeof(Functor*) <= sbo_size, "Functor pointer must fit in storage for non-SBO case");
                return std::launder< Functor const >(*reinterpret_cast< Functor* const* >(f->m_storage.data()));
            }
        }

        template < auto& function_ref >
        struct dispatch_for
        {
            static constexpr auto& id = function_ref;
        };

        template < auto& function_ref >
        auto make_dispatch_for(decltype(function_ref)&) -> dispatch_for< function_ref >
        {
            return {};
        }

        template < auto& Function >
        static R free_call_impl(function< R(Args...) >* f, Args... args)
        {
            return Function(args...);
        }

        template < auto& Function >
        static void free_copy_ctor_impl(function< R(Args...) >* self, function< R(Args...) > const* other)
        {
            self->m_impl_tbl = &free_vtbl< Function >;
            self->m_callable = &free_call_impl< Function >;
        }

        template < auto& Function >
        static void free_copy_assign_impl(function< R(Args...) >* self, function< R(Args...) > const* other)
        {
            self->m_impl_tbl->m_destroy(self);
            self->m_impl_tbl = &free_vtbl< Function >;
            self->m_callable = &free_call_impl< Function >;
        }

        template < auto& Function >
        static void free_move_ctor_impl(function< R(Args...) >* self, function< R(Args...) >* other) noexcept
        {
            self->m_impl_tbl = &free_vtbl< Function >;
            self->m_callable = &free_call_impl< Function >;
        }

        template < auto& Function >
        static void free_move_assign_impl(function< R(Args...) >* self, function< R(Args...) >* other) noexcept
        {
            self->m_impl_tbl->m_destroy(self);
            self->m_impl_tbl = &free_vtbl< Function >;
            self->m_callable = &free_call_impl< Function >;
        }

        template < typename Functor >
        static void copy_ctor_impl(function< R(Args...) >* self, function< R(Args...) > const* other)
        {
            if constexpr (use_sbo< Functor >())
            {
                Functor* self_ptr = std::launder< Functor >(get_storage_address< Functor >(self));
                const Functor* other_ptr = std::launder< Functor const >(get_storage_address< Functor >(other));
                new (self_ptr) Functor(*other_ptr);
                self->m_impl_tbl = &vtbl< Functor >;
                self->m_callable = &call_impl< Functor >;
            }
            else
            {
                // For non-SBO, we need to allocate a new Functor on the heap and copy-construct it.
                const Functor* other_ptr = get_storage_address< Functor >(other);
                Functor* new_functor = new Functor(*other_ptr);
                new ((void*)self->m_storage.data()) Functor*(new_functor);
                self->m_impl_tbl = &vtbl< Functor >;
                self->m_callable = &call_impl< Functor >;
            }
        }

        template < typename Functor >
        static void move_ctor_impl(function< R(Args...) >* self, function< R(Args...) >* other) noexcept
        {
            if constexpr (use_sbo< Functor >())
            {
                Functor* self_ptr = std::launder< Functor >(get_storage_address< Functor >(self));
                Functor* other_ptr = std::launder< Functor >(get_storage_address< Functor >(other));
                new (self_ptr) Functor(std::move(*other_ptr));
                self->m_impl_tbl = &vtbl< Functor >;
                self->m_callable = &call_impl< Functor >;
            }
            else
            {
                // For move-ctor, just steal the contents
                Functor* other_ptr = std::launder< Functor >(get_storage_address< Functor >(other));
                self->m_impl_tbl = &vtbl< Functor >;
                self->m_callable = &call_impl< Functor >;
                new ((void*)self->m_storage.data()) Functor*(other_ptr);
                *std::launder(reinterpret_cast< Functor** >(other->m_storage.data())) = nullptr;
                other->m_impl_tbl = &void_vtbl;
                other->m_callable = &null_call_impl;
            }
        }

        template < typename Functor >
        static void copy_assign_impl(function< R(Args...) >* self, function< R(Args...) > const* other)
        {
            if (self == other)
            {
                return;
            }

            if constexpr (use_sbo< Functor >())
            {
                self->m_impl_tbl->m_destroy(self);
                poison_region(self, sizeof(self));
                copy_ctor_impl< Functor >(self, other);
                if constexpr (sizeof(Functor) < sizeof(m_storage))
                {
                    // If the functor is smaller than the storage, we can poison the remaining bytes to prevent accidental misuse.
                    poison_region(reinterpret_cast< std::byte* >(self) + sizeof(Functor), sizeof(m_storage) - sizeof(Functor));
                }
            }
            else
            {
                // For non-SBO, we need to allocate a new Functor on the heap and copy-construct it.
                const Functor* other_ptr = get_storage_address< Functor >(other);
                Functor* new_functor = new Functor(*other_ptr);
                self->m_impl_tbl->m_destroy(self);
                new ((void*)self->m_storage.data()) Functor*(new_functor);
                self->m_impl_tbl = &vtbl< Functor >;
                self->m_callable = vtbl< Functor >.m_call;
            }
        }

        template < typename Functor >
        static void move_assign_impl(function< R(Args...) >* self, function< R(Args...) >* other) noexcept
        {
            if (self == other)
            {
                return;
            }
            self->m_impl_tbl->m_destroy(self);
            move_ctor_impl< Functor >(self, other);
        }

        template < typename Functor >
        static void destroy_impl(function< R(Args...) >* f) noexcept
        {
            Functor* ptr = get_storage_address< Functor >(f);
            if constexpr (use_sbo< Functor >())
            {
                ptr->~Functor();
            }
            else
            {
                delete ptr;
            }
        }

        static void null_copy_ctor_impl(function< R(Args...) >* self, function< R(Args...) > const* other) noexcept
        {
            self->m_impl_tbl = &void_vtbl;
            self->m_callable = &null_call_impl;
        }

        static void null_move_ctor_impl(function< R(Args...) >* self, function< R(Args...) >* other) noexcept
        {
            self->m_impl_tbl = &void_vtbl;
            self->m_callable = &null_call_impl;
        }

        static void null_copy_assign_impl(function< R(Args...) >* self, function< R(Args...) > const* other) noexcept
        {
            self->m_impl_tbl->m_destroy(self);
            self->m_impl_tbl = &void_vtbl;
            self->m_callable = &null_call_impl;
        }

        static void null_move_assign_impl(function< R(Args...) >* self, function< R(Args...) >* other) noexcept
        {
            self->m_impl_tbl->m_destroy(self);
            self->m_impl_tbl = &void_vtbl;
            self->m_callable = &null_call_impl;
        }

        static void null_destroy_impl(function< R(Args...) >* f) noexcept
        {
            // No-op since there is no functor to destroy
        }

        static void null_reset_impl(function< R(Args...) >* f) noexcept
        {
            // No-op since the null state is the reset state...
            poison_region(reinterpret_cast< void* >(&f->m_storage), sizeof(f->m_storage));
        }

        template < typename Functor >
        static void reset_impl(function< R(Args...) >* f) noexcept
        {
            destroy_impl< Functor >(f);
            f->m_impl_tbl = &void_vtbl;
            f->m_callable = &null_call_impl;
        }

        template < typename Functor >
        static R call_impl(function< R(Args...) >* f, Args... args)
        {
            Functor* ptr = get_storage_address< Functor >(f);
            return (*ptr)(std::forward< Args >(args)...);
        }

        static R null_call_impl(function< R(Args...) >* f, Args... args)
        {
            throw std::bad_function_call();
        }

        template < typename Functor >
        static constexpr impl_tbl make_impl_tbl()
        {
            return {.m_copy_ctor = &copy_ctor_impl< Functor >, .m_copy_assign = &copy_assign_impl< Functor >, .m_move_ctor = &move_ctor_impl< Functor >, .m_move_assign = &move_assign_impl< Functor >, .m_reset = &reset_impl< Functor >, .m_destroy = &destroy_impl< Functor >, .m_call = &call_impl< Functor >};
        }

        static constexpr impl_tbl make_null_impl_tbl()
        {
            return {
                .m_copy_ctor = &null_copy_ctor_impl,
                .m_copy_assign = &null_copy_assign_impl,
                .m_move_ctor = &null_move_ctor_impl,
                .m_move_assign = &null_move_assign_impl,
                .m_reset = &null_reset_impl,
                .m_destroy = &null_destroy_impl,
                .m_call = &null_call_impl,
            };
        }

        template < typename Functor >
        static constexpr impl_tbl const vtbl = make_impl_tbl< Functor >();
        template < auto& Function >
        static constexpr impl_tbl const free_vtbl = {.m_copy_ctor = &free_copy_ctor_impl< Function >, .m_copy_assign = &free_copy_assign_impl< Function >, .m_move_ctor = &free_move_ctor_impl< Function >, .m_move_assign = &free_move_assign_impl< Function >, .m_reset = &null_reset_impl, .m_destroy = &null_destroy_impl, .m_call = &free_call_impl< Function >};

        static constexpr impl_tbl const void_vtbl = make_null_impl_tbl();

      public:
        function() noexcept : m_callable(&null_call_impl), m_impl_tbl(&void_vtbl)
        {
            poison_region(reinterpret_cast< void* >(&m_storage), sizeof(m_storage));
        }

        ~function() noexcept
        {
            m_impl_tbl->m_destroy(this);
        }

        /**
         * @brief Compile-time identity tag for a free function.
         * @tparam fn Function identified by the tag.
         */
        template < auto& fn >
        struct function_tag
        {
            /// Function represented by this tag.
            static constexpr auto& id = fn;
        };

        /**
         * @brief Creates a compile-time identity tag for a free function.
         * @tparam fn Function to identify.
         * @return A stateless tag identifying `fn`.
         */
        template < auto& fn >
        constexpr function_tag< fn > tag_of(decltype(fn)&)
        {
            return {};
        }

        // Optional convenience: accept a function lvalue reference and forward to tag_of

        /**
         * @brief Constructs a function from a compatible callable.
         * @tparam Functor Callable source type.
         * @param f Callable to own.
         */
        template < typename Functor, typename = std::enable_if_t< !std::is_same_v< std::decay_t< Functor >, function > > >
        function(Functor&& f)
        {
            using Decayed = std::decay_t< Functor >;
            static_assert(std::is_invocable_r_v< R, Decayed&, Args... >, "Functor must be invokable with the correct signature");

            if constexpr (use_sbo< Decayed >())
            {
                new (get_storage_address< Decayed >(this)) Decayed(std::forward< Functor >(f));
                m_impl_tbl = &vtbl< Decayed >;
                m_callable = &call_impl< Decayed >;
            }
            else
            {
                Decayed* storage = new Decayed(std::forward< Functor >(f));
                new ((void*)m_storage.data()) Decayed*(storage);
                m_impl_tbl = &vtbl< Decayed >;
                m_callable = &call_impl< Decayed >;
            }
        }

        /** @brief Copy-constructs a function and its stored callable. @param other Function to copy. */
        function(function const& other)
        {
            other.m_impl_tbl->m_copy_ctor(this, &other);
        }

        /** @brief Move-constructs a function, leaving `other` empty when it owns heap storage. @param other Function to move. */
        function(function&& other) noexcept
        {
            other.m_impl_tbl->m_move_ctor(this, &other);
        }

        /** @brief Copy-assigns a function and its stored callable. @param other Function to copy. @return Reference to this function. */
        function& operator=(function const& other)
        {
            if (this == &other)
            {
                return *this;
            }
            other.m_impl_tbl->m_copy_assign(this, &other);
            return *this;
        }

        /** @brief Move-assigns a function. @param other Function to move. @return Reference to this function. */
        function& operator=(function&& other) noexcept
        {
            if (this == &other)
            {
                return *this;
            }
            other.m_impl_tbl->m_move_assign(this, &other);
            return *this;
        }

        /** @brief Tests whether a callable is stored. @return `true` when invocation is valid. */
        operator bool() const noexcept
        {
            return m_callable != &null_call_impl;
        }

        /**
         * @brief Invokes the stored callable.
         * @param args Arguments forwarded according to the function signature.
         * @return The callable result when `R` is non-void.
         * @throws std::bad_function_call if the function is empty.
         */
        R operator()(Args... args)
        {
            return m_callable(this, std::forward< Args >(args)...);
        }

        /** @brief Tests whether the function is empty. @return `true` when no callable is stored. */
        bool operator==(std::nullptr_t) const noexcept
        {
            return m_callable == &null_call_impl;
        }
    };

    static_assert(sizeof(rpnx::function< int(int) >) == 32);
} // namespace rpnx

#endif // RPNXDATASTRUCTURES_FUNCTIONAL_HPP
