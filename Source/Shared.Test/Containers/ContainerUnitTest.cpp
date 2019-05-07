//
// Copyright (C) Microsoft Corporation. All rights reserved.
//

#include <arcana/containers/sorted_vector.h>
#include <arcana/containers/unique_vector.h>
#include <arcana/containers/unordered_bimap.h>
#include <arcana/containers/ticketed_collection.h>
#include <CppUnitTest.h>
#include <algorithm>
#include <numeric>

using Assert = Microsoft::VisualStudio::CppUnitTestFramework::Assert;

namespace UnitTests
{
    TEST_CLASS(ContainerUnitTest)
    {
        TEST_METHOD(SortedVectorInsert)
        {
            arcana::sorted_vector<int> elements{ 2, 3, 1, 4 };

            std::vector<int> desired{ 1, 2, 3, 4 };
            Assert::IsTrue(equal(elements.begin(), elements.end(), desired.begin(), desired.end()), L"elements should be sorted");

            desired = { 1, 2, 3, 4, 5 };
            elements.insert(5);
            Assert::IsTrue(equal(elements.begin(), elements.end(), desired.begin(), desired.end()), L"elements should be sorted");
        }

        TEST_METHOD(UniqueVectorInsert)
        {
            arcana::unique_vector<int> elements{ 2, 3, 1, 4 };

            std::vector<int> desired = { 1, 2, 3, 4 };
            elements.insert(3);
            Assert::IsTrue(equal(elements.begin(), elements.end(), desired.begin(), desired.end()), L"elements should be sorted");

            desired = { 1, 2, 3, 4 };
            elements.insert(desired.begin(), desired.end());
            Assert::IsTrue(equal(elements.begin(), elements.end(), desired.begin(), desired.end()), L"elements should be sorted");
        }

        TEST_METHOD(SortedVectorMerge)
        {
            {
                arcana::sorted_vector<int> elements{ 2, 3, 1, 4 };

                std::vector<int> desired{ 1, 1, 2, 2, 3, 3, 4, 4 };
                elements.merge(elements);
                Assert::IsTrue(equal(elements.begin(), elements.end(), desired.begin(), desired.end()), L"elements should be sorted");
            }
            {
                arcana::unique_vector<int> elements{ 1, 2, 3, 4 };

                std::vector<int> desired = { 1, 2, 3, 4 };
                elements.merge(elements);
                Assert::IsTrue(equal(elements.begin(), elements.end(), desired.begin(), desired.end()), L"elements should be sorted");
            }
            {
                arcana::sorted_vector<int> elements{ 2, 3, 1, 4 };

                arcana::sorted_vector<int> empty;
                std::vector<int> desired = { 1, 2, 3, 4 };
                elements.merge(empty);
                Assert::IsTrue(equal(elements.begin(), elements.end(), desired.begin(), desired.end()), L"elements should be sorted");
            }
            {
                arcana::unique_vector<int> elements{ 2, 3, 1, 4 };

                arcana::unique_vector<int> empty;
                std::vector<int> desired = { 1, 2, 3, 4 };
                elements.merge(empty);
                Assert::IsTrue(equal(elements.begin(), elements.end(), desired.begin(), desired.end()), L"elements should be sorted");
            }
        }

        static auto insert_item(arcana::ticketed_collection<int>& items, int i, std::mutex& mutex)
        {
            std::lock_guard<std::mutex> guard{ mutex };
            return items.insert(i, mutex);
        }

        TEST_METHOD(TicketedCollectionManipulation)
        {
            arcana::ticketed_collection<int> items;
            std::mutex mutex;

            for (int i = 0; i < 10; ++i)
            {
                auto el = insert_item(items, i, mutex);
            }

            Assert::AreEqual<size_t>(0, items.size());
            Assert::IsTrue(items.empty());

            {
                auto elHeld = insert_item(items, 10, mutex);

                Assert::AreEqual<size_t>(1, items.size());
                Assert::IsFalse(items.empty());

                int count = 0;
                for (auto& el : items)
                {
                    count++;
                    Assert::AreEqual(10, el);
                }
                Assert::AreEqual(1, count);
            }

            Assert::AreEqual<size_t>(0, items.size());
            Assert::IsTrue(items.empty());
        }

        TEST_METHOD(UnorderedBimap)
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
                Assert::AreEqual(value.second, bimap.left().find(value.first)->second);
                Assert::AreEqual(value.first, bimap.right().find(value.second)->second);
            }
        }
    };
}
