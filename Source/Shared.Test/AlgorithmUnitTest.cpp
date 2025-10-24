
#include <algorithm>
#include <iterator>
#include <chrono>

#include <gtest/gtest.h>

#include <arcana/algorithm.h>

using namespace std::chrono_literals;

namespace
{
    template<class T>
    void AssertComputationOfSetOfSubsets(
        std::vector<T> inputSet,
        std::vector<std::set<T>> expectedSetOfSubsets)
    {
        for (size_t i = 0; i <= inputSet.size(); ++i)
        {
            auto setOfSubsets = arcana::compute_subsets<T>(inputSet.begin(), inputSet.end(), i);

            for (size_t j = 0; j < setOfSubsets.size(); ++j)
            {
                std::set<T> included = setOfSubsets[j];

                const size_t invalidResult = std::numeric_limits<size_t>::max();
                size_t indexSetFound = invalidResult;

                for (size_t k = 0; k < expectedSetOfSubsets.size(); ++k)
                {
                    // Find a matching element in expectedSetOfSubsets and remove it from there.
                    std::vector<T> diff;
                    std::set_difference(
                        expectedSetOfSubsets[k].begin(),
                        expectedSetOfSubsets[k].end(),
                        included.begin(),
                        included.end(),
                        std::inserter(diff, diff.begin()));

                    if (diff.size() == 0)
                    {
                        indexSetFound = k;
                        break;
                    }
                }

                EXPECT_TRUE(indexSetFound != invalidResult);

                expectedSetOfSubsets.erase(expectedSetOfSubsets.begin() + indexSetFound);
            }
        }

        EXPECT_EQ(0, (int)expectedSetOfSubsets.size());
    }

    std::vector<std::set<int>> getExpectedSetOfSubsetsForIntLength3()
    {
        std::vector<std::set<int>> expectedSetOfSubsets;

        expectedSetOfSubsets.resize(8);

        expectedSetOfSubsets[1] = { 1 };

        expectedSetOfSubsets[2] = { 2 };

        expectedSetOfSubsets[3] = { 1, 2 };

        expectedSetOfSubsets[4] = { 3 };

        expectedSetOfSubsets[5] = { 1, 3 };

        expectedSetOfSubsets[6] = { 2, 3 };

        expectedSetOfSubsets[7] = { 1, 2, 3 };

        return expectedSetOfSubsets;
    }

    std::vector<std::set<char>>getExpectedSetOfSubsetsForCharsLength3()
    {
        std::vector<std::set<char>> expectedSetOfSubsets;

        expectedSetOfSubsets.resize(8);

        expectedSetOfSubsets[1] = { 'a' };

        expectedSetOfSubsets[2] = { 'b' };

        expectedSetOfSubsets[3] = { 'a', 'b' };

        expectedSetOfSubsets[4] = { 'c' };

        expectedSetOfSubsets[5] = { 'a', 'c' };

        expectedSetOfSubsets[6] = { 'b', 'c' };

        expectedSetOfSubsets[7] = { 'a', 'b', 'c' };

        return expectedSetOfSubsets;
    }

    TEST(AlgorithmUnitTest, TestSetOfSubsetsForIntLength0)
    {
        std::vector<int> inputSet;
        std::vector<std::set<int>> expectedSetOfSubsets;

        AssertComputationOfSetOfSubsets(inputSet, expectedSetOfSubsets);
    }
}

TEST(AlgorithmUnitTest, TestSetOfSubsetsForIntLength3)
{
    std::vector<int> inputSet = { 1, 2, 3 };

    auto expectedSetOfSubsets = getExpectedSetOfSubsetsForIntLength3();
    AssertComputationOfSetOfSubsets(inputSet, expectedSetOfSubsets);
}

