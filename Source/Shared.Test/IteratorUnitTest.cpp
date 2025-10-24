#include <gtest/gtest.h>

#include <arcana/iterators.h>

TEST(IteratorUnitTest, Iterator_WhenStaticForEmpty_NothingIsRun)
{
    int value = 0;
    arcana::static_for<0>([&](auto index)
    {
        value += 1;
    });

    EXPECT_EQ(0, value) << "Value shouldn't have been incremented";
}

TEST(IteratorUnitTest, Iterator_WhenStaticFor_NumberOfIterationsIsCorrect)
{
    int iterations = 0;
    arcana::static_for<10>([&](auto index)
    {
        iterations += 1;
    });

    EXPECT_EQ(10, iterations) << "Invalid number of iterations";
}

TEST(IteratorUnitTest, Iterator_WhenStaticForEachEmpty_NothingIsRun)
{
    int value = 0;
    arcana::static_foreach([&](auto index)
    {
        value += 1;
    });

    EXPECT_EQ(0, value) << "Value shouldn't have been incremented";
}

TEST(IteratorUnitTest, Iterator_WhenStaticForEach_NumberOfIterationsIsCorrect)
{
    int iterations = 0, value = 0;
    arcana::static_foreach([&](auto index)
    {
        iterations += 1;
        value += index;
    }, 0, 1, 2, 3, 4, 5);

    EXPECT_EQ(6, iterations) << "Invalid number of iterations";
    EXPECT_EQ(15, value) << "Invalid sum";
}

TEST(IteratorUnitTest, Iterator_WhenTupleForEachEmpty_NothingIsRun)
{
    int sum = 0;
    arcana::iterate_tuple(std::tuple<>{}, [&](auto value, auto index)
    {
        sum += 1;
    });

    EXPECT_EQ(0, sum) << "Value shouldn't have been incremented";
}

TEST(IteratorUnitTest, Iterator_WhenTupleForEach_NumberOfIterationsIsCorrect)
{
    int iterations = 0, sum = 0;
    arcana::iterate_tuple(std::make_tuple(0, 1, 2, 3, 4, 5), [&](auto value, auto index)
    {
        iterations += 1;
        sum += value;
    });

    EXPECT_EQ(6, iterations) << "Invalid number of iterations";
    EXPECT_EQ(15, sum) << "Invalid sum";
}
