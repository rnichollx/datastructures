// Copyright 2024 Ryan P. Nicholl, rnicholl@protonmail.com
// Copyright (c) 2024 Ryan Nicholl $USER_EMAIL

#ifndef RANGE_HPP
#define RANGE_HPP
#include <typeindex>

#include <typeindex>
#include <typeinfo>
#include <utility>

namespace rpnx
{
    /** @brief Provides one stable RTTI identity object per type. @tparam T Type represented by the identity. */
    template < typename T >
    class type_info_holder
    {
      public:
        /// Runtime type identity for `T`.
        static inline const std::type_index type = std::type_index(typeid(T));
    };

    /**
     * @brief Owning type-erased input iterator that yields values by copy.
     *
     * The erased iterator is heap allocated and must be copy constructible.
     * Default-constructed and moved-from instances are empty and may only be
     * destroyed or assigned.
     *
     * @tparam V Value type produced by dereference.
     */
    template < typename V >
    class dyn_input_iter
    {
        struct iterator_vtable
        {
            using get_input_f = V (*)(void* self);
            using copy_f = void* (*)(void const* self);
            using advance_f = void (*)(void* self);
            using delete_f = void (*)(void* self);

            get_input_f v_get_input = {};
            copy_f v_copy = {};
            advance_f v_advance = {};
            delete_f v_delete = {};
            std::type_index const* v_type = &type_info_holder< void >::type;
        };

        template < typename T >
        static constexpr iterator_vtable vtable = {
            .v_get_input = [](void* self) -> V
            {
                return **static_cast< T* >(self);
            },
            .v_copy = [](void const* self) -> void*
            {
                return new T(*static_cast< T const* >(self));
            },
            .v_advance = [](void* self) -> void
            {
                ++(*static_cast< T* >(self));
            },
            .v_delete = [](void* self) -> void
            {
                delete static_cast< T* >(self);
            },
            .v_type = &type_info_holder< T >::type,
        };

        iterator_vtable const* m_vtable;
        void* m_self;

      public:
        /** @brief Constructs an empty iterator. */
        dyn_input_iter() : m_vtable(nullptr), m_self(nullptr)
        {
        }

        /** @brief Erases and owns an iterator value. @tparam It Concrete iterator type. @param it Iterator to store. */
        template < typename It >
        dyn_input_iter(It it) : m_vtable(&vtable< It >), m_self(new It(std::move(it)))
        {
        }

        /** @brief Copies the erased iterator. @param other Iterator to copy. */
        dyn_input_iter(dyn_input_iter< V > const& other) : m_vtable(other.m_vtable), m_self(other.m_vtable == nullptr ? nullptr : other.m_vtable->v_copy(other.m_self))
        {
        }

        /** @brief Moves the erased iterator and leaves the source empty. @param other Iterator to move. */
        dyn_input_iter(dyn_input_iter< V >&& other) noexcept : m_vtable(other.m_vtable), m_self(other.m_self)
        {
            other.m_vtable = nullptr;
            other.m_self = nullptr;
        }

        /** @brief Copy-assigns the erased iterator. @param other Iterator to copy. @return Reference to this iterator. */
        dyn_input_iter< V >& operator=(dyn_input_iter< V > const& other)
        {
            if (this == &other)
            {
                return *this;
            }

            void* copy = other.m_vtable == nullptr ? nullptr : other.m_vtable->v_copy(other.m_self);

            if (m_vtable != nullptr)
            {
                m_vtable->v_delete(m_self);
            }

            m_vtable = other.m_vtable;
            m_self = copy;
            return *this;
        }

        /** @brief Move-assigns the erased iterator and leaves the source empty. @param other Iterator to move. @return Reference to this iterator. */
        dyn_input_iter< V >& operator=(dyn_input_iter< V >&& other) noexcept
        {
            if (this == &other)
            {
                return *this;
            }

            if (m_vtable != nullptr)
            {
                m_vtable->v_delete(m_self);
            }

            m_vtable = other.m_vtable;
            m_self = other.m_self;
            other.m_vtable = nullptr;
            other.m_self = nullptr;
            return *this;
        }

        ~dyn_input_iter()
        {
            if (m_vtable != nullptr)
            {
                m_vtable->v_delete(m_self);
            }
        }

