//
// Copyright (C) Microsoft Corporation. All rights reserved.
//

#ifdef __cpp_coroutines

#include <gtest/gtest.h>

#include <arcana/threading/dispatcher.h>
#include <arcana/threading/coroutine.h>

#include <future>

TEST(CoroutineTests, VoidTaskReturningCoroutine_GeneratesTask)
{
    bool executed = false;

    auto coroutine = [&executed]() -> arcana::task<void, std::error_code>
    {
        executed = true;
        co_return arcana::coroutine_success;
    };

    auto task = coroutine();

    EXPECT_TRUE(executed) << "Coroutine did not execute";
}

TEST(CoroutineTests, ValueTaskReturningCoroutine_GeneratesTask)
{
    auto coroutine = []() -> arcana::task<int, std::error_code>
    {
        co_return 42;
    };

    auto task = coroutine();

    bool completed = false;
    task.then(arcana::inline_scheduler, arcana::cancellation::none(), [&completed](int value) noexcept
    {
        completed = true;
        EXPECT_EQ(42, value) << "Task does not have the expected value";
    });

    EXPECT_TRUE(completed) << "Task did not complete synchronously";
}

TEST(CoroutineTests, ValueTaskReturningCoroutine_CanReturnErrorCode)
{
    auto error = std::make_error_code(std::errc::operation_not_supported);

    auto coroutine = [&error]() noexcept -> arcana::task<int, std::error_code>
    {
        co_return arcana::make_unexpected(error);
    };

    auto task = coroutine();

    bool completed = false;
    task.then(arcana::inline_scheduler, arcana::cancellation::none(), [&completed, &error](const arcana::expected<int, std::error_code>& result) noexcept
    {
        completed = true;
        EXPECT_EQ(error.value(), result.error().value()) << "Task does not have the expected error state";
    });

    EXPECT_TRUE(completed) << "Task did not complete synchronously";
}

TEST(CoroutineTests, CoAwaitingVoidReturningTask_ResumesOnCompletion)
{
    bool executed = false;

    auto coroutine = [&executed]() -> std::future<void>
    {
        arcana::expected<void, std::error_code> result = co_await arcana::configure_await(arcana::inline_scheduler, arcana::task_from_result<std::error_code>());
        executed = true;
    };

    coroutine().wait();

    EXPECT_TRUE(executed) << "Continuation did not execute";
}

TEST(CoroutineTests, CoAwaitingValueReturningTask_ResumesOnCompletion)
{
    auto coroutine = []() noexcept -> std::future<int>
    {
        arcana::expected<int, std::error_code> result = co_await arcana::configure_await(arcana::inline_scheduler, arcana::task_from_result<std::error_code>(42));
        co_return result.value();
    };

    auto future = coroutine();

    EXPECT_EQ(42, future.get()) << "Task does not have the expected value";
}

TEST(CoroutineTests, CoAwaitingValueReturningTask_CanReturnValue)
{
    auto coroutine = []() noexcept -> std::future<int>
    {
        int result = co_await arcana::configure_await(arcana::inline_scheduler, arcana::task_from_result<std::error_code>(42));
        co_return result;
    };

    auto future = coroutine();

    EXPECT_EQ(42, future.get()) << "Task does not have the expected value";
}

TEST(CoroutineTests, ValueTaskReturningCoroutine_CanCoAwaitTask)
{
    auto coroutine = []() noexcept -> arcana::task<int, std::error_code>
    {
        arcana::expected<int, std::error_code> result = co_await arcana::configure_await(arcana::inline_scheduler, arcana::task_from_result<std::error_code>(42));
        co_return result.value();
    };

    auto task = coroutine();

    bool completed = false;
    task.then(arcana::inline_scheduler, arcana::cancellation::none(), [&completed](int value) noexcept
    {
        completed = true;
        EXPECT_EQ(42, value) << "Task does not have the expected value";
    });

    EXPECT_TRUE(completed) << "Task did not complete synchronously";
}

TEST(CoroutineTests, CoAwaitingVoidReturningTask_CanPropagateError)
{
    auto error = std::make_error_code(std::errc::operation_not_supported);

    auto coroutine = [&error]() noexcept -> arcana::task<void, std::error_code>
    {
        co_await arcana::configure_await(arcana::inline_scheduler, arcana::task_from_error<void>(error));
        co_return arcana::coroutine_success;
    };

    auto task = coroutine();

    task.then(arcana::inline_scheduler, arcana::cancellation::none(), [&error](const arcana::expected<void, std::error_code>& result) noexcept
    {
        EXPECT_EQ(error.value(), result.error().value()) << "Final task does not contain expected error";
    });
}

