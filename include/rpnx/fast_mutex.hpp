// Copyright (c) 2026 Ryan P. Nicholl <rnicholl@protonmail.com>
// SPDX-License-Identifier: Apache-2.0
#ifndef RPNX_FAST_MUTEX_HPP
#define RPNX_FAST_MUTEX_HPP

#if defined(_WIN32)
#include <rpnx/detail/windows_synchronization.hpp>
#elif defined(__APPLE__)
#include <os/lock.h>
#elif defined(__linux__)
#include <pthread.h>
#include <system_error>
#else
#include <mutex>
#endif

namespace rpnx
{
    /**
     * @brief Nonrecursive, process-local mutex using native primitives with minimal checking.
     *
     * Uses an exclusive SRW lock on Windows, os_unfair_lock on macOS, and a
     * PTHREAD_MUTEX_NORMAL mutex on Linux. Other platforms use std::mutex.
     * The Linux constructor reports initialization failures with std::system_error.
     * Native lock/unlock operations add no ownership checks or error handling.
     * Diagnostics provided by the operating system may still apply.
     *
     * Provides the Lockable interface for standard lock guards and
     * std::condition_variable_any. Acquisition has acquire ordering and release
     * has release ordering. Scheduling is unfair; starvation is possible.
     * The owning thread must unlock the mutex. Recursive locking is invalid.
     * Destruction requires an unlocked mutex with no thread waiting to acquire it
     * and no future attempts to access it.
     * The mutex cannot be copied or moved.
     */
    class fast_mutex
    {
#if defined(_WIN32)
        SRWLOCK m_mutex = SRWLOCK_INIT;
#elif defined(__APPLE__)
        os_unfair_lock m_mutex = OS_UNFAIR_LOCK_INIT;
#elif defined(__linux__)
        pthread_mutex_t m_mutex;
#else
        std::mutex m_mutex;
#endif

      public:
        /** @brief Constructs an unlocked mutex with native error checking disabled where configurable. */
#if defined(__linux__) && !defined(_WIN32) && !defined(__APPLE__)
        fast_mutex()
        {
            pthread_mutexattr_t attributes;
            // pthread functions return an API-defined integer error number.
            using pthread_error = int;
            pthread_error error = pthread_mutexattr_init(&attributes);
            if (error != 0)
            {
                throw std::system_error(error, std::generic_category(), "pthread_mutexattr_init");
            }
            error = pthread_mutexattr_settype(&attributes, PTHREAD_MUTEX_NORMAL);
            if (error != 0)
            {
                pthread_mutexattr_destroy(&attributes);
                throw std::system_error(error, std::generic_category(), "pthread_mutexattr_settype");
            }
            error = pthread_mutex_init(&m_mutex, &attributes);
            pthread_mutexattr_destroy(&attributes);
            if (error != 0)
            {
                throw std::system_error(error, std::generic_category(), "pthread_mutex_init");
            }
        }
#else
        fast_mutex() = default;
#endif

        /** @brief Releases the resources of an unlocked, unused mutex. */
        ~fast_mutex()
        {
#if defined(__linux__) && !defined(_WIN32) && !defined(__APPLE__)
            pthread_mutex_destroy(&m_mutex);
#endif
        }

        /** @brief Prevents copying synchronization ownership. */
        fast_mutex(fast_mutex const&) = delete;
        /** @brief Prevents assigning synchronization ownership. */
        fast_mutex& operator=(fast_mutex const&) = delete;

        /** @brief Acquires ownership, blocking until the mutex is available. */
        void lock()
        {
#if defined(_WIN32)
            AcquireSRWLockExclusive(&m_mutex);
#elif defined(__APPLE__)
            os_unfair_lock_lock(&m_mutex);
#elif defined(__linux__)
            pthread_mutex_lock(&m_mutex);
#else
            m_mutex.lock();
#endif
        }

        /** @brief Attempts to acquire ownership without blocking. */
        bool try_lock()
        {
#if defined(_WIN32)
            return TryAcquireSRWLockExclusive(&m_mutex) != 0;
#elif defined(__APPLE__)
            return os_unfair_lock_trylock(&m_mutex);
#elif defined(__linux__)
            return pthread_mutex_trylock(&m_mutex) == 0;
#else
            return m_mutex.try_lock();
#endif
        }

        /** @brief Releases ownership; the calling thread must own the mutex. */
        void unlock() noexcept
        {
#if defined(_WIN32)
            ReleaseSRWLockExclusive(&m_mutex);
#elif defined(__APPLE__)
            os_unfair_lock_unlock(&m_mutex);
#elif defined(__linux__)
            pthread_mutex_unlock(&m_mutex);
#else
            m_mutex.unlock();
#endif
        }
    };
} // namespace rpnx

#endif