        /** @brief Reads the current element by value. @return Copy or conversion of the referenced element. @pre The iterator is non-empty and dereferenceable. */
        V operator*() const
        {
            return m_vtable->v_get_input(m_self);
        }

        /** @brief Advances the erased iterator. @return Reference to this iterator. @pre The iterator is non-empty and incrementable. */
        dyn_input_iter< V >& operator++()
        {
            m_vtable->v_advance(m_self);
            return *this;
        }

        /** @brief Advances and returns the previous position. @return Independent copy before advancement. */
        dyn_input_iter< V > operator++(int)
        {
            dyn_input_iter< V > copy(*this);
            m_vtable->v_advance(m_self);
            return copy;
        }
    };

    /**
     * @brief Owning type-erased input iterator with equality and ordering.
     *
     * Iterators that erase the same concrete type compare using that type's
     * operators. Empty iterators compare equal; an empty iterator orders before
     * every non-empty iterator. Default-constructed and moved-from instances
     * must not be dereferenced or incremented.
     *
     * @tparam V Value type produced by dereference.
     */
    template < typename V >
    class dyn_comparable_input_iter
    {
        struct iterator_vtable
        {
            using get_input_f = V (*)(void* self);
            using copy_f = void* (*)(void const* self);
            using advance_f = void (*)(void* self);
            using less_f = bool (*)(void const* self, void const* other);
            using equal_f = bool (*)(void const* self, void const* other);
            using delete_f = void (*)(void* self);

            get_input_f v_get_input = {};
            copy_f v_copy = {};
            advance_f v_advance = {};
            less_f v_less = {};
            equal_f v_equal = {};
            delete_f v_delete = {};
            std::type_index const* v_type = &type_info_holder< void >::type;
        };

        template < typename T >
        static constexpr iterator_vtable vtable = {
            .v_get_input = [](void* self) -> V
            {
                return **static_cast< T* >(self);
            },
            .v_copy = [](void const* self) -> void*
            {
                return new T(*static_cast< T const* >(self));
            },
            .v_advance = [](void* self) -> void
            {
                ++(*static_cast< T* >(self));
            },
            .v_less = [](void const* self, void const* other) -> bool
            {
                return *static_cast< T const* >(self) < *static_cast< T const* >(other);
            },
            .v_equal = [](void const* self, void const* other) -> bool
            {
                return *static_cast< T const* >(self) == *static_cast< T const* >(other);
            },
            .v_delete = [](void* self) -> void
            {
                delete static_cast< T* >(self);
            },
            .v_type = &type_info_holder< T >::type,
        };

        iterator_vtable const* m_vtable;
        void* m_self;

      public:
        /** @brief Constructs an empty iterator. */
        dyn_comparable_input_iter() : m_vtable(nullptr), m_self(nullptr)
        {
        }

        /** @brief Erases and owns a comparable iterator value. @tparam It Concrete iterator type. @param it Iterator to store. */
        template < typename It >
        dyn_comparable_input_iter(It it) : m_vtable(&vtable< It >), m_self(new It(std::move(it)))
        {
        }

        /** @brief Copies the erased iterator. @param other Iterator to copy. */
        dyn_comparable_input_iter(dyn_comparable_input_iter< V > const& other) : m_vtable(other.m_vtable), m_self(other.m_vtable == nullptr ? nullptr : other.m_vtable->v_copy(other.m_self))
        {
        }

        /** @brief Moves the erased iterator and leaves the source empty. @param other Iterator to move. */
        dyn_comparable_input_iter(dyn_comparable_input_iter< V >&& other) noexcept : m_vtable(other.m_vtable), m_self(other.m_self)
        {
            other.m_vtable = nullptr;
            other.m_self = nullptr;
        }

        /** @brief Copy-assigns the erased iterator. @param other Iterator to copy. @return Reference to this iterator. */
        dyn_comparable_input_iter< V >& operator=(dyn_comparable_input_iter< V > const& other)
        {
            if (this == &other)
            {
                return *this;
            }

            void* copy = other.m_vtable == nullptr ? nullptr : other.m_vtable->v_copy(other.m_self);

            if (m_vtable != nullptr)
            {
                m_vtable->v_delete(m_self);
            }

            m_vtable = other.m_vtable;
            m_self = copy;
            return *this;
        }

