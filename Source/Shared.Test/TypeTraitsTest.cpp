#include <gtest/gtest.h>

#include <arcana/type_traits.h>

namespace
{
    enum TestCStyleEnum
    {
        Value1 = 1,
        Value2 = 3,
        Value3 = 157
    };

    enum class TestEnumClass : long long
    {
        Value1 = 2,
        Value2 = -1,
        Value3 = 256
    };
}

TEST(TypeTraitsTest, UnderlyingCast_WithCStyleEnum_ReturnsCorrectType)
{
    constexpr auto underlyingValue1 = arcana::underlying_cast(TestCStyleEnum::Value1);
    static_assert(std::is_same<std::decay_t<decltype(underlyingValue1)>, std::underlying_type_t<TestCStyleEnum>>::value, "");
}

TEST(TypeTraitsTest, UnderlyingCast_WithCStyleEnum_ReturnsCorrectValues)
{
    constexpr auto underlyingValue1 = arcana::underlying_cast(TestCStyleEnum::Value1);
    constexpr auto underlyingValue2 = arcana::underlying_cast(TestCStyleEnum::Value2);
    constexpr auto underlyingValue3 = arcana::underlying_cast(TestCStyleEnum::Value3);

    static_assert(underlyingValue1 == 1, "");
    static_assert(underlyingValue2 == 3, "");
    static_assert(underlyingValue3 == 157, "");
}

TEST(TypeTraitsTest, UnderlyingCast_WithEnumClass_ReturnsCorrectType)
{
    constexpr auto underlyingValue1 = arcana::underlying_cast(TestEnumClass::Value1);

    static_assert(std::is_same<std::decay_t<decltype(underlyingValue1)>, std::underlying_type_t<TestEnumClass>>::value, "");
}

TEST(TypeTraitsTest, UnderlyingCast_WithEnumClass_ReturnsCorrectValues)
{
    constexpr auto underlyingValue1 = arcana::underlying_cast(TestEnumClass::Value1);
    constexpr auto underlyingValue2 = arcana::underlying_cast(TestEnumClass::Value2);
    constexpr auto underlyingValue3 = arcana::underlying_cast(TestEnumClass::Value3);

    static_assert(underlyingValue1 == 2, "");
    static_assert(underlyingValue2 == -1, "");
    static_assert(underlyingValue3 == 256, "");
}

TEST(TypeTraitsTest, InvokeOptionalParameter_InvokesTheRightFunction)
{
    bool invokedvoid = false;
    auto funcvoid = [&] { invokedvoid = true; };

    bool invokedp = false;
    auto funcp = [&](int value)
    {
        EXPECT_EQ(10, value);
        invokedp = true;
    };

    arcana::invoke_with_optional_parameter(funcvoid, 10);
    EXPECT_TRUE(invokedvoid);

    arcana::invoke_with_optional_parameter(funcp, 10);
    EXPECT_TRUE(invokedp);
}

TEST(TypeTraitsTest, CountTrueConditionalExpressions)
{
    EXPECT_EQ(3u, (arcana::count_true<std::true_type, std::true_type, std::true_type>::value));
    EXPECT_EQ(0u, (arcana::count_true<>::value));
    EXPECT_EQ(1u, (arcana::count_true<std::true_type>::value));
    EXPECT_EQ(0u, (arcana::count_true<std::false_type>::value));
    EXPECT_EQ(2u, (arcana::count_true<std::true_type, std::false_type, std::true_type>::value));
}

TEST(TypeTraitsTest, FindFirstTrueConditionalExpressionIndex)
{
    EXPECT_EQ(0u, (arcana::find_first_index<std::true_type, std::true_type, std::true_type>::value));
    EXPECT_EQ(1u, (arcana::find_first_index<std::false_type, std::true_type, std::true_type>::value));
    EXPECT_EQ(1u, (arcana::find_first_index<std::false_type, std::true_type>::value));
    EXPECT_EQ(0u, (arcana::find_first_index<std::true_type, std::false_type>::value));
    EXPECT_EQ(2u, (arcana::find_first_index<std::false_type, std::false_type, std::true_type>::value));

    // test the end condition
    EXPECT_EQ(0u, (arcana::find_first_index<>::value));
    EXPECT_EQ(3u, (arcana::find_first_index<std::false_type, std::false_type, std::false_type>::value));
}
