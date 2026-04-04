// Copyright (c) 2026 Ryan P. Nicholl <rnicholl@protonmail.com>

#include <gtest/gtest.h>
#include "rpnx/variant.hpp"
#include <string>
#include <vector>

TEST(variant, default_constructor)
{
    rpnx::variant<int, std::string> v;
    ASSERT_EQ(v.index(), 0);
    ASSERT_EQ(v.get_as<int>(), 0);
}

TEST(variant, value_constructor)
{
    rpnx::variant<int, std::string> v1(5);
    ASSERT_EQ(v1.index(), 0);
    ASSERT_EQ(v1.get_as<int>(), 5);

    rpnx::variant<int, std::string> v2(std::string("hello"));
    ASSERT_EQ(v2.index(), 1);
    ASSERT_EQ(v2.get_as<std::string>(), "hello");

    rpnx::variant<int, std::string> v3 = 10;
    ASSERT_EQ(v3.index(), 0);
    ASSERT_EQ(v3.get_as<int>(), 10);
}

TEST(variant, copy_move_constructor)
{
    rpnx::variant<int, std::string> v1(std::string("test"));
    rpnx::variant<int, std::string> v2(v1);
    ASSERT_EQ(v2.index(), 1);
    ASSERT_EQ(v2.get_as<std::string>(), "test");

    rpnx::variant<int, std::string> v3(std::move(v1));
    ASSERT_EQ(v3.index(), 1);
    ASSERT_EQ(v3.get_as<std::string>(), "test");
}

TEST(variant, assignment)
{
    rpnx::variant<int, std::string> v;
    v = 10;
    ASSERT_EQ(v.index(), 0);
    ASSERT_EQ(v.get_as<int>(), 10);

    v = std::string("world");
    ASSERT_EQ(v.index(), 1);
    ASSERT_EQ(v.get_as<std::string>(), "world");

    rpnx::variant<int, std::string> v2;
    v2 = v;
    ASSERT_EQ(v2.index(), 1);
    ASSERT_EQ(v2.get_as<std::string>(), "world");

    rpnx::variant<int, std::string> v3;
    v3 = std::move(v2);
    ASSERT_EQ(v3.index(), 1);
    ASSERT_EQ(v3.get_as<std::string>(), "world");
}

TEST(variant, accessors)
{
    rpnx::variant<int, std::string> v(42);

    ASSERT_EQ(v.get_as<int>(), 42);
    ASSERT_EQ(v.as<int>(), 42);
    ASSERT_EQ(v.unwrap<int>(), 42);
    ASSERT_EQ(v.static_cast_as<int>(), 42);
    ASSERT_EQ(v.get_n<0>(), 42);

    ASSERT_THROW(v.get_as<std::string>(), std::bad_variant_access);
    ASSERT_THROW(v.get_n<1>(), std::bad_variant_access);

    ASSERT_NE(v.cast_ptr<int>(), nullptr);
    ASSERT_EQ(*v.cast_ptr<int>(), 42);
    ASSERT_EQ(v.cast_ptr<std::string>(), nullptr);

    v = std::string("test");
    ASSERT_EQ(v.get_as<std::string>(), "test");
    ASSERT_EQ(v.get_n<1>(), "test");
    ASSERT_EQ(v.static_cast_as<std::string>(), "test");
}

TEST(variant, type_info)
{
    rpnx::variant<int, std::string> v(42);
    ASSERT_EQ(v.type(), typeid(int));
    ASSERT_EQ(v.type_index(), std::type_index(typeid(int)));
    ASSERT_TRUE(v.type_is<int>());
    ASSERT_FALSE(v.type_is<std::string>());

    v = std::string("hello");
    ASSERT_EQ(v.type(), typeid(std::string));
    ASSERT_TRUE(v.type_is<std::string>());
}

