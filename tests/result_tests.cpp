// Copyright (c) 2026 Ryan P. Nicholl <rnicholl@protonmail.com>

#include "gtest/gtest.h"
#include "rpnx/result.hpp"

#include <memory>
#include <type_traits>
#include <utility>

static_assert(std::is_same_v< decltype(std::declval< rpnx::result< int > & >().get()), int & >);
static_assert(std::is_same_v< decltype(std::declval< rpnx::result< int > const & >().get()), int const & >);
static_assert(std::is_same_v< decltype(std::declval< rpnx::result< int > && >().get()), int && >);
static_assert(std::is_same_v< decltype(std::declval< rpnx::result< int > const && >().get()), int const && >);

static_assert(std::is_same_v< decltype(std::declval< rpnx::result< int > & >().value()), int & >);
static_assert(std::is_same_v< decltype(std::declval< rpnx::result< int > const & >().value()), int const & >);
static_assert(std::is_same_v< decltype(std::declval< rpnx::result< int > && >().value()), int && >);
static_assert(std::is_same_v< decltype(std::declval< rpnx::result< int > const && >().value()), int const && >);

TEST(result, void_set_exception_and_has_exception)
{
    rpnx::result< void > r;

    r.set_exception(std::make_exception_ptr(std::runtime_error("x")));

    EXPECT_FALSE(r.has_value());
    EXPECT_TRUE(r.has_error());
    EXPECT_TRUE(r.has_exception());
    EXPECT_TRUE(r.has_result());
    EXPECT_THROW(r.get(), std::runtime_error);
}

TEST(result, void_value_aliases_get)
{
    rpnx::result< void > r;

    EXPECT_THROW(r.value(), std::logic_error);

    r.set_value();
    EXPECT_NO_THROW(r.value());
}

TEST(result, void_set_error_clears_value)
{
    rpnx::result< void > r;

    r.set_value();
    EXPECT_TRUE(r.has_value());

    r.set_error(std::make_exception_ptr(std::runtime_error("x")));
    EXPECT_FALSE(r.has_value());
}

TEST(result, get_and_value_non_const_lvalue_are_references)
{
    rpnx::result< int > r(7);

    int & g = r.get();
    g = 11;
    EXPECT_EQ(r.value(), 11);

    int & v = r.value();
    v = 13;
    EXPECT_EQ(r.get(), 13);
}

TEST(result, get_and_value_const_lvalue_are_const_references)
{
    rpnx::result< int > const r(17);

    int const & g = r.get();
    int const & v = r.value();

    EXPECT_EQ(g, 17);
    EXPECT_EQ(v, 17);
}

TEST(result, get_and_value_on_temporaries_move_out)
{
    rpnx::result< std::unique_ptr< int > > rg(std::make_unique< int >(19));
    std::unique_ptr< int > g = std::move(rg).get();
    ASSERT_TRUE(g);
    EXPECT_EQ(*g, 19);

    rpnx::result< std::unique_ptr< int > > rv(std::make_unique< int >(23));
    std::unique_ptr< int > v = std::move(rv).value();
    ASSERT_TRUE(v);
    EXPECT_EQ(*v, 23);
}