TEST(CoroutineTests, CoAwaitingValueReturningTask_CanPropagateError)
{
    auto error = std::make_error_code(std::errc::operation_not_supported);

    auto coroutine = [&error]() noexcept -> arcana::task<int, std::error_code>
    {
        int result = co_await arcana::configure_await(arcana::inline_scheduler, arcana::task_from_error<int>(error));
        co_return result * 42;
    };

    auto task = coroutine();

    task.then(arcana::inline_scheduler, arcana::cancellation::none(), [&error](const arcana::expected<int, std::error_code>& result) noexcept
    {
        EXPECT_EQ(error.value(), result.error().value()) << "Final task does not contain expected error";
    });
}

TEST(CoroutineTests, CoAwaitingErrorReturningTask_CanManuallyHandleError)
{
    auto error = std::make_error_code(std::errc::operation_not_supported);

    auto coroutine = [&error]() -> arcana::task<int, std::error_code>
    {
        arcana::expected<int, std::error_code> result = co_await arcana::configure_await(arcana::inline_scheduler, arcana::task_from_error<int>(error));
        co_return 42;
    };

    auto task = coroutine();

    task.then(arcana::inline_scheduler, arcana::cancellation::none(), [&error](const arcana::expected<int, std::error_code>& result) noexcept
    {
        EXPECT_EQ(42, result.value()) << "Final task does not contain expected result";
    });
}

//TEST(CoroutineTests, CoAwaitingValueReturningTask_CanPropagateErrorInception)
//{
//    auto coroutine = []() -> arcana::task<int, std::error_code>
//    {
//        auto error = std::make_error_code(std::errc::not_supported);
//        auto result1 = co_await arcana::configure_await(arcana::inline_scheduler, arcana::task_from_error<void>(error));
//        auto result2 = co_await arcana::configure_await(arcana::inline_scheduler, arcana::task_from_error<void>(error));
//        co_return 42;
//    };

//    auto task = coroutine();

//    // TODO: This should blow up, but it should blow up in a way that makes it easy to diagnose.
//}

TEST(CoroutineTests, SwitchToScheduler_ChangesContext)
{
    arcana::background_dispatcher<32> background;

    auto coroutine = [&background]() -> std::future<void>
    {
        auto foregroundThreadId = std::this_thread::get_id();
        co_await switch_to(background);
        auto backgroundThreadId = std::this_thread::get_id();

        EXPECT_NE(std::hash<std::thread::id>{}(foregroundThreadId), std::hash<std::thread::id>{}(backgroundThreadId));
    };

    coroutine().wait();
}

TEST(CoroutineTests, VoidTaskReturningCoroutine_GeneratesExceptionalTask)
{
    bool executed = false;

    auto coroutine = [&executed]() -> arcana::task<void, std::exception_ptr>
    {
        executed = true;
        co_return;
    };

    auto task = coroutine();

    EXPECT_TRUE(executed) << "Coroutine did not execute";
}

TEST(CoroutineTests, ValueTaskReturningCoroutine_GeneratesExceptionalTask)
{
    auto coroutine = []() -> arcana::task<int, std::exception_ptr>
    {
        co_return 42;
    };

    auto task = coroutine();

    bool completed = false;
    task.then(arcana::inline_scheduler, arcana::cancellation::none(), [&completed](int value)
    {
        completed = true;
        EXPECT_EQ(42, value) << "Task does not have the expected value";
    });

    EXPECT_TRUE(completed) << "Task did not complete synchronously";
}

TEST(CoroutineTests, ValueTaskReturningCoroutine_CanThrowException)
{
    auto coroutine = []() -> arcana::task<int, std::exception_ptr>
    {
        throw std::logic_error("Some error.");
        co_return 42;
    };

    auto task = coroutine();

    bool completed = false;
    bool hasError = false;
    task.then(arcana::inline_scheduler, arcana::cancellation::none(), [&completed, &hasError](const arcana::expected<int, std::exception_ptr>& result)
    {
        completed = true;
        try
        {
            std::rethrow_exception(result.error());
        }
        catch (const std::logic_error&)
        {
            hasError = true;
        }
        catch (...)
        {
            FAIL() << "Wrong exception type thrown.";
        }
    });

    EXPECT_TRUE(completed) << "Task did not complete synchronously";
    EXPECT_TRUE(hasError) << "Task did not complete with an exception";
}

