//
// Copyright (C) Microsoft Corporation. All rights reserved.
//

#ifdef __cpp_coroutines

#include <CppUnitTest.h>

#include <arcana/threading/dispatcher.h>
#include <arcana/threading/coroutine.h>

#include <future>

using Assert = Microsoft::VisualStudio::CppUnitTestFramework::Assert;

namespace UnitTests
{
    TEST_CLASS(CoroutineTests)
    {
    public:
        TEST_METHOD(VoidTaskReturningCoroutine_GeneratesTask)
        {
            bool executed = false;

            auto coroutine = [&executed]() -> arcana::task<void, std::error_code>
            {
                executed = true;
                co_return arcana::coroutine_success;
            };

            auto task = coroutine();

            Assert::IsTrue(executed, L"Coroutine did not execute");
        }

        TEST_METHOD(ValueTaskReturningCoroutine_GeneratesTask)
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
                Assert::AreEqual(42, value, L"Task does not have the expected value");
            });

            Assert::IsTrue(completed, L"Task did not complete synchronously");
        }

        TEST_METHOD(ValueTaskReturningCoroutine_CanReturnErrorCode)
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
                Assert::AreEqual(error.value(), result.error().value(), L"Task does not have the expected error state");
            });

            Assert::IsTrue(completed, L"Task did not complete synchronously");
        }

        TEST_METHOD(CoAwaitingVoidReturningTask_ResumesOnCompletion)
        {
            bool executed = false;

            auto coroutine = [&executed]() -> std::future<void>
            {
                arcana::expected<void, std::error_code> result = co_await arcana::configure_await(arcana::inline_scheduler, arcana::task_from_result<std::error_code>());
                executed = true;
            };

            coroutine().wait();

            Assert::IsTrue(executed, L"Continuation did not execute");
        }

        TEST_METHOD(CoAwaitingValueReturningTask_ResumesOnCompletion)
        {
            auto coroutine = []() noexcept -> std::future<int>
            {
                arcana::expected<int, std::error_code> result = co_await arcana::configure_await(arcana::inline_scheduler, arcana::task_from_result<std::error_code>(42));
                co_return result.value();
            };

            auto future = coroutine();

            Assert::AreEqual(42, future.get(), L"Task does not have the expected value");
        }

        TEST_METHOD(CoAwaitingValueReturningTask_CanReturnValue)
        {
            auto coroutine = []() noexcept -> std::future<int>
            {
                int result = co_await arcana::configure_await(arcana::inline_scheduler, arcana::task_from_result<std::error_code>(42));
                co_return result;
            };

            auto future = coroutine();

            Assert::AreEqual(42, future.get(), L"Task does not have the expected value");
        }

        TEST_METHOD(ValueTaskReturningCoroutine_CanCoAwaitTask)
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
                Assert::AreEqual(42, value, L"Task does not have the expected value");
            });

            Assert::IsTrue(completed, L"Task did not complete synchronously");
        }

        TEST_METHOD(CoAwaitingVoidReturningTask_CanPropagateError)
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
                Assert::AreEqual(error.value(), result.error().value(), L"Final task does not contain expected error");
            });
        }

        TEST_METHOD(CoAwaitingValueReturningTask_CanPropagateError)
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
                Assert::AreEqual(error.value(), result.error().value(), L"Final task does not contain expected error");
            });
        }

        TEST_METHOD(CoAwaitingErrorReturningTask_CanManuallyHandleError)
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
                Assert::AreEqual(42, result.value(), L"Final task does not contain expected result");
            });
        }

        //TEST_METHOD(CoAwaitingValueReturningTask_CanPropagateErrorInception)
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

        TEST_METHOD(SwitchToScheduler_ChangesContext)
        {
            arcana::background_dispatcher<32> background;

            auto coroutine = [&background]() -> std::future<void>
            {
                auto foregroundThreadId = std::this_thread::get_id();
                co_await switch_to(background);
                auto backgroundThreadId = std::this_thread::get_id();

                Assert::AreNotEqual(std::hash<std::thread::id>{}(foregroundThreadId), std::hash<std::thread::id>{}(backgroundThreadId));
            };

            coroutine().wait();
        }

        TEST_METHOD(VoidTaskReturningCoroutine_GeneratesExceptionalTask)
        {
            bool executed = false;

            auto coroutine = [&executed]() -> arcana::task<void, std::exception_ptr>
            {
                executed = true;
                co_return;
            };

            auto task = coroutine();

            Assert::IsTrue(executed, L"Coroutine did not execute");
        }

        TEST_METHOD(ValueTaskReturningCoroutine_GeneratesExceptionalTask)
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
                Assert::AreEqual(42, value, L"Task does not have the expected value");
            });

            Assert::IsTrue(completed, L"Task did not complete synchronously");
        }

        TEST_METHOD(ValueTaskReturningCoroutine_CanThrowException)
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
                    Assert::Fail(L"Wrong exception type thrown.");
                }
            });

            Assert::IsTrue(completed, L"Task did not complete synchronously");
            Assert::IsTrue(hasError, L"Task did not complete with an exception");
        }

        TEST_METHOD(CoAwaitingVoidReturningExceptionalTask_ResumesOnCompletion)
        {
            bool executed = false;

            auto coroutine = [&executed]() -> std::future<void>
            {
                co_await arcana::configure_await(arcana::inline_scheduler, arcana::task_from_result<std::exception_ptr>());
                executed = true;
            };

            coroutine().wait();

            Assert::IsTrue(executed, L"Continuation did not execute");
        }

        TEST_METHOD(CoAwaitingValueReturningExceptionalTask_ResumesOnCompletion)
        {
            auto coroutine = []() noexcept -> std::future<int>
            {
                int result = co_await arcana::configure_await(arcana::inline_scheduler, arcana::task_from_result<std::exception_ptr>(42));
                co_return result;
            };

            auto future = coroutine();

            Assert::AreEqual(42, future.get(), L"Task does not have the expected value");
        }

        TEST_METHOD(CoAwaitingVoidReturningExceptionalTask_CanPropagateException)
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
                Assert::Fail(L"Wrong exception type thrown.");
            }

            Assert::IsTrue(hasError, L"Task did not complete with an exception");
        }

        TEST_METHOD(CoAwaitingValueReturningExceptionalTask_CanPropagateException)
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
                Assert::Fail(L"Wrong exception type thrown.");
            }

            Assert::IsTrue(hasError, L"Task did not complete with an exception");
        }

        TEST_METHOD(CoAwaitingValueReturningExceptionalTask_CanCatchException)
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

            Assert::AreEqual(42, future.get(), L"Value was not returned from coroutine.");
            Assert::IsTrue(caughtException, L"Exception from co_await was not caught.");
        }

        TEST_METHOD(CoAwaitingVoidReturningTask_CanPropagateException)
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
                    Assert::AreEqual(error.value(), exception.code().value(), L"Task does not have the expected error state");
                }
                catch (...)
                {
                    Assert::Fail(L"Wrong exception type thrown.");
                }
            });

            Assert::IsTrue(completed, L"Task did not complete synchronously");
            Assert::IsTrue(hasError, L"Task did not complete with an exception");
        }
    };
}

#endif
