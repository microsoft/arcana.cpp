//
// Copyright (C) Microsoft Corporation. All rights reserved.
//

#include <arcana/containers/sorted_vector.h>
#include <arcana/containers/unique_vector.h>
#include <arcana/containers/unordered_bimap.h>
#include <arcana/containers/ticketed_collection.h>
#include <gtest/gtest.h>
#include <algorithm>
#include <numeric>

TEST(ContainerUnitTest, SortedVectorInsert)
{
    arcana::sorted_vector<int> elements{ 2, 3, 1, 4 };

    std::vector<int> desired{ 1, 2, 3, 4 };
    EXPECT_TRUE(equal(elements.begin(), elements.end(), desired.begin(), desired.end())) << "elements should be sorted";

    desired = { 1, 2, 3, 4, 5 };
    elements.insert(5);
    EXPECT_TRUE(equal(elements.begin(), elements.end(), desired.begin(), desired.end())) << "elements should be sorted";
}

TEST(ContainerUnitTest, UniqueVectorInsert)
{
    arcana::unique_vector<int> elements{ 2, 3, 1, 4 };

    std::vector<int> desired = { 1, 2, 3, 4 };
    elements.insert(3);
    EXPECT_TRUE(equal(elements.begin(), elements.end(), desired.begin(), desired.end())) << "elements should be sorted";

    desired = { 1, 2, 3, 4 };
    elements.insert(desired.begin(), desired.end());
    EXPECT_TRUE(equal(elements.begin(), elements.end(), desired.begin(), desired.end())) << "elements should be sorted";
}

TEST(ContainerUnitTest, SortedVectorMerge)
{
    {
        arcana::sorted_vector<int> elements{ 2, 3, 1, 4 };

        std::vector<int> desired{ 1, 1, 2, 2, 3, 3, 4, 4 };
        elements.merge(elements);
        EXPECT_TRUE(equal(elements.begin(), elements.end(), desired.begin(), desired.end())) << "elements should be sorted";
    }
    {
        arcana::unique_vector<int> elements{ 1, 2, 3, 4 };

        std::vector<int> desired = { 1, 2, 3, 4 };
        elements.merge(elements);
        EXPECT_TRUE(equal(elements.begin(), elements.end(), desired.begin(), desired.end())) << "elements should be sorted";
    }
    {
        arcana::sorted_vector<int> elements{ 2, 3, 1, 4 };

        arcana::sorted_vector<int> empty;
        std::vector<int> desired = { 1, 2, 3, 4 };
        elements.merge(empty);
        EXPECT_TRUE(equal(elements.begin(), elements.end(), desired.begin(), desired.end())) << "elements should be sorted";
    }
    {
        arcana::unique_vector<int> elements{ 2, 3, 1, 4 };

        arcana::unique_vector<int> empty;
        std::vector<int> desired = { 1, 2, 3, 4 };
        elements.merge(empty);
        EXPECT_TRUE(equal(elements.begin(), elements.end(), desired.begin(), desired.end())) << "elements should be sorted";
    }
}

static auto insert_item(arcana::ticketed_collection<int>& items, int i, std::mutex& mutex)
{
    std::lock_guard<std::mutex> guard{ mutex };
    return items.insert(i, mutex);
}

TEST(ContainerUnitTest, TicketedCollectionManipulation)
{
    arcana::ticketed_collection<int> items;
    std::mutex mutex;

    for (int i = 0; i < 10; ++i)
    {
        auto el = insert_item(items, i, mutex);
    }

    EXPECT_EQ(0u, items.size());
    EXPECT_TRUE(items.empty());

    {
        auto elHeld = insert_item(items, 10, mutex);

        EXPECT_EQ(1u, items.size());
        EXPECT_FALSE(items.empty());

        int count = 0;
        for (auto& el : items)
        {
            count++;
            EXPECT_EQ(10, el);
        }
        EXPECT_EQ(1, count);
    }

    EXPECT_EQ(0u, items.size());
    EXPECT_TRUE(items.empty());
}

TEST(ContainerUnitTest, UnorderedBimap)
{
    arcana::unordered_bimap<int, float> bimap;
    constexpr std::array<std::pair<int, float>, 3> values
    {
        std::pair<int, float>{ 5, 10.0f },
        std::pair<int, float>{ 15, 110.0f },
        std::pair<int, float>{ 115, 1110.0f }
    };

    for (const auto& value : values)
    {
        int left = value.first;
        float right = value.second;
        bimap.emplace(std::move(left), std::move(right));
    }

    for (const auto& value : values)
    {
        EXPECT_EQ(value.second, bimap.left().find(value.first)->second);
        EXPECT_EQ(value.first, bimap.right().find(value.second)->second);
    }
}
