#include <CppUnitTest.h>

#include <arcana/iterators.h>

using Assert = Microsoft::VisualStudio::CppUnitTestFramework::Assert;

namespace UnitTests
{
    TEST_CLASS(IteratorUnitTest)
    {
        TEST_METHOD(Iterator_WhenStaticForEmpty_NothingIsRun)
        {
            int value = 0;
            arcana::static_for<0>([&](auto index)
            {
                value += 1;
            });

            Assert::AreEqual(0, value, L"Value shouldn't have been incremented");
        }

        TEST_METHOD(Iterator_WhenStaticFor_NumberOfIterationsIsCorrect)
        {
            int iterations = 0;
            arcana::static_for<10>([&](auto index)
            {
                iterations += 1;
            });

            Assert::AreEqual(10, iterations, L"Invalid number of iterations");
        }

        TEST_METHOD(Iterator_WhenStaticForEachEmpty_NothingIsRun)
        {
            int value = 0;
            arcana::static_foreach([&](auto index)
            {
                value += 1;
            });

            Assert::AreEqual(0, value, L"Value shouldn't have been incremented");
        }

        TEST_METHOD(Iterator_WhenStaticForEach_NumberOfIterationsIsCorrect)
        {
            int iterations = 0, value = 0;
            arcana::static_foreach([&](auto index)
            {
                iterations += 1;
                value += index;
            }, 0, 1, 2, 3, 4, 5);

            Assert::AreEqual(6, iterations, L"Invalid number of iterations");
            Assert::AreEqual(15, value, L"Invalid sum");
        }

        TEST_METHOD(Iterator_WhenTupleForEachEmpty_NothingIsRun)
        {
            int sum = 0;
            arcana::iterate_tuple(std::tuple<>{}, [&](auto value, auto index)
            {
                sum += 1;
            });

            Assert::AreEqual(0, sum, L"Value shouldn't have been incremented");
        }

        TEST_METHOD(Iterator_WhenTupleForEach_NumberOfIterationsIsCorrect)
        {
            int iterations = 0, sum = 0;
            arcana::iterate_tuple(std::make_tuple(0, 1, 2, 3, 4, 5), [&](auto value, auto index)
            {
                iterations += 1;
                sum += value;
            });

            Assert::AreEqual(6, iterations, L"Invalid number of iterations");
            Assert::AreEqual(15, sum, L"Invalid sum");
        }
    };
}