        /** @brief Move-assigns the erased iterator and leaves the source empty. @param other Iterator to move. @return Reference to this iterator. */
        dyn_comparable_input_iter< V >& operator=(dyn_comparable_input_iter< V >&& other) noexcept
        {
            if (this == &other)
            {
                return *this;
            }

            if (m_vtable != nullptr)
            {
                m_vtable->v_delete(m_self);
            }

            m_vtable = other.m_vtable;
            m_self = other.m_self;
            other.m_vtable = nullptr;
            other.m_self = nullptr;
            return *this;
        }

        ~dyn_comparable_input_iter()
        {
            if (m_vtable != nullptr)
            {
                m_vtable->v_delete(m_self);
            }
        }

        /** @brief Reads the current element by value. @return Copy or conversion of the referenced element. @pre The iterator is non-empty and dereferenceable. */
        V operator*() const
        {
            return m_vtable->v_get_input(m_self);
        }

        /** @brief Advances the erased iterator. @return Reference to this iterator. @pre The iterator is non-empty and incrementable. */
        dyn_comparable_input_iter< V >& operator++()
        {
            m_vtable->v_advance(m_self);
            return *this;
        }

        /** @brief Advances and returns the previous position. @return Independent copy before advancement. */
        dyn_comparable_input_iter< V > operator++(int)
        {
            dyn_comparable_input_iter< V > copy(*this);
            m_vtable->v_advance(m_self);
            return copy;
        }

        /** @brief Orders erased iterators. @param other Iterator to compare. @return `true` when this iterator orders first. */
        bool operator<(dyn_comparable_input_iter< V > const& other) const
        {
            if (m_vtable == nullptr && other.m_vtable == nullptr)
            {
                return false;
            }
            if (m_vtable == nullptr)
            {
                return true;
            }
            if (other.m_vtable == nullptr)
            {
                return false;
            }
            if (m_vtable->v_type != other.m_vtable->v_type)
            {
                return m_vtable->v_type < other.m_vtable->v_type;
            }

            if (m_vtable->v_less(m_self, other.m_self))
            {
                return true;
            }

            return false;
        }

        /** @brief Compares erased iterators for equality. @param other Iterator to compare. @return `true` for equal positions of the same erased type, or for two empty iterators. */
        bool operator==(dyn_comparable_input_iter< V > const& other) const
        {
            if (m_vtable == nullptr && other.m_vtable == nullptr)
            {
                return true;
            }
            if (m_vtable == nullptr || other.m_vtable == nullptr)
            {
                return false;
            }

            if (m_vtable->v_type != other.m_vtable->v_type)
            {
                return false;
            }

            return m_vtable->v_equal(m_self, other.m_self);
        }

        /** @brief Compares erased iterators for inequality. @param other Iterator to compare. @return Negation of equality. */
        bool operator!=(dyn_comparable_input_iter< V > const& other) const
        {
            return !(*this == other);
        }
    };

    /**
     * @brief Owning type-erased bidirectional input iterator.
     *
     * Dereference returns `V` by value. Equality is defined only through the
     * concrete erased iterator type; different concrete types compare unequal.
     * Empty instances may only be destroyed, assigned, or compared.
     *
     * @tparam V Value type produced by dereference.
     */
    template < typename V >
    class dyn_bidirectional_input_iter
    {
        struct iterator_vtable
        {
            using get_value_f = V const& (*)(void* self);
            using copy_f = void* (*)(void const* self);
            using advance_f = void (*)(void* self);
            using recede_f = void (*)(void* self);
            using less_f = bool (*)(void const* self, void const* other);
            using equal_f = bool (*)(void const* self, void const* other);
            using delete_f = void (*)(void* self);

            get_value_f v_get_value = {};
            copy_f v_copy = {};
            advance_f v_advance = {};
            recede_f v_recede = {};
            // less_f v_less = {};
            equal_f v_equal = {};
            delete_f v_delete = {};
            std::type_index const* v_type = &type_info_holder< void >::type;
        };

