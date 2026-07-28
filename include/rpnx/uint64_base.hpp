// Copyright 2025 Ryan P. Nicholl, rnicholl@protonmail.com

#ifndef RPNX_DATASTRUCTURES_RPNX_UINT64_BASE_HPP
#define RPNX_DATASTRUCTURES_RPNX_UINT64_BASE_HPP

#include <cstdint>

namespace rpnx
{
    /**
     * @brief CRTP base for strongly typed unsigned 64-bit integer wrappers.
     *
     * Derived types inherit ordinary unsigned arithmetic, comparison, bitwise,
     * and serialization operations while remaining distinct from one another.
     * Arithmetic follows `std::uint64_t` modulo-2^64 semantics.
     *
     * @tparam derived_t Complete wrapper type derived from this specialization.
     */
    template < typename derived_t >
    class uint64_base
    {
      private:
        std::uint64_t value_;

      public:
        // Default constructor
        uint64_base() : value_(0)
        {
        }

        /** @brief Constructs from an unsigned 64-bit value. @param val Initial value. */
        explicit uint64_base(std::uint64_t val) : value_(val)
        {
        }

        /** @brief Copies a value from the derived wrapper type. @param other Value to copy. */
        uint64_base(const derived_t& other) : value_(other.value_)
        {
        }

        /** @brief Assigns a value from the derived wrapper type. @param other Value to copy. @return Reference to the derived object. */
        derived_t& operator=(const derived_t& other)
        {
            if (this != &other)
            {
                value_ = other.value_;
            }
            return static_cast< derived_t& >(*this);
        }

        /** @brief Converts to the underlying unsigned integer. @return Stored value. */
        operator std::uint64_t() const
        {
            return value_;
        }

        /** @brief Adds two wrapped values. @param other Right operand. @return Sum modulo 2^64. */
        derived_t operator+(const derived_t& other) const
        {
            return derived_t(value_ + other.value_);
        }

        /** @brief Subtracts two wrapped values. @param other Right operand. @return Difference modulo 2^64. */
        derived_t operator-(const derived_t& other) const
        {
            return derived_t(value_ - other.value_);
        }

        /** @brief Multiplies two wrapped values. @param other Right operand. @return Product modulo 2^64. */
        derived_t operator*(const derived_t& other) const
        {
            return derived_t(value_ * other.value_);
        }

        /** @brief Divides two wrapped values. @param other Divisor. @return Integer quotient. @pre `other` is nonzero. */
        derived_t operator/(const derived_t& other) const
        {
            // You might want to add error handling for division by zero
            return derived_t(value_ / other.value_);
        }

        /** @brief Computes the remainder of two wrapped values. @param other Divisor. @return Remainder. @pre `other` is nonzero. */
        derived_t operator%(const derived_t& other) const
        {
            return derived_t(value_ % other.value_);
        }

        /** @brief Adds and assigns. @param other Right operand. @return Reference to the derived object. */
        derived_t& operator+=(const derived_t& other)
        {
            value_ += other.value_;
            return static_cast< derived_t& >(*this);
        }

        /** @brief Subtracts and assigns. @param other Right operand. @return Reference to the derived object. */
        derived_t& operator-=(const derived_t& other)
        {
            value_ -= other.value_;
            return static_cast< derived_t& >(*this);
        }

        /** @brief Multiplies and assigns. @param other Right operand. @return Reference to the derived object. */
        derived_t& operator*=(const derived_t& other)
        {
            value_ *= other.value_;
            return static_cast< derived_t& >(*this);
        }

        /** @brief Divides and assigns. @param other Nonzero divisor. @return Reference to the derived object. */
        derived_t& operator/=(const derived_t& other)
        {
            value_ /= other.value_;
            return static_cast< derived_t& >(*this);
        }

        /** @brief Computes a remainder and assigns. @param other Nonzero divisor. @return Reference to the derived object. */
        derived_t& operator%=(const derived_t& other)
        {
            value_ %= other.value_;
            return static_cast< derived_t& >(*this);
        }

        /** @brief Increments the stored value. @return Reference to the derived object after incrementing. */
        derived_t& operator++()
        { // Pre-increment
            ++value_;
            return static_cast< derived_t& >(*this);
        }

        /** @brief Increments the stored value. @return Derived value before incrementing. */
        derived_t operator++(int)
        { // Post-increment
            derived_t temp = static_cast< derived_t& >(*this);
            ++value_;
            return temp;
        }

        /** @brief Decrements the stored value. @return Reference to the derived object after decrementing. */
        derived_t& operator--()
        { // Pre-decrement
            --value_;
            return static_cast< derived_t& >(*this);
        }

        /** @brief Decrements the stored value. @return Derived value before decrementing. */
        derived_t operator--(int)
        { // Post-decrement
            derived_t temp = static_cast< derived_t& >(*this);
            --value_;
            return temp;
        }

        /** @brief Compares with a derived value for equality. @param other Value to compare. @return Comparison result. */
        bool operator==(const derived_t& other) const
        {
            return value_ == other.value_;
        }

        /** @brief Compares with a base value for equality. @param other Value to compare. @return Comparison result. */
        bool operator==( uint64_base<derived_t> const& other) const
        {
            return value_ == other.value_;
        }

