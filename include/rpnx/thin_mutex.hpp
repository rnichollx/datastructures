// Copyright (c) 2026 Ryan P. Nicholl <rnicholl@protonmail.com>
// SPDX-License-Identifier: Apache-2.0
#ifndef RPNX_THIN_MUTEX_HPP
#define RPNX_THIN_MUTEX_HPP

#if defined(_WIN32) || defined(__linux__)
#include <atomic>
#include <cstdint>
#include <rpnx/detail/mutex_address_wait.hpp>
#elif defined(__APPLE__)
#include <os/lock.h>
#else
#include <mutex>
#endif

namespace rpnx
{
    /**
     * @brief Nonrecursive, process-local mutex occupying at most four bytes on Windows, macOS, and Linux.
     *
     * Uses WaitOnAddress on Windows 8 and later, private futexes on Linux, and
     * os_unfair_lock on macOS 10.12 and later. Windows and Linux use an atomic
     * word for state transitions and native address waits. Other platforms use
     * std::mutex and are exempt from the size limit.
     *
     * Provides the Lockable interface for standard lock guards and
     * std::condition_variable_any. Acquisition has acquire ordering and release
     * has release ordering. Scheduling is unfair; starvation is possible.
     * The owning thread must unlock the mutex. Recursive locking is invalid.
     * Destruction requires an unlocked mutex with no thread waiting to acquire it
     * and no future attempts to access it. An unlock that has released ownership
     * may finish its address-only wake after destruction. The mutex cannot be
     * copied or moved.
     */
    class thin_mutex
    {
#if defined(_WIN32) || defined(__linux__)
        /** @brief Ownership and wake obligation stored in the mutex word. */
        enum state : std::uint32_t { unlocked, locked, contended };

        // The Windows and Linux atomic ABI stores a lock-free uint32_t inline.
        static_assert(std::atomic< std::uint32_t >::is_always_lock_free);
        static_assert(sizeof(std::atomic< std::uint32_t >) == 4);
        alignas(4) std::atomic< std::uint32_t > m_state{state::unlocked};
#elif defined(__APPLE__)
        os_unfair_lock m_mutex = OS_UNFAIR_LOCK_INIT;
#else
        std::mutex m_mutex;
#endif

      public:
        /** @brief Constructs an unlocked mutex. */
        thin_mutex() = default;
        /** @brief Prevents copying synchronization ownership. */
        thin_mutex(thin_mutex const&) = delete;
        /** @brief Prevents assigning synchronization ownership. */
        thin_mutex& operator=(thin_mutex const&) = delete;

        /** @brief Acquires ownership, blocking until the mutex is available. */
        void lock()
        {
#if defined(_WIN32) || defined(__linux__)
            if (try_lock())
            {
                return;
            }
            // Every slow-path owner retains the wake obligation so that one
            // awakened waiter can pass ownership onward to remaining sleepers.
            while (m_state.exchange(state::contended, std::memory_order_acquire) != state::unlocked)
            {
                detail::wait_on_mutex_address(&m_state, state::contended);
            }
#elif defined(__APPLE__)
            os_unfair_lock_lock(&m_mutex);
#else
            m_mutex.lock();
#endif
        }

        /** @brief Attempts to acquire ownership without blocking. */
        bool try_lock()
        {
#if defined(_WIN32) || defined(__linux__)
            std::uint32_t expected = state::unlocked;
            return m_state.compare_exchange_strong(expected, state::locked, std::memory_order_acquire, std::memory_order_relaxed);
#elif defined(__APPLE__)
            return os_unfair_lock_trylock(&m_mutex);
#else
            return m_mutex.try_lock();
#endif
        }

        /** @brief Releases ownership and wakes a waiter when contention was recorded. */
        void unlock() noexcept
        {
#if defined(_WIN32) || defined(__linux__)
            void* address = &m_state;
            // Capture the wait key before releasing ownership. A new owner may
            // destroy this object as soon as the exchange has completed.
            if (m_state.exchange(state::unlocked, std::memory_order_release) == state::contended)
            {
                detail::wake_mutex_address(address);
            }
#elif defined(__APPLE__)
            os_unfair_lock_unlock(&m_mutex);
#else
            m_mutex.unlock();
#endif
        }
    };

#if defined(_WIN32) || defined(__APPLE__) || defined(__linux__)
    static_assert(sizeof(thin_mutex) <= 4, "thin_mutex must occupy at most four bytes on supported platforms");
#endif
} // namespace rpnx

#endif
