// Copyright (c) 2026 Ryan P. Nicholl <rnicholl@protonmail.com>
// Copyright 2024 Ryan P. Nicholl, rnicholl@protonmail.com

#ifndef RPNXDATASTRUCTURES_RESULT_HPP
#define RPNXDATASTRUCTURES_RESULT_HPP

#include <exception>
#include <optional>
#include <cassert>
#include <stdexcept>
#include <utility>

namespace rpnx
{
    /**
     * @brief A generic result class that can hold either a value or an exception.
     * @tparam T The type of the value stored in the result.
     */
    template < typename T >
    class result
    {
        std::optional< T > t;
        std::exception_ptr er;

      public:
        /**
         * @brief Constructs a result object with a value.
         * @param t The value to store in the result.
         */
        result(T t) : t(std::move(t))
        {
        }

        /**
         * @brief Constructs an empty result object.
         */
        result()
        {
        }

        /**
         * @brief Constructs a result object with an exception.
         * @param er The exception to store in the result.
         */
        result(std::exception_ptr er) : er(er)
        {
        }

        /**
         * @brief Tests if the result contains an exception and rethrows it if present.
         * @note Rethrows the stored exception if present.
         */
        void test() const
        {
            if (er)
            {
                std::rethrow_exception(er);
            }
        }

        /**
         * @brief Returns the stored value from an lvalue result.
         * @return A mutable reference to the stored value.
         * @note Rethrows the stored exception when the result contains an error.
         * @pre The result contains a value or an error.
         */
        T & get() &
        {
            if (er)
            {
                std::rethrow_exception(er);
            }

            assert(t.has_value());

            return t.value();
        }

        /** @brief Returns the stored value from a const lvalue result. @return An immutable reference to the stored value. @note Rethrows the stored exception when the result contains an error. @pre The result contains a value or an error. */
        T const & get() const &
        {
            if (er)
            {
                std::rethrow_exception(er);
            }

            assert(t.has_value());

            return t.value();
        }

        /**
         * @brief Returns the stored value from an rvalue result.
         * @return An rvalue reference to the stored value.
         * @note Rethrows the stored exception when the result contains an error.
         * @pre The result contains a value or an error.
         */
        T && get() &&
        {
            if (er)
            {
                std::rethrow_exception(er);
            }

            assert(t.has_value());

            return std::move(t.value());
        }

        /** @brief Returns the stored value from a const rvalue result. @return An immutable rvalue reference to the stored value. @note Rethrows the stored exception when the result contains an error. @pre The result contains a value or an error. */
        T const && get() const &&
        {
            if (er)
            {
                std::rethrow_exception(er);
            }

            assert(t.has_value());

            return std::move(t.value());
        }

        /** @brief Alias for `get()`. @return A mutable reference to the stored value. */
        T & value() &
        {
            return get();
        }

        /** @brief Alias for `get()`. @return An immutable reference to the stored value. */
        T const & value() const &
        {
            return get();
        }

        /** @brief Alias for rvalue-qualified `get()`. @return An rvalue reference to the stored value. */
        T && value() &&
        {
            return std::move(*this).get();
        }

        /** @brief Alias for const rvalue-qualified `get()`. @return An immutable rvalue reference to the stored value. */
        T const && value() const &&
        {
            return std::move(*this).get();
        }

        /**
         * @brief Sets the value of the result.
         * @param t The value to set.
         */
        void set_value(T t)
        {
            this->er = nullptr;
            this->t = std::move(t);
        }

        /**
         * @brief Sets the exception of the result.
         * @param er The exception to set.
         */
        void set_error(std::exception_ptr er)
        {
            this->t.reset();
            this->er = er;
        }

        /** @brief Alias for `set_error()`. @param er Exception to store. */
        void set_exception(std::exception_ptr er)
        {
            set_error(er);
        }

        /**
         * @brief Checks if the result has a value or an exception.
         * @return True if the result has a value or an exception, false otherwise.
         */
        bool has_result() const
        {
            return t.has_value() || er != nullptr;
        }

        /**
         * @brief Checks if the result has a value.
         * @return True if the result has a value, false otherwise.
         */
        bool has_value() const
        {
            return t.has_value();
        }

        /**
         * @brief Checks if the result has an exception.
         * @return True if the result has an exception, false otherwise.
         */
        bool has_error() const
        {
            return er != nullptr;
        }

        /** @brief Alias for `has_error()`. @return `true` if an exception is stored. */
        bool has_exception() const
        {
            return has_error();
        }

        /**
         * @brief Gets the stored exception.
         * @return The stored exception.
         */
        std::exception_ptr get_error() const
        {
            return er;
        }

        /**
         * @brief Implicit conversion to bool.
         * @return True if the result has a value or an exception, false otherwise.
         */
        inline operator bool() const
        {
            return has_result();
        }
    };

    /**
     * @brief Specialization of the result class for void type.
     */
    template <>
    class result< void >
    {
        bool t = false;
        std::exception_ptr er;

      public:
        /**
         * @brief Constructs an empty result object.
         */
        result()
        {
        }

        /**
         * @brief Constructs a result object with an exception.
         * @param er The exception to store in the result.
         */
        result(std::exception_ptr er) : er(er)
        {
        }

        /**
         * @brief Tests if the result contains an exception and rethrows it if present.
         * @note Rethrows the stored exception if present.
         */
        void test() const
        {
            if (er)
            {
                std::rethrow_exception(er);
            }
        }

        /**
         * @brief Gets the stored value (void).
         * @note Rethrows the stored exception if present.
         * @throws std::logic_error if no value is stored.
         */
        void get() const
        {
            if (er)
                std::rethrow_exception(er);
            if (!t)
                throw std::logic_error("No value");
            return;
        }

        /** @brief Alias for `get()`. @note Rethrows the stored exception if present. @throws std::logic_error when incomplete. */
        void value() const
        {
            get();
        }

        /**
         * @brief Sets the value of the result (void).
         */
        void set_value()
        {
            this->er = nullptr;
            this->t = true;
        }

        /**
         * @brief Sets the exception of the result.
         * @param er The exception to set.
         */
        void set_error(std::exception_ptr er)
        {
            this->t = false;
            this->er = er;
        }

        /** @brief Alias for `set_error()`. @param er Exception to store. */
        void set_exception(std::exception_ptr er)
        {
            set_error(er);
        }

        /**
         * @brief Checks if the result has a value (void).
         * @return True if the result has a value, false otherwise.
         */
        bool has_value() const
        {
            return t;
        }

        /**
         * @brief Checks if the result has an exception.
         * @return True if the result has an exception, false otherwise.
         */
        bool has_error() const
        {
            return er != nullptr;
        }

        /** @brief Alias for `has_error()`. @return `true` if an exception is stored. */
        bool has_exception() const
        {
            return has_error();
        }

        /**
         * @brief Gets the stored exception.
         * @return The stored exception.
         */
        std::exception_ptr get_error() const
        {
            return er;
        }

        /**
         * @brief Checks if the result has a value (void) or an exception.
         * @return True if the result has a value or an exception, false otherwise.
         */
        inline bool has_result() const
        {
            return has_value() || has_error();
        }

        /**
         * @brief Implicit conversion to bool.
         * @return True if the result has a value or an exception, false otherwise.
         */
        inline operator bool() const
        {
            return has_result();
        }
    };
} // namespace rpnx

#endif // RPNXDATASTRUCTURES_RESULT_HPP
