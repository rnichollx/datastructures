// Copyright (c) 2026 Ryan P. Nicholl <rnicholl@protonmail.com>
// SPDX-License-Identifier: Apache-2.0
#ifndef RPNX_DETAIL_MUTEX_ADDRESS_WAIT_HPP
#define RPNX_DETAIL_MUTEX_ADDRESS_WAIT_HPP

#include <array>
#include <atomic>
#include <bit>
#include <cstdint>
#include <exception>
#include <limits>
#include <type_traits>

#if defined(_WIN32)
#include <rpnx/detail/windows_synchronization.hpp>
#elif defined(__linux__)
#include <cerrno>
#include <linux/futex.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>
#elif defined(__APPLE__)
#include <cerrno>
#include <os/os_sync_wait_on_address.h>
#endif

namespace rpnx::detail
{
#if defined(_WIN32) || defined(__linux__) || defined(__APPLE__)
    /**
     * @brief Parks while a live, four-byte-aligned mutex integer equals expected.
     * Returns on notification, mismatch, interruption, or a spurious wake.
     * The caller must retry acquisition. Unexpected wait errors terminate.
     */
    inline void wait_on_mutex_address(std::atomic< std::uint32_t >* address, std::uint32_t expected) noexcept
    {
#if defined(_WIN32)
        if (!WaitOnAddress(address, &expected, sizeof(expected), INFINITE))
        {
            std::terminate();
        }
#elif defined(__linux__)
        if (syscall(SYS_futex, address, FUTEX_WAIT_PRIVATE, expected, nullptr, nullptr, 0) == -1 && errno != EAGAIN && errno != EINTR)
        {
            std::terminate();
        }
#elif defined(__APPLE__)
        if (os_sync_wait_on_address(address, expected, sizeof(expected), OS_SYNC_WAIT_ON_ADDRESS_NONE) == -1 && errno != EINTR && errno != ENOMEM && errno != EFAULT)
        {
            std::terminate();
        }
#endif
    }

    /**
     * @brief Parks while a live, eight-byte-aligned mutex integer equals expected.
     * Returns on notification, mismatch, interruption, or a spurious wake.
     * Linux requires futex_waitv (5.16+). Unexpected wait errors terminate.
     */
    inline void wait_on_mutex_address(std::atomic< std::uint64_t >* address, std::uint64_t expected) noexcept
    {
#if defined(_WIN32)
        if (!WaitOnAddress(address, &expected, sizeof(expected), INFINITE))
        {
            std::terminate();
        }
#elif defined(__linux__)
        // Waiting on both halves detects mismatches across the complete integer.
        // All wake operations use the first address, which is in this vector.
        std::uintptr_t bits = reinterpret_cast< std::uintptr_t >(address);
        std::array< std::uint32_t, 2 > halves = std::bit_cast< std::array< std::uint32_t, 2 > >(expected);
        futex_waitv waiters[2] = {{halves[0], bits, FUTEX_32 | FUTEX_PRIVATE_FLAG, 0}, {halves[1], bits + 4, FUTEX_32 | FUTEX_PRIVATE_FLAG, 0}};
        if (syscall(SYS_futex_waitv, waiters, 2u, 0u, nullptr, CLOCK_MONOTONIC) == -1 && errno != EAGAIN && errno != EINTR)
        {
            std::terminate();
        }
#elif defined(__APPLE__)
        if (os_sync_wait_on_address(address, expected, sizeof(expected), OS_SYNC_WAIT_ON_ADDRESS_NONE) == -1 && errno != EINTR && errno != ENOMEM && errno != EFAULT)
        {
            std::terminate();
        }
#endif
    }

#if defined(_WIN32) || defined(__linux__)
    /**
     * @brief Wakes one waiter using only an address as the operating-system wait key.
     * The integer may have been destroyed or reused after ownership was released.
     * This function must never read or modify storage at the supplied address.
     */
    inline void wake_mutex_address(void* address) noexcept
    {
#if defined(_WIN32)
        WakeByAddressSingle(address);
#else
        // A late wake may target reclaimed storage. It needs no value access,
        // and no waiters remain if that storage has already been reclaimed.
        syscall(SYS_futex, address, FUTEX_WAKE_PRIVATE, 1, nullptr, nullptr, 0);
#endif
    }
#endif

    /**
     * @brief Broadcasts using an address-only wait key for the specified integer width.
     * The object at address may already have been destroyed or reused. Notification
     * never reads or modifies its storage.
     */
    template < typename Int >
    void wake_all_mutex_address(void* address) noexcept
    {
        static_assert(std::is_same_v< Int, std::uint32_t > || std::is_same_v< Int, std::uint64_t >);
#if defined(_WIN32)
        WakeByAddressAll(address);
#elif defined(__linux__)
        syscall(SYS_futex, address, FUTEX_WAKE_PRIVATE, std::numeric_limits< std::int32_t >::max(), nullptr, nullptr, 0);
#elif defined(__APPLE__)
        os_sync_wake_by_address_all(address, sizeof(Int), OS_SYNC_WAKE_BY_ADDRESS_NONE);
#endif
    }
#endif
} // namespace rpnx::detail

#endif
