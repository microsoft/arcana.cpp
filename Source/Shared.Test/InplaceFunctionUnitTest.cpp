#include <gtest/gtest.h>

#include <arcana/functional/inplace_function.h>

#include <memory>

TEST(InplaceFunctionUnitTest, MoveSemanticsInvalidatesMovedFunction)
{
    stdext::inplace_function<void()> source = [] {};
    stdext::inplace_function<void()> dest = std::move(source);
    EXPECT_TRUE(!source) << "Once the function is moved, I shouldn't be able to call it with a bunch of invalid data";
}

TEST(InplaceFunctionUnitTest, MovedFunctionGetsPropertyDestroyed)
{
    std::weak_ptr<int> weak;

    {
        const std::shared_ptr<int> value = std::make_shared<int>(10);
        weak = value;

        stdext::inplace_function<void()> source = [value] {};
        stdext::inplace_function<void()> dest = std::move(source);
    }

    EXPECT_TRUE(weak.expired());
}