        template < typename T >
        static constexpr iterator_vtable vtable = {
            .v_get_value = [](void* self) -> V const&
            {
                return **static_cast< T* >(self);
            },
            .v_copy = [](void const* self) -> void*
            {
                return new T(*static_cast< T const* >(self));
            },
            .v_advance = [](void* self) -> void
            {
                ++(*static_cast< T* >(self));
            },
            .v_recede = [](void* self) -> void
            {
                --(*static_cast< T* >(self));
            },
            //    .v_less = [](void const* self, void const* other) -> bool { return *static_cast< T const* >(self) < *static_cast< T const* >(other); },
            .v_equal = [](void const* self, void const* other) -> bool
            {
                return *static_cast< T const* >(self) == *static_cast< T const* >(other);
            },
            .v_delete = [](void* self) -> void
            {
                delete static_cast< T* >(self);
            },
            .v_type = &type_info_holder< T >::type,
        };

        iterator_vtable const* m_vtable;
        void* m_self;

      public:
        /** @brief Constructs an empty iterator. */
        dyn_bidirectional_input_iter() : m_vtable(nullptr), m_self(nullptr)
        {
        }

        /** @brief Erases and owns a bidirectional iterator value. @tparam It Concrete iterator type. @param it Iterator to store. */
        template < typename It >
        dyn_bidirectional_input_iter(It it) : m_vtable(&vtable< It >), m_self(new It(std::move(it)))
        {
        }

        /** @brief Copies the erased iterator. @param other Iterator to copy. */
        dyn_bidirectional_input_iter(dyn_bidirectional_input_iter< V > const& other) : m_vtable(other.m_vtable), m_self(other.m_vtable == nullptr ? nullptr : other.m_vtable->v_copy(other.m_self))
        {
        }

        /** @brief Moves the erased iterator and leaves the source empty. @param other Iterator to move. */
        dyn_bidirectional_input_iter(dyn_bidirectional_input_iter< V >&& other) noexcept : m_vtable(other.m_vtable), m_self(other.m_self)
        {
            other.m_vtable = nullptr;
            other.m_self = nullptr;
        }

        /** @brief Copy-assigns the erased iterator. @param other Iterator to copy. @return Reference to this iterator. */
        dyn_bidirectional_input_iter< V >& operator=(dyn_bidirectional_input_iter< V > const& other)
        {
            if (this == &other)
            {
                return *this;
            }

            void* copy = other.m_vtable == nullptr ? nullptr : other.m_vtable->v_copy(other.m_self);

            if (m_vtable != nullptr)
            {
                m_vtable->v_delete(m_self);
            }

            m_vtable = other.m_vtable;
            m_self = copy;
            return *this;
        }

        /** @brief Move-assigns the erased iterator and leaves the source empty. @param other Iterator to move. @return Reference to this iterator. */
        dyn_bidirectional_input_iter< V >& operator=(dyn_bidirectional_input_iter< V >&& other) noexcept
        {
            if (this == &other)
            {
                return *this;
            }

            if (m_vtable != nullptr)
            {
                m_vtable->v_delete(m_self);
            }

            m_vtable = other.m_vtable;
            m_self = other.m_self;
            other.m_vtable = nullptr;
            other.m_self = nullptr;
            return *this;
        }

        ~dyn_bidirectional_input_iter()
        {
            if (m_vtable != nullptr)
            {
                m_vtable->v_delete(m_self);
            }
        }

        /** @brief Reads the current element by value. @return Copy of the referenced element. @pre The iterator is non-empty and dereferenceable. */
        V operator*() const
        {
            return m_vtable->v_get_value(m_self);
        }

        /** @brief Advances one position. @return Reference to this iterator. */
        dyn_bidirectional_input_iter< V >& operator++()
        {
            m_vtable->v_advance(m_self);
            return *this;
        }

        /** @brief Advances and returns the previous position. @return Independent copy before advancement. */
        dyn_bidirectional_input_iter< V > operator++(int)
        {
            dyn_bidirectional_input_iter< V > copy(*this);
            m_vtable->v_advance(m_self);
            return copy;
        }

        /** @brief Retreats one position. @return Reference to this iterator. */
        dyn_bidirectional_input_iter< V >& operator--()
        {
            m_vtable->v_recede(m_self);
            return *this;
        }