TEST(AlgorithmUnitTest, TestSetOfSubsetsForCharLength3)
{
    std::vector<char> inputSet = { 'c', 'b', 'a' };

    auto expectedSetOfSubsets = getExpectedSetOfSubsetsForCharsLength3();
    AssertComputationOfSetOfSubsets(inputSet, expectedSetOfSubsets);
}

TEST(AlgorithmUnitTest, TestMedianSingleItem)
{
    std::vector<int> inputSet = { 3 };
    int output = arcana::median<int>(inputSet.begin(), inputSet.end());
    EXPECT_EQ(3, output);
}

TEST(AlgorithmUnitTest, TestMedianOddSize)
{
    std::vector<int> inputSet = { 3, 7, 8, 9, 2, 0, 1 };
    int output = arcana::median<int>(inputSet.begin(), inputSet.end());
    EXPECT_EQ(3, output);
}

TEST(AlgorithmUnitTest, TestMedianEvenSize)
{
    std::vector<float> inputSet = { 3, 7, 8, 9, 2, 0, 1, 4 };
    float output = arcana::median<float>(inputSet.begin(), inputSet.end());
    EXPECT_FLOAT_EQ(3.5f, output);
}

TEST(AlgorithmUnitTest, TestMedianEvenSizeSqrtf)
{
    std::vector<float> inputSet = {25, 36};
    float output = arcana::median<float>(inputSet.begin(), inputSet.end(), std::sqrtf);
    EXPECT_FLOAT_EQ(5.5f, output);
}

using duration_t = std::chrono::duration<double, std::milli>;

TEST(AlgorithmUnitTest, RangeForStats_MultipleValue_Sum)
{
    std::vector<duration_t> testVector;
    testVector.push_back(11ms);
    testVector.push_back(21ms);
    testVector.push_back(10ms);
    testVector.push_back(42ms);
    testVector.push_back(53ms);
    const auto sum = arcana::sum<duration_t>(testVector.begin(), testVector.end());
    EXPECT_DOUBLE_EQ(sum.count(), 137.0);
}

TEST(AlgorithmUnitTest, RangeForStats_SingleValue_Sum)
{
    std::vector<duration_t> testVector;
    testVector.push_back(5ms);
    const auto sum = arcana::sum<duration_t>(testVector.begin(), testVector.end());
    EXPECT_DOUBLE_EQ(sum.count(), 5.0);
}

TEST(AlgorithmUnitTest, RangeForStats_SimilarValues_Mean)
{
    std::vector<duration_t> testVector;
    testVector.push_back(4ms);
    testVector.push_back(4ms);
    testVector.push_back(4ms);
    testVector.push_back(4ms);
    const auto mean = arcana::mean<duration_t>(testVector.begin(), testVector.end());
    EXPECT_DOUBLE_EQ(mean.count(), 4.0);
}

TEST(AlgorithmUnitTest, RangeForStats_DifferentValues_Mean)
{
    std::vector<duration_t> testVector;
    testVector.push_back(12ms);
    testVector.push_back(13ms);
    testVector.push_back(23ms);
    testVector.push_back(44ms);
    testVector.push_back(55ms);
    const auto mean = arcana::mean<duration_t>(testVector.begin(), testVector.end());
    EXPECT_DOUBLE_EQ(mean.count(), 29.4);
}

TEST(AlgorithmUnitTest, RangeForStats_OddSizeCollection_Median)
{
    std::vector<duration_t> testVector;
    testVector.push_back(13ms);
    testVector.push_back(23ms);
    testVector.push_back(12ms);
    testVector.push_back(44ms);
    testVector.push_back(55ms);
    std::sort(testVector.begin(), testVector.end());
    const auto median = arcana::median<duration_t>(testVector.begin(), testVector.end());
    EXPECT_DOUBLE_EQ(median.count(), 23.0);
}

