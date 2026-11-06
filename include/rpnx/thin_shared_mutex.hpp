// Copyright (c) 2026 Ryan P. Nicholl <rnicholl@protonmail.com>
// SPDX-License-Identifier: Apache-2.0
#ifndef RPNX_THIN_SHARED_MUTEX_HPP
#define RPNX_THIN_SHARED_MUTEX_HPP

#include <atomic>
#include <cstdint>
#include <limits>
#include <rpnx/detail/mutex_address_wait.hpp>
#include <type_traits>

namespace rpnx::detail
{
    /** @brief Shared ownership protocol stored entirely in one lock-free unsigned integer. */
    template < typename Int >
    class alignas(sizeof(Int)) basic_thin_shared_mutex
    {
        static_assert(std::is_same_v< Int, std::uint32_t > || std::is_same_v< Int, std::uint64_t >);
        static_assert(std::atomic< Int >::is_always_lock_free);
        static_assert(sizeof(std::atomic< Int >) == sizeof(Int));
        static_assert(alignof(std::atomic< Int >) <= sizeof(Int));

        static constexpr Int writer_flag = Int{1} << (std::numeric_limits< Int >::digits - 1);
        static constexpr Int contention_flag = Int{1} << (std::numeric_limits< Int >::digits - 2);
        static constexpr Int reader_mask = contention_flag - 1;
        std::atomic< Int > m_state{0};

        /** @brief Registers a wake obligation and parks while the observed state remains current. */
        void wait_for_change(Int observed) noexcept
        {
            if ((observed & contention_flag) == 0)
            {
                Int waiting = observed | contention_flag;
                if (!m_state.compare_exchange_strong(observed, waiting, std::memory_order_relaxed, std::memory_order_relaxed))
                {
                    return;
                }
                observed = waiting;
            }
            wait_on_mutex_address(&m_state, observed);
        }

      public:
        /** @brief Constructs an unlocked mutex. */
        basic_thin_shared_mutex() noexcept = default;
        /** @brief Prevents copying synchronization ownership. */
        basic_thin_shared_mutex(basic_thin_shared_mutex const&) = delete;
        /** @brief Prevents copying synchronization ownership. */
        basic_thin_shared_mutex& operator=(basic_thin_shared_mutex const&) = delete;
        /** @brief Prevents moving synchronization ownership. */
        basic_thin_shared_mutex(basic_thin_shared_mutex&&) = delete;
        /** @brief Prevents moving synchronization ownership. */
        basic_thin_shared_mutex& operator=(basic_thin_shared_mutex&&) = delete;

        /** @brief Reserves exclusive ownership and waits for existing readers to drain. */
        void lock() noexcept
        {
            Int observed = m_state.load(std::memory_order_relaxed);
            for (;;)
            {
                if ((observed & writer_flag) == 0)
                {
                    if (m_state.compare_exchange_weak(observed, observed | writer_flag, std::memory_order_acquire, std::memory_order_relaxed))
                    {
                        break;
                    }
                }
                else
                {
                    wait_for_change(observed);
                    observed = m_state.load(std::memory_order_relaxed);
                }
            }
            // Reservation prevents further readers. The acquire observing the
            // final reader release also observes earlier reader release sequences.
            while (((observed = m_state.load(std::memory_order_acquire)) & reader_mask) != 0)
            {
                wait_for_change(observed);
            }
        }

        /** @brief Attempts exclusive acquisition without reserving or waiting on failure. */
        bool try_lock() noexcept
        {
            Int observed = m_state.load(std::memory_order_relaxed);
            return (observed & (writer_flag | reader_mask)) == 0 && m_state.compare_exchange_strong(observed, observed | writer_flag, std::memory_order_acquire, std::memory_order_relaxed);
        }

        /** @brief Releases exclusive ownership and broadcasts when contention was registered. */
        void unlock() noexcept
        {
            void* address = &m_state;
            Int previous = m_state.exchange(Int{0}, std::memory_order_release);
            if ((previous & contention_flag) != 0)
            {
                wake_all_mutex_address< Int >(address);
            }
        }

        /** @brief Acquires shared ownership, waiting while a writer is reserved or capacity is full. */
        void lock_shared() noexcept
        {
            Int observed = m_state.load(std::memory_order_relaxed);
            for (;;)
            {
                if ((observed & writer_flag) == 0 && (observed & reader_mask) != reader_mask)
                {
                    if (m_state.compare_exchange_weak(observed, observed + Int{1}, std::memory_order_acquire, std::memory_order_relaxed))
                    {
                        return;
                    }
                }
                else
                {
                    wait_for_change(observed);
                    observed = m_state.load(std::memory_order_relaxed);
                }
            }
        }

        /** @brief Attempts shared acquisition without parking or changing contention state. */
        bool try_lock_shared() noexcept
        {
            Int observed = m_state.load(std::memory_order_relaxed);
            while ((observed & writer_flag) == 0 && (observed & reader_mask) != reader_mask)
            {
                if (m_state.compare_exchange_weak(observed, observed + Int{1}, std::memory_order_acquire, std::memory_order_relaxed))
                {
                    return true;
                }
            }
            return false;
        }

        /** @brief Releases shared ownership and wakes contenders when acquisition becomes possible. */
        void unlock_shared() noexcept
        {
            void* address = &m_state;
            Int previous = m_state.fetch_sub(Int{1}, std::memory_order_release);
            Int readers = previous & reader_mask;
            if ((previous & contention_flag) != 0 && (readers == 1 || readers == reader_mask))
            {
                wake_all_mutex_address< Int >(address);
            }
        }
    };

    /** @brief Lock-free state integer supported by the platform address-wait API. */
    using thin_shared_mutex_int = std::conditional_t< std::atomic< std::uint64_t >::is_always_lock_free && sizeof(std::atomic< std::uint64_t >) == 8 && alignof(std::atomic< std::uint64_t >) <= 8, std::uint64_t, std::uint32_t >;
} // namespace rpnx::detail

namespace rpnx
{
    /**
     * @brief Nonrecursive, process-local shared mutex occupying eight or four bytes.
     *
     * Uses an eight-byte lock-free state integer where available, with 62 reader
     * count bits. Otherwise a four-byte integer provides 30 reader count bits.
     * A reserved writer blocks new readers while existing readers drain.
     * Scheduling is not FIFO and individual threads can starve.
     *
     * Supports standard exclusive/shared lock wrappers and condition_variable_any.
     * Acquisition has acquire ordering; release has release ordering. The owning
     * thread must release the corresponding mode. Recursive acquisition and
     * conversion between modes while holding ownership are invalid.
     *
     * Contended waits use native Windows address waits, Linux futexes, or macOS
     * address waits. Eight-byte Linux states require futex_waitv (Linux 5.16+).
     * macOS requires 14.4 or later. Unexpected wait errors terminate.
     * Each mutex contains only its atomic state integer.
     * Destruction requires an unlocked mutex, no waiting threads and no future
     * access. A releasing thread may finish its address-only wake after destruction.
     */
    using thin_shared_mutex = detail::basic_thin_shared_mutex< detail::thin_shared_mutex_int >;

    static_assert(sizeof(thin_shared_mutex) == sizeof(detail::thin_shared_mutex_int));
    static_assert(alignof(thin_shared_mutex) == sizeof(detail::thin_shared_mutex_int));
} // namespace rpnx

#endif