TEST(variant, comparisons)
{
    rpnx::variant<int, std::string> v1(10);
    rpnx::variant<int, std::string> v2(20);
    rpnx::variant<int, std::string> v3(10);
    rpnx::variant<int, std::string> v4(std::string("abc"));

    ASSERT_TRUE(v1 == v3);
    ASSERT_FALSE(v1 == v2);
    ASSERT_TRUE(v1 != v2);
    ASSERT_TRUE(v1 < v2);
    ASSERT_TRUE(v1 < v4); // different indices, compared by index
    ASSERT_FALSE(v4 < v1);

    ASSERT_EQ(v1 <=> v3, std::strong_ordering::equal);
    ASSERT_EQ(v1 <=> v2, std::strong_ordering::less);
}

TEST(variant, visitors)
{
    rpnx::variant<int, std::string> v(42);

    int result = rpnx::apply_visitor<int>(v, [](auto&& arg) -> int {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, int>) return arg;
        else return 0;
    });
    ASSERT_EQ(result, 42);

    int member_result = v.apply_visitor<int>([](auto&& arg) -> int {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, int>) return arg + 1;
        else return 0;
    });
    ASSERT_EQ(member_result, 43);

    rpnx::variant<int, std::string> cv(std::string("hello"));
    std::size_t member_const_result = cv.apply_visitor<std::size_t>([](auto&& arg) -> std::size_t {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, std::string>) return arg.size();
        else return 0;
    });
    ASSERT_EQ(member_const_result, 5);

    bool matched = v.match<int>([](int val) {
        ASSERT_EQ(val, 42);
    });
    ASSERT_TRUE(matched);

    matched = v.match<std::string>([](const std::string& s) {
        GTEST_FAIL() << "Should not match std::string";
    });
    ASSERT_FALSE(matched);

    bool tested = v.test<int>([](int val) { return val > 40; });
    ASSERT_TRUE(tested);

    tested = v.test<int>([](int val) { return val < 40; });
    ASSERT_FALSE(tested);
}

TEST(variant, complex_types)
{
    rpnx::variant<std::vector<int>, std::string> v(std::vector<int>{1, 2, 3});
    ASSERT_EQ(v.index(), 0);
    ASSERT_EQ(v.get_as<std::vector<int>>().size(), 3);

    v = std::string("a long string that hopefully triggers heap allocation if it wasn't already");
    ASSERT_EQ(v.index(), 1);
    ASSERT_EQ(v.get_as<std::string>()[0], 'a');
}

TEST(variant, conversion_constructor)
{
    rpnx::variant<int, double> v1(1.5);
    rpnx::variant<int, double, std::string> v2(v1);
    ASSERT_EQ(v2.index(), 1);
    ASSERT_EQ(v2.get_as<double>(), 1.5);
}

struct copy_move_tracker
{
    int value = 0;
    int* copy_count = nullptr;
    int* move_count = nullptr;

    copy_move_tracker() = default;

    copy_move_tracker(int value, int* copy_count, int* move_count) : value(value), copy_count(copy_count), move_count(move_count)
    {
    }

    copy_move_tracker(const copy_move_tracker& other) : value(other.value), copy_count(other.copy_count), move_count(other.move_count)
    {
        if (copy_count)
        {
            ++(*copy_count);
        }
    }

    copy_move_tracker(copy_move_tracker&& other) noexcept : value(other.value), copy_count(other.copy_count), move_count(other.move_count)
    {
        if (move_count)
        {
            ++(*move_count);
        }
    }

    copy_move_tracker& operator=(const copy_move_tracker& other)
    {
        value = other.value;
        copy_count = other.copy_count;
        move_count = other.move_count;
        if (copy_count)
        {
            ++(*copy_count);
        }
        return *this;
    }

    copy_move_tracker& operator=(copy_move_tracker&& other) noexcept
    {
        value = other.value;
        copy_count = other.copy_count;
        move_count = other.move_count;
        if (move_count)
        {
            ++(*move_count);
        }
        return *this;
    }

    auto operator<=>(const copy_move_tracker&) const = default;
};