        /** @brief Compares with a derived value for inequality. @param other Value to compare. @return Comparison result. */
        bool operator!=(const derived_t& other) const
        {
            return value_ != other.value_;
        }

        /** @brief Compares with a base value for inequality. @param other Value to compare. @return Comparison result. */
        bool operator!=(uint64_base<derived_t> const& other) const
        {
            return value_ != other.value_;
        }

        /** @brief Tests whether this value is less. @param other Value to compare. @return Comparison result. */
        bool operator<(const derived_t& other) const
        {
            return value_ < other.value_;
        }

        /** @brief Tests whether this value is greater. @param other Value to compare. @return Comparison result. */
        bool operator>(const derived_t& other) const
        {
            return value_ > other.value_;
        }

        /** @brief Tests whether this value is less than or equal. @param other Value to compare. @return Comparison result. */
        bool operator<=(const derived_t& other) const
        {
            return value_ <= other.value_;
        }

        /** @brief Tests whether this value is greater than or equal. @param other Value to compare. @return Comparison result. */
        bool operator>=(const derived_t& other) const
        {
            return value_ >= other.value_;
        }

        /** @brief Computes bitwise AND. @param other Right operand. @return Derived result. */
        derived_t operator&(const derived_t& other) const
        {
            return derived_t(value_ & other.value_);
        }

        /** @brief Computes bitwise OR. @param other Right operand. @return Derived result. */
        derived_t operator|(const derived_t& other) const
        {
            return derived_t(value_ | other.value_);
        }

        /** @brief Computes bitwise exclusive OR. @param other Right operand. @return Derived result. */
        derived_t operator^(const derived_t& other) const
        {
            return derived_t(value_ ^ other.value_);
        }

        /** @brief Computes bitwise complement. @return Derived result. */
        derived_t operator~() const
        {
            return derived_t(~value_);
        }

        /** @brief Shifts left. @param shift Bit count in `[0, 63]`. @return Shifted derived result. */
        derived_t operator<<(int shift) const
        {
            return derived_t(value_ << shift);
        }

        /** @brief Shifts right. @param shift Bit count in `[0, 63]`. @return Shifted derived result. */
        derived_t operator>>(int shift) const
        {
            return derived_t(value_ >> shift);
        }

        /** @brief Computes bitwise AND and assigns. @param other Right operand. @return Reference to the derived object. */
        derived_t& operator&=(const derived_t& other)
        {
            value_ &= other.value_;
            return static_cast< derived_t& >(*this);
        }

        /** @brief Computes bitwise OR and assigns. @param other Right operand. @return Reference to the derived object. */
        derived_t& operator|=(const derived_t& other)
        {
            value_ |= other.value_;
            return static_cast< derived_t& >(*this);
        }

        /** @brief Computes bitwise exclusive OR and assigns. @param other Right operand. @return Reference to the derived object. */
        derived_t& operator^=(const derived_t& other)
        {
            value_ ^= other.value_;
            return static_cast< derived_t& >(*this);
        }

        /** @brief Shifts left and assigns. @param shift Bit count in `[0, 63]`. @return Reference to the derived object. */
        derived_t& operator<<=(int shift)
        {
            value_ <<= shift;
            return static_cast< derived_t& >(*this);
        }

        /** @brief Shifts right and assigns. @param shift Bit count in `[0, 63]`. @return Reference to the derived object. */
        derived_t& operator>>=(int shift)
        {
            value_ >>= shift;
            return static_cast< derived_t& >(*this);
        }

        /** @brief Exposes the stored integer to serialization code. @return Immutable reference to the stored value. */
        std::uint64_t const& serialize_interface() const
        {
            return value_;
        }

        /** @brief Exposes the stored integer to deserialization code. @return Mutable reference to the stored value. */
        std::uint64_t& deserialize_interface()
        {
            return value_;
        }

        /** @brief Returns the serialization fields as an immutable tuple. @return Tuple containing the stored integer. */
        auto tie() const
        {
            return std::tie(value_);
        }
        /** @brief Returns the serialization fields as a mutable tuple. @return Tuple containing the stored integer. */
        auto tie()
        {
            return std::tie(value_);
        }

        /** @brief Returns serialization field names in tuple order. @return A single-element list containing `"value"`. */
        static auto constexpr strings()
        {
            return std::vector< std::string >{ "value" };
        }


    };
} // namespace rpnx

/**
 * @brief Declares a strongly typed unsigned 64-bit integer wrapper.
 *
 * The generated structure derives from `rpnx::uint64_base` and cannot
 * implicitly interconvert with wrappers declared under other names.
 *
 * @param name Name of the structure to declare.
 */
#define RPNX_UNIQUE_U64(name) \
    struct name : public rpnx::uint64_base< name > \
    { \
        using rpnx::uint64_base< name >::uint64_base; \
        static constexpr auto class_name() { return #name; } \
        using rpnx::uint64_base< name >::operator=; \
        name() = default; \
        using rpnx::uint64_base< name >::operator==; \
        using rpnx::uint64_base< name >::operator!=; \
        using rpnx::uint64_base< name >::operator<; \
        using rpnx::uint64_base< name >::operator>; \
        using rpnx::uint64_base< name >::operator<=; \
        using rpnx::uint64_base< name >::operator>=; \
    };

#endif // UINT64_BASE_HPP
