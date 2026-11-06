// Copyright (c) 2026 Ryan P. Nicholl <rnicholl@protonmail.com>
// SPDX-License-Identifier: Apache-2.0

#ifndef RPNXDATASTRUCTURES_SHARED_PTR_HPP
#define RPNXDATASTRUCTURES_SHARED_PTR_HPP

#include <atomic>
#include <cstddef>
#include <utility>

namespace rpnx
{
    template < typename T >
    class shared_ptr;

    /** @brief Construct a shared object and its ownership count in one allocation. */
    template < typename T, typename... Args >
    shared_ptr< T > make_shared(Args&&... args);

    /** @brief Own an inline value through a reference-counted allocation without weak ownership.
     *
     * Separate shared_ptr handles may be copied and destroyed concurrently. Access to
     * the same handle requires external synchronization. The value must be protected
     * separately when mutable access can overlap access through another owner.
     */
    template < typename T >
    class shared_ptr
    {
        /** @brief Store the owner count and value in the same allocation. */
        struct control_block
        {
            std::atomic< std::size_t > owners{1};
            T value;

            /** @brief Construct the value inside its ownership block. */
            template < typename... Args >
            explicit control_block(Args&&... args)
                : value(std::forward< Args >(args)...)
            {
            }
        };

        control_block* block = nullptr;

        /** @brief Adopt the block allocated by make_shared. */
        explicit shared_ptr(control_block* input) noexcept
            : block(input)
        {
        }

        template < typename U, typename... Args >
        friend shared_ptr< U > make_shared(Args&&... args);

      public:
        /** @brief Construct an empty handle. */
        shared_ptr() noexcept = default;

        /** @brief Construct an empty handle. */
        shared_ptr(std::nullptr_t) noexcept
        {
        }

        /** @brief Add an owner of the same value. */
        shared_ptr(shared_ptr const& other) noexcept
            : block(other.block)
        {
            if (block != nullptr)
            {
                block->owners.fetch_add(1, std::memory_order_relaxed);
            }
        }

        /** @brief Transfer ownership from another handle. */
        shared_ptr(shared_ptr&& other) noexcept
            : block(std::exchange(other.block, nullptr))
        {
        }

        /** @brief Release this handle's ownership of its value. */
        ~shared_ptr()
        {
            if (block != nullptr && block->owners.fetch_sub(1, std::memory_order_acq_rel) == 1)
            {
                delete block;
            }
        }

        /** @brief Share ownership of another handle's value. */
        shared_ptr& operator=(shared_ptr const& other) noexcept
        {
            shared_ptr copy(other);
            swap(copy);
            return *this;
        }

        /** @brief Transfer ownership from another handle. */
        shared_ptr& operator=(shared_ptr&& other) noexcept
        {
            shared_ptr moved(std::move(other));
            swap(moved);
            return *this;
        }

        /** @brief Release ownership and leave this handle empty. */
        void reset() noexcept
        {
            shared_ptr empty;
            swap(empty);
        }

        /** @brief Exchange ownership with another handle. */
        void swap(shared_ptr& other) noexcept
        {
            std::swap(block, other.block);
        }

        /** @brief Return the owned value, or null when this handle is empty. */
        T* get() const noexcept
        {
            return block != nullptr ? &block->value : nullptr;
        }

        /** @brief Return the owned value; the handle must be nonempty. */
        T& operator*() const noexcept
        {
            return block->value;
        }

        /** @brief Return the owned value; the handle must be nonempty. */
        T* operator->() const noexcept
        {
            return &block->value;
        }

        /** @brief Report whether this handle owns a value. */
        explicit operator bool() const noexcept
        {
            return block != nullptr;
        }

        /** @brief Return the current owner count, acquiring preceding owner releases.
         *
         * A result of one orders accesses that finished before the other owners
         * released this value. References that outlive those owners are not tracked.
         */
        std::size_t use_count() const noexcept
        {
            return block != nullptr ? block->owners.load(std::memory_order_acquire) : 0;
        }

        /** @brief Return an owner-count estimate without synchronizing with owner releases.
         *
         * The count can change concurrently. A result of one does not establish that
         * prior accesses through other owners have completed.
         */
        std::size_t use_count_estimate() const noexcept
        {
            return block != nullptr ? block->owners.load(std::memory_order_relaxed) : 0;
        }
    };

    template < typename T, typename... Args >
    shared_ptr< T > make_shared(Args&&... args)
    {
        return shared_ptr< T >(new typename shared_ptr< T >::control_block(std::forward< Args >(args)...));
    }
} // namespace rpnx

#endif // RPNXDATASTRUCTURES_SHARED_PTR_HPP
