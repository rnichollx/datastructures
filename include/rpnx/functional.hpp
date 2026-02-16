// Copyright (c) 2026 Ryan P. Nicholl <rnicholl@protonmail.com>

#ifndef RPNXDATASTRUCTURES_FUNCTIONAL_HPP
#define RPNXDATASTRUCTURES_FUNCTIONAL_HPP

#include "rpnx/memory.hpp"
#include <array>
#include <cstddef>
#include <cstring>
#include <type_traits>
#include <utility>

namespace rpnx
{
    template < typename F >
    class function;

    template < typename F >
    class const_function;

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

        static std::size_t constexpr sbo_size = 16;
        static std::size_t constexpr sbo_align = 16;

        callable_f m_callable;
        impl_tbl const* m_impl_tbl;
        alignas(sbo_align) std::array< std::byte, sbo_size > m_storage;



        template < typename Functor >
        static constexpr bool use_sbo()
        {
            return sizeof(Functor) <= sizeof(decltype(m_storage)) && alignof(Functor) <= alignof(decltype(m_storage)) && std::is_nothrow_copy_constructible_v< Functor > && std::is_nothrow_copy_assignable_v< Functor >;
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

        template < typename Function >
        static R free_call_impl(function< R(Args...) >* f, Args... args)
        {
            return Function(args...);
        }


        template < typename Function >
        static void free_copy_ctor_impl(function< R(Args...) >* self, function< R(Args...) > const* other)
        {
            self->m_impl_tbl = &free_vtbl< Function >;
            self->m_callable = &free_call_impl< Function >;
        }

        template < typename Function >
        static void free_copy_assign_impl(function< R(Args...) >* self, function< R(Args...) > const* other)
        {
            self->m_impl_tbl->m_destroy(self);
            self->m_impl_tbl = &free_vtbl< Function >;
            self->m_callable = &free_call_impl< Function >;
        }

        template < typename Function >
        static void free_move_ctor_impl(function< R(Args...) >* self, function< R(Args...) > * other) noexcept
        {
            self->m_impl_tbl = &free_vtbl< Function >;
            self->m_callable = &free_call_impl< Function >;
        }

        template < typename Function >
       static void free_move_assign_impl(function< R(Args...) >* self, function< R(Args...) > * other)
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
                // throws: bad alloc
                auto new_functor = new std::aligned_storage_t< sizeof(Functor), alignof(Functor) >();
                try
                {
                    // maybe throws: copy constructor of Functor
                    // Need to deallocate storage if this happens.
                    new ((void*)new_functor) Functor(*other_ptr);
                }
                catch (...)
                {
                    delete reinterpret_cast< std::aligned_storage_t< sizeof(Functor), alignof(Functor) >* >(new_functor);
                    throw;
                }
                new ((void*)self->m_storage.data()) Functor*(std::launder< Functor >((Functor*)new_functor));
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
                const Functor* other_ptr = std::launder< Functor const >(get_storage_address< Functor >(other));
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
                // throws: bad alloc
                auto new_functor = new std::aligned_storage_t< sizeof(Functor), alignof(Functor) >();
                try
                {
                    // maybe throws: copy constructor of Functor
                    // Need to deallocate storage if this happens.
                    new ((void*)new_functor) Functor(*other_ptr);
                }
                catch (...)
                {
                    delete reinterpret_cast< std::aligned_storage_t< sizeof(Functor), alignof(Functor) >* >(new_functor);
                    throw;
                }
                self->m_impl_tbl->m_destroy(self);
                new ((void*)self->m_storage.data()) Functor*(std::launder< Functor >((Functor*)new_functor));
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

        static void null_move_ctor_impl(function< R(Args...) >* self, function< R(Args...) > * other) noexcept
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

        static void null_move_assign_impl(function< R(Args...) >* self, function< R(Args...) > * other) noexcept
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
            return { .m_copy_ctor = &copy_ctor_impl< Functor >, .m_copy_assign = &copy_assign_impl< Functor >, .m_move_ctor = &move_ctor_impl< Functor >, .m_move_assign = &move_assign_impl< Functor >,.m_reset = &reset_impl< Functor >, .m_destroy = &destroy_impl< Functor >, .m_call = &call_impl< Functor >};
        }

        static constexpr impl_tbl make_null_impl_tbl()
        {
            return {.m_copy_ctor = &null_copy_ctor_impl, .m_copy_assign = &null_copy_assign_impl,.m_move_ctor = &null_move_ctor_impl, .m_move_assign = &null_move_assign_impl,  .m_reset = &null_reset_impl, .m_destroy = &null_destroy_impl, .m_call = &null_call_impl,};
        }

        template < typename Functor >
        static constexpr impl_tbl const vtbl = make_impl_tbl< Functor >();
        template < typename Function >
        static constexpr impl_tbl const free_vtbl = { .m_copy_ctor = &free_copy_ctor_impl<Function>,
                .m_copy_assign = &free_copy_assign_impl<Function>,
                .m_move_ctor = &free_move_ctor_impl<Function>,
                .m_move_assign = &free_move_assign_impl<Function>,
                .m_reset = &null_reset_impl,
                .m_destroy = &null_destroy_impl,
                .m_call = &free_call_impl<Function> };

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

        template < typename Functor, typename = std::enable_if_t< !std::is_same_v< std::decay_t< Functor >, function > > >
        function(Functor&& f)
        {
            using Decayed = std::decay_t< Functor >;
            static_assert(std::is_invocable_r_v< R, Decayed, Args... >, "Functor must be invokable with the correct signature");
            if constexpr (use_sbo< Decayed >())
            {
                new (get_storage_address< Decayed >(this)) Decayed(std::forward< Functor >(f));
                m_impl_tbl = &vtbl< Decayed >;
                m_callable = &call_impl< Decayed >;
            }
            else
            {
                auto storage = new std::aligned_storage_t< sizeof(Decayed), alignof(Decayed) >();
                try
                {
                    new ((void*)storage) Decayed(std::forward< Functor >(f));
                }
                catch (...)
                {
                    delete storage;
                    throw;
                }
                new ((void*)m_storage.data()) Decayed*(std::launder< Decayed >(reinterpret_cast< Decayed* >(storage)));
                m_impl_tbl = &vtbl< Decayed >;
                m_callable = &call_impl< Decayed >;
            }
        }

        function(function const& other)
        {
            other.m_impl_tbl->m_copy_ctor(this, &other);
        }

        function(function&& other) noexcept
        {
            other.m_impl_tbl->m_move_ctor(this, &other);
        }

        function& operator=(function const& other)
        {
            other.m_impl_tbl->m_copy_assign(this, &other);
            return *this;
        }

        function& operator=(function&& other) noexcept
        {
            other.m_impl_tbl->m_move_assign(this, &other);
            return *this;
        }

        operator bool() const noexcept
        {
            return m_callable != &null_call_impl;
        }

        R operator()(Args... args)
        {
            return m_callable(this, std::forward< Args >(args)...);
        }

        bool operator==(std::nullptr_t) const noexcept
        {
            return m_callable == &null_call_impl;
        }
    };

    static_assert(sizeof(rpnx::function<int(int)>) == 32);
} // namespace rpnx

#endif // RPNXDATASTRUCTURES_FUNCTIONAL_HPP
