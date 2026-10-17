// Copyright (c) 2026 Ryan P. Nicholl <rnicholl@protonmail.com>
// SPDX-License-Identifier: Apache-2.0
#ifndef RPNX_DETAIL_MUTEX_ADDRESS_WAIT_HPP
#define RPNX_DETAIL_MUTEX_ADDRESS_WAIT_HPP

#include <cstdint>
#include <exception>

#if defined(_WIN32)
#include <rpnx/detail/windows_synchronization.hpp>
#elif defined(__linux__)
#include <cerrno>
#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

namespace rpnx::detail
{
#if defined(_WIN32) || defined(__linux__)
    /**
     * @brief Blocks while a live, four-byte-aligned mutex word matches expected.
     * Returns on notification, a value mismatch, interruption, or a spurious wake.
     * The caller must retry acquisition. Unexpected operating-system errors terminate.
     */
    inline void wait_on_mutex_address(void* address, std::uint32_t expected) noexcept
    {
#if defined(_WIN32)
        if (!WaitOnAddress(address, &expected, sizeof(expected), INFINITE))
        {
            std::terminate();
        }
#else
        if (syscall(SYS_futex, address, FUTEX_WAIT_PRIVATE, expected, nullptr, nullptr, 0) == -1 && errno != EAGAIN && errno != EINTR)
        {
            std::terminate();
        }
#endif
    }

    /**
     * @brief Wakes one waiter using only an address as the operating-system wait key.
     * The word may have been destroyed or reused after ownership was released.
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
} // namespace rpnx::detail

#endif
