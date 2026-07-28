// Copyright (c) 2026 Ryan P. Nicholl <rnicholl@protonmail.com>

#ifndef RPNXDATASTRUCTURES_MEMORY_HPP
#define RPNXDATASTRUCTURES_MEMORY_HPP

#include <cstddef>

/** @brief Containers, iterator adapters, callable wrappers, and value utilities. */
namespace rpnx
{
    /**
     * @brief Marks a memory region as poisoned when instrumentation is enabled.
     *
     * The default implementation is a no-op. Instrumented builds may replace
     * or augment it to make invalid memory access easier to diagnose.
     *
     * @param ptr Start of the memory region; may be null when `size` is zero.
     * @param size Region size in bytes.
     */
    inline void poison_region(void* ptr, std::size_t size)
    {
    }

} // namespace rpnx

#endif // RPNXDATASTRUCTURES_MEMORY_HPP
