#include <CppUnitTest.h>

#include <arcana\expected.h>

#include <memory>

using Assert = Microsoft::VisualStudio::CppUnitTestFramework::Assert;

namespace UnitTests
{
    TEST_CLASS(ExpectedUnitTest)
    {
        template<typename E>
        static void ExpectedCopyTest()
        {
            auto data = std::make_shared<int>(42);
            std::weak_ptr<int> weak = data;

            {
                arcana::basic_expected<std::shared_ptr<int>, E> exp{ data };

                arcana::basic_expected<std::shared_ptr<int>, E> other{ exp };

                exp = other;
            }

            data.reset();
            Assert::IsTrue(weak.lock() == nullptr);
        }

        TEST_METHOD(ExpectedCopy)
        {
            ExpectedCopyTest<std::error_code>();
            ExpectedCopyTest<std::exception_ptr>();
        }

        template<typename E>
        static void ExpectedMoveTest()
        {
            auto data = std::make_shared<int>(42);
            std::weak_ptr<int> weak = data;

            {
                arcana::basic_expected<std::shared_ptr<int>, E> exp{ std::move(data) };

                arcana::basic_expected<std::shared_ptr<int>, E> other{ std::move(exp) };

                exp = std::move(other);
            }

            Assert::IsTrue(weak.lock() == nullptr);
        }

        TEST_METHOD(ExpectedMove)
        {
            ExpectedMoveTest<std::error_code>();
            ExpectedMoveTest<std::exception_ptr>();
        }

        template<typename E>
        static void ExpectedCopyErrorTest()
        {
            auto data = std::make_shared<int>(42);
            std::weak_ptr<int> weak = data;

            {
                arcana::basic_expected<std::shared_ptr<int>, E> exp{ std::move(data) };

                arcana::basic_expected<std::shared_ptr<int>, E> other{ arcana::make_unexpected(std::errc::operation_canceled) };

                exp = other;
            }

            Assert::IsTrue(weak.lock() == nullptr);
        }

        TEST_METHOD(ExpectedCopyError)
        {
            ExpectedCopyErrorTest<std::error_code>();
            ExpectedCopyErrorTest<std::exception_ptr>();
        }

        template<typename E>
        static void ExpectedMoveErrorTest()
        {
            auto data = std::make_shared<int>(42);
            std::weak_ptr<int> weak = data;

            {
                arcana::basic_expected<std::shared_ptr<int>, E> exp{ std::move(data) };

                arcana::basic_expected<std::shared_ptr<int>, E> other{ arcana::make_unexpected(std::errc::operation_canceled) };

                exp = std::move(other);
            }

            Assert::IsTrue(weak.lock() == nullptr);
        }

        TEST_METHOD(ExpectedMoveError)
        {
            ExpectedMoveErrorTest<std::error_code>();
            ExpectedMoveErrorTest<std::exception_ptr>();
        }

        template<typename E>
        static void ExpectedAssignErrorTest()
        {
            auto data = std::make_shared<int>(42);
            std::weak_ptr<int> weak = data;

            {
                arcana::basic_expected<std::shared_ptr<int>, E> exp{ std::move(data) };

                exp = arcana::make_unexpected(std::errc::operation_canceled);
            }

            Assert::IsTrue(weak.lock() == nullptr);
        }

        TEST_METHOD(ExpectedAssignError)
        {
            ExpectedAssignErrorTest<std::error_code>();
            ExpectedAssignErrorTest<std::exception_ptr>();
        }

        template<typename E>
        static void ExpectedAccessExceptionsTest()
        {
            {
                arcana::basic_expected<int, E> exp{ arcana::make_unexpected(std::errc::broken_pipe) };
                exp.error();
            }

            try
            {
                arcana::basic_expected<int, E> exp{ arcana::make_unexpected(std::errc::broken_pipe) };
                exp.value();
                Assert::Fail(L"value didn't throw an exception");
            }
            catch (const arcana::bad_expected_access&)
            {
            }

            {
                arcana::basic_expected<int, E> exp{ 10 };
                exp.value();
            }

            try
            {
                arcana::basic_expected<int, E> exp{ 10 };
                exp.error();
                Assert::Fail(L"error didn't throw an exception");
            }
            catch (const arcana::bad_expected_access&)
            {
            }

            try
            {
                arcana::basic_expected<void, E> exp = arcana::basic_expected<void, E>::make_valid();
                exp.error();
                Assert::Fail(L"error didn't throw an exception");
            }
            catch (const arcana::bad_expected_access&)
            {
            }
        }

        TEST_METHOD(ExpectedAccessExceptions)
        {
            ExpectedAccessExceptionsTest<std::error_code>();
            ExpectedAccessExceptionsTest<std::exception_ptr>();
        }

        TEST_METHOD(ExpectedToExceptionalConversion)
        {
            arcana::basic_expected<int, std::errc> errc{ arcana::make_unexpected(std::errc::broken_pipe) };
            arcana::basic_expected<int, std::exception_ptr> exp{ errc };
            try
            {
                std::rethrow_exception(exp.error());
            }
            catch (const std::system_error& error)
            {
                Assert::IsTrue(error.code() == std::errc::broken_pipe, L"Wrong error code");
            }
            catch (...)
            {
                Assert::Fail(L"The exception should have been converted to a system_error");
            }
        }

        TEST_METHOD(VoidExpectedToExceptionalConversion)
        {
            arcana::basic_expected<void, std::errc> errc{ arcana::make_unexpected(std::errc::broken_pipe) };
            arcana::basic_expected<void, std::exception_ptr> exp{ errc };

            try
            {
                std::rethrow_exception(exp.error());
            }
            catch (const std::system_error& error)
            {
                Assert::IsTrue(error.code() == std::errc::broken_pipe, L"Wrong error code");
            }
            catch (...)
            {
                Assert::Fail(L"The exception should have been converted to a system_error");
            }

            arcana::basic_expected<void, std::exception_ptr> exp2 = errc;

            try
            {
                std::rethrow_exception(exp2.error());
            }
            catch (const std::system_error& error)
            {
                Assert::IsTrue(error.code() == std::errc::broken_pipe, L"Wrong error code");
            }
            catch (...)
            {
                Assert::Fail(L"The exception should have been converted to a system_error");
            }

        }
    };
}
