#include <gtest/gtest.h>

#include <arcana/expected.h>

#include <memory>

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
    EXPECT_TRUE(weak.lock() == nullptr);
}

TEST(ExpectedUnitTest, ExpectedCopy)
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

    EXPECT_TRUE(weak.lock() == nullptr);
}

TEST(ExpectedUnitTest, ExpectedMove)
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

    EXPECT_TRUE(weak.lock() == nullptr);
}

TEST(ExpectedUnitTest, ExpectedCopyError)
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

    EXPECT_TRUE(weak.lock() == nullptr);
}

TEST(ExpectedUnitTest, ExpectedMoveError)
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

    EXPECT_TRUE(weak.lock() == nullptr);
}

TEST(ExpectedUnitTest, ExpectedAssignError)
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
        FAIL() << "value didn't throw an exception";
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
        FAIL() << "error didn't throw an exception";
    }
    catch (const arcana::bad_expected_access&)
    {
    }

    try
    {
        arcana::basic_expected<void, E> exp = arcana::basic_expected<void, E>::make_valid();
        exp.error();
        FAIL() << "error didn't throw an exception";
    }
    catch (const arcana::bad_expected_access&)
    {
    }
}

TEST(ExpectedUnitTest, ExpectedAccessExceptions)
{
    ExpectedAccessExceptionsTest<std::error_code>();
    ExpectedAccessExceptionsTest<std::exception_ptr>();
}

TEST(ExpectedUnitTest, ExpectedToExceptionalConversion)
{
    arcana::basic_expected<int, std::errc> errc{ arcana::make_unexpected(std::errc::broken_pipe) };
    arcana::basic_expected<int, std::exception_ptr> exp{ errc };
    try
    {
        std::rethrow_exception(exp.error());
    }
    catch (const std::system_error& error)
    {
        EXPECT_TRUE(error.code() == std::errc::broken_pipe) << "Wrong error code";
    }
    catch (...)
    {
        FAIL() << "The exception should have been converted to a system_error";
    }
}

TEST(ExpectedUnitTest, VoidExpectedToExceptionalConversion)
{
    arcana::basic_expected<void, std::errc> errc{ arcana::make_unexpected(std::errc::broken_pipe) };
    arcana::basic_expected<void, std::exception_ptr> exp{ errc };

    try
    {
        std::rethrow_exception(exp.error());
    }
    catch (const std::system_error& error)
    {
        EXPECT_TRUE(error.code() == std::errc::broken_pipe) << "Wrong error code";
    }
    catch (...)
    {
        FAIL() << "The exception should have been converted to a system_error";
    }

    arcana::basic_expected<void, std::exception_ptr> exp2 = errc;

    try
    {
        std::rethrow_exception(exp2.error());
    }
    catch (const std::system_error& error)
    {
        EXPECT_TRUE(error.code() == std::errc::broken_pipe) << "Wrong error code";
    }
    catch (...)
    {
        FAIL() << "The exception should have been converted to a system_error";
    }
}