        /** @brief Retreats and returns the previous position. @return Independent copy before retreat. */
        dyn_bidirectional_input_iter< V > operator--(int)
        {
            dyn_bidirectional_input_iter< V > copy(*this);
            m_vtable->v_recede(m_self);
            return copy;
        }
        /*

        bool operator<(dyn_bidirectional_input_iter< V > const& other) const
        {
            if (m_vtable == nullptr && other.m_vtable == nullptr)
            {
                return false;
            }
            if (m_vtable == nullptr)
            {
                return true;
            }
            if (other.m_vtable == nullptr)
            {
                return false;
            }
            if (m_vtable->v_type != other.m_vtable->v_type)
            {
                return m_vtable->v_type < other.m_vtable->v_type;
            }

            return m_vtable->v_less(m_self, other.m_self);
        }
*/
        /** @brief Compares erased iterators for equality. @param other Iterator to compare. @return `true` for equal positions of the same erased type, or for two empty iterators. */
        bool operator==(dyn_bidirectional_input_iter< V > const& other) const
        {
            if (m_vtable == nullptr && other.m_vtable == nullptr)
            {
                return true;
            }
            if (m_vtable == nullptr || other.m_vtable == nullptr)
            {
                return false;
            }

            if (m_vtable->v_type != other.m_vtable->v_type)
            {
                return false;
            }

            return m_vtable->v_equal(m_self, other.m_self);
        }

        /** @brief Compares erased iterators for inequality. @param other Iterator to compare. @return Negation of equality. */
        bool operator!=(dyn_bidirectional_input_iter< V > const& other) const
        {
            return !(*this == other);
        }
    };

    /** @brief Lightweight range of type-erased comparable input iterators. @tparam V Value type produced by iteration. */
    template < typename V >
    class dyn_input_range
    {
        dyn_comparable_input_iter< V > m_pos;
        dyn_comparable_input_iter< V > m_end;

      public:
        /** @brief Constructs a half-open range. @param begin First position. @param end One-past-last position with the same erased iterator type. */
        dyn_input_range(dyn_comparable_input_iter< V > begin, dyn_comparable_input_iter< V > end) : m_pos(begin), m_end(end)
        {
        }

        /** @brief Returns the first position by value. @return Beginning iterator. */
        dyn_comparable_input_iter< V > begin() const
        {
            return m_pos;
        }

        /** @brief Returns the one-past-last position by value. @return Ending iterator. */
        dyn_comparable_input_iter< V > end() const
        {
            return m_end;
        }
    };

    /**
     * @brief Owning type-erased output iterator.
     *
     * Dereference returns a proxy whose assignment writes through the erased
     * iterator. Empty and moved-from instances may only be destroyed or assigned.
     *
     * @tparam V Value type accepted by the output proxy.
     */
    template < typename V >
    class dyn_output_iter
    {
        struct iterator_vtable
        {
            using set_output_f = void (*)(void* self, V const& value);
            using copy_f = void* (*)(void const* self);
            using advance_f = void (*)(void* self);
            using delete_f = void (*)(void* self);

            set_output_f v_set_output = {};
            copy_f v_copy = {};
            advance_f v_advance = {};
            delete_f v_delete = {};
            std::type_index const* v_type = &type_info_holder< void >::type;
        };

        template < typename T >
        static constexpr iterator_vtable vtable = {
            .v_set_output =
                [](void* self, V const& value)
            {
                *(*static_cast< T* >(self)) = value;
            },
            .v_copy = [](void const* self) -> void*
            {
                return new T(*static_cast< T const* >(self));
            },
            .v_advance = [](void* self) -> void
            {
                ++(*static_cast< T* >(self));
            },
            .v_delete = [](void* self) -> void
            {
                delete static_cast< T* >(self);
            },
            .v_type = &type_info_holder< T >::type,
        };

        iterator_vtable const* m_vtable;
        void* m_self;

        struct proxy
        {
            dyn_output_iter< V > const* m_self;

            void operator=(V const& value)
            {
                m_self->m_vtable->v_set_output(m_self->m_self, value);
            }
        };

      public:
        /** @brief Constructs an empty output iterator. */
        dyn_output_iter() : m_vtable(nullptr), m_self(nullptr)
        {
        }

        /** @brief Erases and owns an output iterator value. @tparam It Concrete output-iterator type. @param it Iterator to store. */
        template < typename It >
        dyn_output_iter(It it) : m_vtable(&vtable< It >), m_self(new It(std::move(it)))
        {
        }