TEST(variant, rvalue_value_constructor_and_assignment_move_the_payload)
{
    int copy_count = 0;
    int move_count = 0;

    rpnx::variant<int, copy_move_tracker> v(copy_move_tracker(7, &copy_count, &move_count));
    ASSERT_EQ(v.index(), 1);
    ASSERT_EQ(v.get_as<copy_move_tracker>().value, 7);
    ASSERT_EQ(copy_count, 0);
    ASSERT_EQ(move_count, 1);

    copy_count = 0;
    move_count = 0;

    rpnx::variant<int, copy_move_tracker> v2(0);
    v2 = copy_move_tracker(9, &copy_count, &move_count);
    ASSERT_EQ(v2.index(), 1);
    ASSERT_EQ(v2.get_as<copy_move_tracker>().value, 9);
    ASSERT_EQ(copy_count, 0);
    ASSERT_EQ(move_count, 1);
}

TEST(variant, converting_value_constructor_and_assignment_use_the_selected_alternative_type)
{
    rpnx::variant<std::string, int> v("hello");
    ASSERT_EQ(v.index(), 0);
    ASSERT_EQ(v.get_as<std::string>(), "hello");

    rpnx::variant<std::string, int> v2(0);
    v2 = "world";
    ASSERT_EQ(v2.index(), 0);
    ASSERT_EQ(v2.get_as<std::string>(), "world");
}

TEST(variant, exceptions)
{
    rpnx::variant<int, std::string> v(42);

    // Accessing wrong type
    ASSERT_THROW(v.get_as<std::string>(), std::bad_variant_access);
    ASSERT_THROW(v.as<std::string>(), std::bad_variant_access);
    ASSERT_THROW(v.unwrap<std::string>(), std::bad_variant_access);
    ASSERT_THROW(v.get_n<1>(), std::bad_variant_access);

    // Invalid variant (after reset)
    v.reset();
    ASSERT_THROW(v.index(), std::bad_variant_access);
    ASSERT_THROW(v.type(), std::bad_variant_access);
    ASSERT_THROW(v.get_as<int>(), std::bad_variant_access);
    ASSERT_THROW(v.get_n<0>(), std::bad_variant_access);
}

struct throwing_type
{
    throwing_type() { throw std::runtime_error("constructor throw"); }
    throwing_type(const throwing_type&) { throw std::runtime_error("copy constructor throw"); }
    throwing_type(throwing_type&&) { throw std::runtime_error("move constructor throw"); }
    auto operator<=>(const throwing_type&) const = default;
};

TEST(variant, constructor_exceptions)
{
    // Default constructor of first type throws
    ASSERT_THROW((rpnx::variant<throwing_type, int>()), std::runtime_error);

    rpnx::variant<int, throwing_type> v(42);
    // Assignment to a type whose constructor throws
    ASSERT_THROW(v = throwing_type(), std::runtime_error);
    // Ensure it remains in the old state (strong exception guarantee)
    ASSERT_EQ(v.get_as<int>(), 42);
}

struct int_only_visitor {
    void operator()(int) {}
};

TEST(variant, visitor_exceptions)
{
    rpnx::variant<int, std::string> v(42);

    // Visitor that throws
    ASSERT_THROW(
        rpnx::apply_visitor<void>(v, [](auto&&) { throw std::runtime_error("visitor throw"); }),
        std::runtime_error
    );

    // apply_visitor_checked with missing overload
    // v contains int, and int_only_visitor HAS int overload, so it should NOT throw
    ASSERT_NO_THROW(rpnx::apply_visitor_checked<void>(v, int_only_visitor{}));
    ASSERT_NO_THROW(v.apply_visitor_checked<void>(int_only_visitor{}));

    v = std::string("hello");
    // v contains string, and int_only_visitor LACKS string overload, so it SHOULD throw
    ASSERT_THROW(rpnx::apply_visitor_checked<void>(v, int_only_visitor{}), std::bad_variant_access);
    ASSERT_THROW(v.apply_visitor_checked<void>(int_only_visitor{}), std::bad_variant_access);

    int member_try = v.try_apply_visitor<int>([](int x) { return x + 1; });
    ASSERT_EQ(member_try, 0);
}

TEST(variant, checked_visitor_exception)
{
    rpnx::variant<int, std::string> v(std::string("hello"));
    // int_only_visitor cannot be called with std::string
    ASSERT_THROW(rpnx::apply_visitor_checked<void>(v, int_only_visitor{}), std::bad_variant_access);
}
