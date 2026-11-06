// Copyright 2023-2025 Ryan P. Nicholl, rnicholl@protonmail.com
// SPDX-License-Identifier: Apache-2.0
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef RPNX_COW_HEADER_GUARD
#define RPNX_COW_HEADER_GUARD

#include <cstddef>
#include <rpnx/shared_ptr.hpp>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace rpnx
{
    /** @brief Shared owning pointer that copies its value when edit() accesses shared storage.
     *
     * Value access requires has_value(). Mutable references require callers to avoid
     * sharing or transferring ownership of their value until mutation finishes. A
     * reference or pointer returned by an access operation does not retain ownership;
     * its use must finish before its handle releases ownership of that value.
     * Concurrent const access and copying of T must be safe for the chosen T.
     */
    template < typename T >
    class cow
    {
        /** @brief Shared ownership of the inline value. */
        rpnx::shared_ptr< T > ptr;

      public:
        /** @brief Default constructor that initializes the shared pointer with a new object of type T */
        cow()
            : ptr(rpnx::make_shared< T >())
        {
        }

        /** @brief Constructor that initializes the shared pointer with nullptr */
        cow(std::nullptr_t n)
            : ptr(n)
        {
        }

        /** @brief Constructor that initializes the shared pointer with an object of type T */
        cow(T t)
            : ptr(rpnx::make_shared< T >(std::move(t)))
        {
        }

        /** @brief Variadic template constructor to allow construction with an arbitrary number of arguments */
        template < typename... Ts >
        cow(Ts&&... ts)
            : ptr(rpnx::make_shared< T >(std::forward< Ts >(ts)...))
        {
        }

        /** @brief Share ownership of the value. */
        cow(cow< T > const& other)
            : ptr(other.ptr)
        {
        }

        /** @brief Share the value owned by other. */
        cow(cow< T >& other)
            : ptr(other.ptr)
        {
        }

        /** @brief Share ownership of the value from a const rvalue. */
        cow(cow< T > const&& other)
            : ptr(other.ptr)
        {
        }

        /** @brief Transfer ownership from other. */
        cow(cow< T >&& other)
            : ptr(std::move(other.ptr))
        {
        }

        /** @brief Compare the stored values for equality. */
        bool operator==(cow< T > const& other) const
        {
            if (!has_value() || !other.has_value())
            {
                return has_value() == other.has_value();
            }
            return get() == other.get();
        }

        /** @brief Compare the stored values by ordering. */
        bool operator<(cow< T > const& other) const
        {
            if (!other.has_value())
            {
                return false;
            }
            if (!has_value())
            {
                return true;
            }
            return get() < other.get();
        }

        /** @brief Share ownership of the assigned value. */
        cow< T >& operator=(cow< T > const& other)
        {
            ptr = other.ptr;
            return *this;
        }

        /** @brief Transfer ownership of the assigned value. */
        cow< T >& operator=(cow< T >&& other)
        {
            ptr = std::move(other.ptr);
            return *this;
        }

        /** @brief Move-assign a value, replacing shared storage when necessary. */
        cow< T >& operator=(T&& other)
        {
            if (ptr.use_count() == 1)
            {
                *ptr = std::move(other);
            }
            else
            {
                ptr = rpnx::make_shared< T >(std::move(other));
            }
            return *this;
        }

        /** @brief Assign a copy of a value, replacing shared storage. */
        cow< T >& operator=(T const& other)
        {
            if (ptr.use_count() == 1)
            {
                *ptr = other;
            }
            else
            {
                ptr = rpnx::make_shared< T >(other);
            }
            return *this;
        }

        /** @brief Return an immutable reference to the value. */
        T const& get() const
        {
            return *ptr;
        }

        /** @brief Return an immutable reference to the value. */
        T const & read() const
        {
            return *ptr;
        }

        /** @brief Return a mutable reference, copying the value when ownership is shared. */
        T& edit()
        {
            if (ptr.use_count() > 1)
            {
                rpnx::shared_ptr< T > new_ptr = rpnx::make_shared< T >(*ptr);
                ptr = std::move(new_ptr);
            }
            return *ptr;
        }

        /** @brief Replace the stored value with a newly allocated value. */
        void set(T t)
        {
            rpnx::shared_ptr< T > new_ptr = rpnx::make_shared< T >(std::move(t));
            ptr = std::move(new_ptr);
        }

        /** @brief Return an immutable reference to the value. */
        T const& operator*() const
        {
            return *ptr;
        }

        /** @brief Return an immutable pointer to the value. */
        T const* operator->() const
        {
            return ptr.get();
        }

        /** @brief Exchange ownership with another cow. */
        void swap(cow< T >& other)
        {
            ptr.swap(other.ptr);
        }

        /** @brief Expose the immutable value as a tuple for serialization. */
        auto serialize_interface() const
        {
            return std::tie(read());
        }

        /** @brief Expose the editable value as a tuple for deserialization. */
        auto deserialize_interface()
        {
            return std::tie(edit());
        }

        /** @brief Return the serialized field name. */
        static constexpr auto strings()
        {
            return std::vector< std::string >{"cow_value"};
        }

        /** @brief Return a tuple containing an immutable reference to the value. */
        auto tie() const
        {
            return std::tie(read());
        }

        /** @brief Return a tuple containing an editable reference to the value. */
        auto tie()
        {
            return std::tie(edit());
        }

        /** @brief Report whether this pointer owns a value. */
        bool has_value() const
        {
            return static_cast< bool >(ptr);
        }

    };
} // namespace rpnx

#endif // RPNX_COW_HEADER_GUARD