        /** @brief Moves the erased output iterator and leaves the source empty. @param other Iterator to move. */
        dyn_output_iter(dyn_output_iter< V >&& other) : m_vtable(other.m_vtable), m_self(other.m_self)
        {
            other.m_vtable = nullptr;
            other.m_self = nullptr;
        }

        /** @brief Copies the erased output iterator. @param other Iterator to copy. */
        dyn_output_iter(dyn_output_iter< V > const& other) : m_vtable(other.m_vtable), m_self(other.m_vtable == nullptr ? nullptr : other.m_vtable->v_copy(other.m_self))
        {
        }

        /** @brief Replaces the target with a newly erased output iterator. @tparam It Concrete output-iterator type. @param it Iterator to store. @return Reference to this iterator. */
        template < typename It >
        dyn_output_iter& operator=(It it)
        {
            auto vt = &vtable< It >;
            auto self = new It(std::move(it));

            if (m_vtable)
            {
                m_vtable->v_delete(m_self);
            }

            m_vtable = vt;
            m_self = self;

            return *this;
        }

        /** @brief Copy-assigns the erased output iterator. @param other Iterator to copy. @return Reference to this iterator. */
        dyn_output_iter& operator=(dyn_output_iter< V > const& other)
        {
            if (this == &other)
            {
                return *this;
            }

            if (other.m_vtable == nullptr)
            {
                if (m_vtable)
                {
                    m_vtable->v_delete(m_self);
                }
                m_vtable = nullptr;
                m_self = nullptr;
                return *this;
            }
            auto copy = other.m_vtable->v_copy(other.m_self);

            if (m_vtable)
            {
                m_vtable->v_delete(m_self);
            }

            m_vtable = other.m_vtable;
            m_self = copy;

            return *this;
        }

        /** @brief Move-assigns the erased output iterator and leaves the source empty. @param other Iterator to move. @return Reference to this iterator. */
        dyn_output_iter& operator=(dyn_output_iter< V >&& other)
        {
            if (this == &other)
            {
                return *this;
            }

            if (other.m_vtable == nullptr)
            {
                if (m_vtable)
                {
                    m_vtable->v_delete(m_self);
                }
                m_vtable = nullptr;
                m_self = nullptr;
                return *this;
            }

            if (m_vtable)
            {
                m_vtable->v_delete(m_self);
            }

            m_vtable = other.m_vtable;
            m_self = other.m_self;
            other.m_vtable = nullptr;
            other.m_self = nullptr;

            return *this;
        }

        /** @brief Copy-assigns from a mutable erased output iterator. @param other Iterator to copy. @return Reference to this iterator. */
        dyn_output_iter& operator=(dyn_output_iter< V >& other)
        {
            if (this == &other)
            {
                return *this;
            }

            if (other.m_vtable == nullptr)
            {
                if (m_vtable)
                {
                    m_vtable->v_delete(m_self);
                }
                m_vtable = nullptr;
                m_self = nullptr;
                return *this;
            }
            auto copy = other.m_vtable->v_copy(other.m_self);

            if (m_vtable)
            {
                m_vtable->v_delete(m_self);
            }

            m_vtable = other.m_vtable;
            m_self = copy;

            return *this;
        }

        /** @brief Returns a proxy for writing the current output position. @return Assignment proxy. @pre The iterator is non-empty and writable. */
        proxy operator*() const
        {
            return proxy{.m_self = this};
        }

        /** @brief Advances one output position. @return Reference to this iterator. */
        dyn_output_iter& operator++()
        {
            m_vtable->v_advance(m_self);
            return *this;
        }

        /** @brief Advances and returns the previous position. @return Independent copy before advancement. */
        dyn_output_iter operator++(int)
        {
            dyn_output_iter copy(*this);
            m_vtable->v_advance(m_self);
            return copy;
        }

        ~dyn_output_iter()
        {
            if (m_self)
            {
                m_vtable->v_delete(m_self);
            }
        }

        // Note: Comparison operators (==, !=, <) are typically not used for output iterators
        // but could be implemented similarly to dyn_input_iter if needed.
    };

    class writer
    {
        struct writer_vtable
        {
            void* (*v_copy)(void* self);
            void (*v_destroy)(void* self);
            void (*v_write)(void* self, dyn_input_range< std::byte > data);
        };
    };
} // namespace rpnx

#endif // RANGE_HPP
