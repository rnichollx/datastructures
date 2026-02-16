// Copyright (c) 2026 Ryan P. Nicholl <rnicholl@protonmail.com>

#ifndef RPNXDATASTRUCTURES_TEST_UTILS_HPP
#define RPNXDATASTRUCTURES_TEST_UTILS_HPP

#include <stdexcept>

namespace rpnx::testing {

class copy_limit_exceeded : public std::runtime_error {
public:
    copy_limit_exceeded() : std::runtime_error("copy limit exceeded") {}
};

struct copy_thrower {
    int* copy_count = nullptr;
    int limit = 0;

    copy_thrower() = default;
    explicit copy_thrower(int* count, int max_copies) : copy_count(count), limit(max_copies) {}

    copy_thrower(const copy_thrower& other) : copy_count(other.copy_count), limit(other.limit) {
        if (copy_count) {
            if (*copy_count >= limit) {
                throw copy_limit_exceeded();
            }
            (*copy_count)++;
        }
    }

    copy_thrower& operator=(const copy_thrower& other) {
        if (this == &other) return *this;
        if (other.copy_count) {
            if (*other.copy_count >= other.limit) {
                throw copy_limit_exceeded();
            }
            (*other.copy_count)++;
        }
        copy_count = other.copy_count;
        limit = other.limit;
        return *this;
    }

    copy_thrower(copy_thrower&&) noexcept = default;
    copy_thrower& operator=(copy_thrower&&) noexcept = default;
    ~copy_thrower() = default;
};

} // namespace rpnx::testing

#endif // RPNXDATASTRUCTURES_TEST_UTILS_HPP