TEST(AlgorithmUnitTest, RangeForStats_EvenSizeCollection_Median)
{
    std::vector<duration_t> testVector;
    testVector.push_back(23ms);
    testVector.push_back(12ms);
    testVector.push_back(44ms);
    testVector.push_back(55ms);
    std::sort(testVector.begin(), testVector.end());
    const auto median = arcana::median<duration_t>(testVector.begin(), testVector.end());
    EXPECT_DOUBLE_EQ(median.count(), 33.5);
}

TEST(AlgorithmUnitTest, RangeForStats_SimilarValues_StandardDeviation)
{
    std::vector<duration_t> testVector;
    testVector.push_back(4ms);
    testVector.push_back(4ms);
    testVector.push_back(4ms);
    testVector.push_back(4ms);
    const auto stDev = arcana::standard_deviation<double>(testVector.begin(), testVector.end(), [](const duration_t& duration) { return duration.count(); });
    EXPECT_DOUBLE_EQ(stDev, 0.0);
}

TEST(AlgorithmUnitTest, RangeForStats_DifferentValues_StandardDeviation)
{
    std::vector<duration_t> testVector;
    testVector.push_back(13ms);
    testVector.push_back(23ms);
    testVector.push_back(12ms);
    testVector.push_back(44ms);
    testVector.push_back(55ms);
    const auto stDev = arcana::standard_deviation<double>(testVector.begin(), testVector.end(), [](const duration_t& duration) { return duration.count(); });
    EXPECT_DOUBLE_EQ(stDev, 19.243180610283737);
}

TEST(AlgorithmUnitTest, PopulationStream_DifferentValues_PopulationStandardDeviation)
{
    std::vector<int> testVector;
    testVector.push_back(13);
    testVector.push_back(23);
    testVector.push_back(12);
    testVector.push_back(44);
    testVector.push_back(55);

    auto sum = arcana::sum<double>(testVector.begin(), testVector.end());
    EXPECT_DOUBLE_EQ(sum, 147.0);
    auto squaredSum = arcana::sum<double>(testVector.begin(), testVector.end(), [](const int& t) { return t * t; });
    EXPECT_DOUBLE_EQ(squaredSum, 5803.0);
    const auto stDev = arcana::population_standard_deviation<double>(
        sum,
        squaredSum,
        testVector.size());
    EXPECT_TRUE(std::abs(stDev - 17.21162397916) < 1e-9);
}

TEST(AlgorithmUnitTest, PopulationStream_SameValues_PopulationStandardDeviation)
{
    std::vector<int> testVector;
    testVector.push_back(4);
    testVector.push_back(4);
    testVector.push_back(4);
    testVector.push_back(4);

    auto sum = arcana::sum<double>(testVector.begin(), testVector.end());
    EXPECT_DOUBLE_EQ(sum, 16.0);
    auto squaredSum = arcana::sum<double>(testVector.begin(), testVector.end(), [](const int& t) { return t * t; });
    EXPECT_DOUBLE_EQ(squaredSum, 64.0);
    const auto stDev = arcana::population_standard_deviation<double>(
        sum,
        squaredSum,
        testVector.size());
    EXPECT_TRUE(std::abs(stDev - 0.0) < 1e-9);
}

TEST(AlgorithmUnitTest, PopulationStream_NegativePositiveValues_PopulationStandardDeviation)
{
    std::vector<int> testVector;
    testVector.push_back(-50);
    testVector.push_back(-25);
    testVector.push_back(10);
    testVector.push_back(35);
    testVector.push_back(500);

    auto sum = arcana::sum<double>(testVector.begin(), testVector.end());
    EXPECT_DOUBLE_EQ(sum, 470.0);
    auto squaredSum = arcana::sum<double>(testVector.begin(), testVector.end(), [](const int& t) { return t * t; });
    EXPECT_DOUBLE_EQ(squaredSum, 254450.0);
    const auto stDev = arcana::population_standard_deviation<double>(
        sum,
        squaredSum,
        testVector.size());
    EXPECT_TRUE(std::abs(stDev - 205.07071950915) < 1e-9);
}