TEST(CoroutineTests, CoAwaitingVoidReturningExceptionalTask_ResumesOnCompletion)
{
    bool executed = false;

    auto coroutine = [&executed]() -> std::future<void>
    {
        co_await arcana::configure_await(arcana::inline_scheduler, arcana::task_from_result<std::exception_ptr>());
        executed = true;
    };

    coroutine().wait();

    EXPECT_TRUE(executed) << "Continuation did not execute";
}

TEST(CoroutineTests, CoAwaitingValueReturningExceptionalTask_ResumesOnCompletion)
{
    auto coroutine = []() noexcept -> std::future<int>
    {
        int result = co_await arcana::configure_await(arcana::inline_scheduler, arcana::task_from_result<std::exception_ptr>(42));
        co_return result;
    };

    auto future = coroutine();

    EXPECT_EQ(42, future.get()) << "Task does not have the expected value";
}

TEST(CoroutineTests, CoAwaitingVoidReturningExceptionalTask_CanPropagateException)
{
    auto coroutine = []() noexcept->std::future<void>
    {
        auto exception = std::make_exception_ptr(std::logic_error("Some error"));
        co_await arcana::configure_await(arcana::inline_scheduler, arcana::task_from_error<void>(exception));
    };

    auto future = coroutine();

    bool hasError = false;
    try
    {
        future.get();
    }
    catch (const std::logic_error&)
    {
        hasError = true;
    }
    catch (...)
    {
        FAIL() << "Wrong exception type thrown.";
    }

    EXPECT_TRUE(hasError) << "Task did not complete with an exception";
}

TEST(CoroutineTests, CoAwaitingValueReturningExceptionalTask_CanPropagateException)
{
    auto coroutine = []() noexcept->std::future<int>
    {
        auto exception = std::make_exception_ptr(std::logic_error("Some error"));
        co_await arcana::configure_await(arcana::inline_scheduler, arcana::task_from_error<void>(exception));
        co_return 42;
    };

    auto future = coroutine();

    bool hasError = false;
    try
    {
        future.get();
    }
    catch (const std::logic_error&)
    {
        hasError = true;
    }
    catch (...)
    {
        FAIL() << "Wrong exception type thrown.";
    }

    EXPECT_TRUE(hasError) << "Task did not complete with an exception";
}

TEST(CoroutineTests, CoAwaitingValueReturningExceptionalTask_CanCatchException)
{
    bool caughtException = false;

    auto coroutine = [&caughtException]() noexcept->std::future<int>
    {
        auto exception = std::make_exception_ptr(std::logic_error("Some error"));
        try
        {
            co_await arcana::configure_await(arcana::inline_scheduler, arcana::task_from_error<void>(exception));
        }
        catch (const std::logic_error&)
        {
            caughtException = true;
        }
        co_return 42;
    };

    auto future = coroutine();

    EXPECT_EQ(42, future.get()) << "Value was not returned from coroutine.";
    EXPECT_TRUE(caughtException) << "Exception from co_await was not caught.";
}

TEST(CoroutineTests, CoAwaitingVoidReturningTask_CanPropagateException)
{
    auto error = std::make_error_code(std::errc::operation_not_supported);

    auto coroutine = [&error]() -> arcana::task<void, std::exception_ptr>
    {
        co_await arcana::configure_await(arcana::inline_scheduler, arcana::task_from_error<void>(error));
    };

    auto task = coroutine();

    bool completed = false;
    bool hasError = false;
    task.then(arcana::inline_scheduler, arcana::cancellation::none(), [&completed, &error, &hasError](const arcana::expected<void, std::exception_ptr>& result)
    {
        completed = true;
        try
        {
            std::rethrow_exception(result.error());
        }
        catch (const std::system_error& exception)
        {
            hasError = true;
            EXPECT_EQ(error.value(), exception.code().value()) << "Task does not have the expected error state";
        }
        catch (...)
        {
            FAIL() << "Wrong exception type thrown.";
        }
    });

    EXPECT_TRUE(completed) << "Task did not complete synchronously";
    EXPECT_TRUE(hasError) << "Task did not complete with an exception";
}

#endif
