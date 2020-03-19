#include <CppUnitTest.h>

#include <arcana/threading/dispatcher.h>

#include <arcana/threading/task.h>

#include <arcana/threading/pending_task_scope.h>

#include <arcana/expected.h>

#include <numeric>
#include <algorithm>
#include <future>

using Assert = Microsoft::VisualStudio::CppUnitTestFramework::Assert;

namespace UnitTests
{
    TEST_CLASS(TaskUnitTest)
    {
        TEST_METHOD(CancellationCallback)
        {
            arcana::cancellation_source source;

            int hit = 0;
            auto rego = source.add_listener([&]
            {
                hit++;
            });

            Assert::AreEqual(0, hit);

            source.cancel();

            Assert::AreEqual(1, hit);
        }

        TEST_METHOD(TaskSimpleOrdering)
        {
            arcana::manual_dispatcher<32> dis;

            std::stringstream ss;

            arcana::make_task(dis, arcana::cancellation::none(), [&]() noexcept
            {
                ss << "A";
            }).then(dis, arcana::cancellation::none(), [&]() noexcept
            {
                ss << "B";
            }).then(dis, arcana::cancellation::none(), [&]() noexcept
            {
                ss << "C";
            });

            arcana::cancellation_source cancel;
            while (dis.tick(cancel)) {};

            Assert::AreEqual<std::string>("ABC", ss.str());
        }

        TEST_METHOD(TransformTaskFromResult)
        {
            int result = 0;
            arcana::task_from_result<std::error_code>(10).then(arcana::inline_scheduler, arcana::cancellation::none(), [&](int value) noexcept
            {
                result = 2 * value;
            });

            Assert::AreEqual(20, result);
        }

        TEST_METHOD(CollapsedTaskOrdering)
        {
            auto one = arcana::task_from_result<std::error_code>(),
                two = arcana::task_from_result<std::error_code>();

            arcana::task_completion_source<void, std::error_code> start;
            arcana::task_completion_source<void, std::error_code> other;

            std::stringstream ss;

            auto starttask = start.as_task();

            auto composed = starttask.then(arcana::inline_scheduler, arcana::cancellation::none(), [&]() noexcept
            {
                ss << "1";

                return one.then(arcana::inline_scheduler, arcana::cancellation::none(), [&]() noexcept
                {
                    ss << "2";

                    return other.as_task().then(arcana::inline_scheduler, arcana::cancellation::none(), [&]() noexcept
                    {
                        return two;
                    });

                }).then(arcana::inline_scheduler, arcana::cancellation::none(), [&]() noexcept
                {
                    ss << "4";
                });
            });

            other.as_task().then(arcana::inline_scheduler, arcana::cancellation::none(), [&]() noexcept
            {
                ss << "3";
            });

            two.then(arcana::inline_scheduler, arcana::cancellation::none(), [&]() noexcept
            {
                ss << "0";
            });

            composed.then(arcana::inline_scheduler, arcana::cancellation::none(), [&]() noexcept
            {
                ss << "5";
            });

            auto composed2 = composed.then(arcana::inline_scheduler, arcana::cancellation::none(), [&]() noexcept
            {
                ss << "6";
            });

            // composed2 continuations should run before this extra composed continuation
            composed.then(arcana::inline_scheduler, arcana::cancellation::none(), [&]() noexcept
            {
                ss << "8";
            });

            composed2.then(arcana::inline_scheduler, arcana::cancellation::none(), [&]() noexcept
            {
                ss << "7";
            });

            start.complete();
            other.complete();

            Assert::AreEqual<std::string>("012345678", ss.str());
        }

        TEST_METHOD(TaskDualOrdering)
        {
            arcana::manual_dispatcher<32> dis1;
            arcana::manual_dispatcher<32> dis2;

            std::stringstream ss;

            make_task(dis1, arcana::cancellation::none(), [&]() noexcept
            {
                ss << "A";
            }).then(dis2, arcana::cancellation::none(), [&]() noexcept
            {
                ss << "B";
            }).then(dis1, arcana::cancellation::none(), [&]() noexcept
            {
                ss << "C";
            });

            arcana::cancellation_source cancel;
            while (dis1.tick(cancel) || dis2.tick(cancel)) {};

            Assert::AreEqual<std::string>("ABC", ss.str());
        }

        TEST_METHOD(TaskInvertedDualOrdering)
        {
            arcana::manual_dispatcher<32> dis1;
            arcana::manual_dispatcher<32> dis2;

            std::stringstream ss;

            make_task(dis1, arcana::cancellation::none(), [&]() noexcept
            {
                ss << "A";
            }).then(dis2, arcana::cancellation::none(), [&]() noexcept
            {
                ss << "B";
            }).then(dis1, arcana::cancellation::none(), [&]() noexcept
            {
                ss << "C";
            });

            arcana::cancellation_source cancel;
            while (dis2.tick(cancel) || dis1.tick(cancel)) {};

            Assert::AreEqual<std::string>("ABC", ss.str());
        }

        TEST_METHOD(TaskThreadedOrdering)
        {
            arcana::background_dispatcher<32> dis1;
            arcana::background_dispatcher<32> dis2;
            arcana::background_dispatcher<32> dis3;

            std::promise<void> work;
            std::future<void> finished = work.get_future();

            std::stringstream ss;

            make_task(dis1, arcana::cancellation::none(), [&]() noexcept
            {
                ss << "A";
            }).then(dis2, arcana::cancellation::none(), [&]() noexcept
            {
                ss << "B";
            }).then(dis3, arcana::cancellation::none(), [&]() noexcept
            {
                ss << "C";
            }).then(dis1, arcana::cancellation::none(), [&]() noexcept
            {
                work.set_value();
            });

            finished.wait();

            Assert::AreEqual<std::string>("ABC", ss.str());
        }

        TEST_METHOD(TaskReturnValue)
        {
            arcana::background_dispatcher<32> dis1;
            arcana::background_dispatcher<32> dis2;

            std::promise<std::string> work;
            std::future<std::string> finished = work.get_future();

            make_task(dis1, arcana::cancellation::none(), [&]() noexcept -> std::string
            {
                return "A";
            }).then(dis2, arcana::cancellation::none(), [&](const std::string& letter) noexcept
            {
                return letter + "B";
            }).then(dis1, arcana::cancellation::none(), [&](const std::string& letter) noexcept
            {
                return letter + "C";
            }).then(dis2, arcana::cancellation::none(), [&](const std::string& result) noexcept
            {
                work.set_value(result);
            });

            Assert::AreEqual<std::string>("ABC", finished.get());
        }

        struct counter
        {
            counter() = default;

            counter(int& value)
                : m_value{ &value }
            {
                (*m_value) = 0;
            }

            counter(counter&& other)
                : m_value{other.m_value}
            {
                other.m_value = nullptr;
            }

            counter& operator=(counter&& other)
            {
                std::swap(m_value, other.m_value);
                return *this;
            }

            ~counter()
            {
                if (m_value)
                    (*m_value) += 1;
            }

            int* m_value = nullptr;
        };

        TEST_METHOD(TaskCleanupResults)
        {
            arcana::manual_dispatcher<32> dis1;
            arcana::manual_dispatcher<32> dis2;

            int destructions;

            make_task(dis1, arcana::cancellation::none(), [&]() noexcept
            {
                return counter(destructions);
            }).then(dis2, arcana::cancellation::none(), [&](const counter& count) noexcept
            {
                Assert::AreNotEqual<int*>(nullptr, count.m_value);
            }).then(dis2, arcana::cancellation::none(), [&]() noexcept
            {
                Assert::AreEqual(1, destructions);
            });

            arcana::cancellation_source cancel;
            while (dis2.tick(cancel) || dis1.tick(cancel)) {};

            Assert::AreEqual(1, destructions);
        }

        TEST_METHOD(TaskCleanupLambdas)
        {
            arcana::manual_dispatcher<32> dis1;
            arcana::manual_dispatcher<32> dis2;

            std::shared_ptr<int> shared = std::make_shared<int>(10);
            std::weak_ptr<int> weak = shared;

            make_task(dis1, arcana::cancellation::none(), [shared]() noexcept
            {
            }).then(dis2, arcana::cancellation::none(), [shared]() noexcept
            {
            }).then(dis2, arcana::cancellation::none(), [shared]() noexcept
            {
            });

            shared.reset();

            arcana::cancellation_source cancel;
            while (dis2.tick(cancel) || dis1.tick(cancel)) {};

            Assert::AreEqual<int*>(nullptr, weak.lock().get());
        }

        TEST_METHOD(TaskCleanupVoidLambdasAfterCancellation)
        {
            arcana::cancellation_source cancel;
            arcana::manual_dispatcher<32> dis1;
            arcana::manual_dispatcher<32> dis2;

            std::shared_ptr<int> shared = std::make_shared<int>(10);
            std::weak_ptr<int> weak = shared;

            int run = 0;

            {
                make_task(dis1, arcana::cancellation::none(), [&, shared]() noexcept
                {
                    run++;
                }).then(dis2, arcana::cancellation::none(), [&, shared]() noexcept
                {
                    run++;
                }).then(dis2, arcana::cancellation::none(), [&, shared]() noexcept
                {
                    run++;
                });
            }

            shared.reset();

            dis1.tick(cancel);

            dis1.clear();
            dis2.clear();

            Assert::AreEqual(1, run);
            Assert::AreEqual<int*>(nullptr, weak.lock().get());
        }

        TEST_METHOD(TaskCleanupLambdasAfterCancellation)
        {
            arcana::cancellation_source cancel;
            arcana::manual_dispatcher<32> dis1;
            arcana::manual_dispatcher<32> dis2;

            std::shared_ptr<int> shared = std::make_shared<int>(10);
            std::weak_ptr<int> weak = shared;

            int run = 0;

            {
                make_task(dis1, arcana::cancellation::none(), [&, shared]() noexcept
                {
                    run++;
                    return 0;
                }).then(dis2, arcana::cancellation::none(), [&, shared](int value) noexcept
                {
                    run++;
                    return value + 1;
                }).then(dis2, arcana::cancellation::none(), [&, shared](int value) noexcept
                {
                    run++;
                    return value + 1;
                });
            }

            shared.reset();

            dis1.tick(cancel);

            dis1.clear();
            dis2.clear();

            Assert::AreEqual(1, run);
            Assert::AreEqual<int*>(nullptr, weak.lock().get());
        }

        TEST_METHOD(LateContinuation)
        {
            arcana::cancellation_source cancel;

            arcana::manual_dispatcher<32> dis1;
            arcana::manual_dispatcher<32> dis2;

            std::stringstream ss;

            auto task = make_task(dis1, arcana::cancellation::none(), [&]() noexcept
            {
                ss << "A";
            });

            dis1.tick(cancel);

            task = task.then(dis2, arcana::cancellation::none(), [&]() noexcept
            {
                ss << "B";
            });

            while (dis2.tick(cancel) || dis1.tick(cancel)) {};

            task.then(dis1, arcana::cancellation::none(), [&]() noexcept
            {
                ss << "C";
            });

            dis1.tick(cancel);

            Assert::AreEqual<std::string>("ABC", ss.str());
        }

        TEST_METHOD(FromResult)
        {
            arcana::cancellation_source cancel;

            arcana::manual_dispatcher<32> dis1;
            arcana::manual_dispatcher<32> dis2;

            std::string result;

            arcana::task_from_result<std::error_code, std::string>("A").then(dis2, arcana::cancellation::none(), [&](const std::string& letter) noexcept
            {
                return letter + "B";
            }).then(dis1, arcana::cancellation::none(), [&](const std::string& letter) noexcept
            {
                return letter + "C";
            }).then(dis2, arcana::cancellation::none(), [&](const std::string& res) noexcept { result = res; });

            while (dis2.tick(cancel) || dis1.tick(cancel)) {};

            Assert::AreEqual<std::string>("ABC", result);
        }

        TEST_METHOD(TaskReturningTask)
        {
            arcana::cancellation_source cancel;

            arcana::manual_dispatcher<32> dis1;
            arcana::manual_dispatcher<32> dis2;

            std::shared_ptr<int> shared = std::make_shared<int>(10);
            std::weak_ptr<int> weak = shared;

            std::string result;

            arcana::task_from_result<std::error_code, std::string>("A")
                .then(dis2, arcana::cancellation::none(), [&, shared](const std::string& letter) noexcept
                {
                    return make_task(dis1, arcana::cancellation::none(), [letter = letter + "B", shared]() noexcept
                    {
                        return letter + "C";
                    });
                })
                .then(dis2, arcana::cancellation::none(), [&, shared](const std::string& res) noexcept
                {
                    result = res;
                });

            shared.reset();

            while (dis2.tick(cancel) || dis1.tick(cancel)) {};

            Assert::AreEqual<std::string>("ABC", result);
            Assert::AreEqual(true, weak.expired());
        }

        TEST_METHOD(DifferentSizedTasks)
        {
            arcana::cancellation_source cancel;

            arcana::manual_dispatcher<32> dis1;
            arcana::manual_dispatcher<32> dis2;

            std::shared_ptr<int> shared = std::make_shared<int>(10);
            std::weak_ptr<int> weak = shared;

            std::string result;

            std::array<char, 30> large;
            large.back() = 'B';

            arcana::task_from_result<std::error_code, std::string>("A")
                .then(dis2, arcana::cancellation::none(), [&, shared](const std::string& letter) noexcept
                {
                    return make_task(dis1, arcana::cancellation::none(), [letter, large, shared]() noexcept
                    {
                        return letter + large.back();
                    });
                }).then(dis2, arcana::cancellation::none(), [&, letter = std::string{ "C" }](const std::string& res) noexcept
                {
                    result = res + letter;
                });

            shared.reset();

            while (dis2.tick(cancel) || dis1.tick(cancel)) {};

            Assert::AreEqual<std::string>("ABC", result);
            Assert::AreEqual(true, weak.expired());
        }

        TEST_METHOD(InlineContinuation)
        {
            arcana::cancellation_source cancel;

            arcana::manual_dispatcher<32> dis1;

            int runs = 0;
            auto task = make_task(dis1, arcana::cancellation::none(), [&]() noexcept
            {
                runs++;
            }).then(arcana::inline_scheduler, arcana::cancellation::none(), [&]() noexcept
            {
                runs++;
            });

            dis1.tick(cancel);

            Assert::AreEqual(2, runs);
        }

        TEST_METHOD(WhenAll)
        {
            arcana::background_dispatcher<32> dis1;
            arcana::background_dispatcher<32> dis2;

            std::promise<int> work;

            std::stringstream ss;

            int a = 0, b = 0, c = 0;

            std::vector<arcana::task<void, std::error_code>> tasks;

            tasks.push_back(make_task(dis1, arcana::cancellation::none(), [&]() noexcept
            {
                c = 3;
            }));
            tasks.push_back(make_task(dis2, arcana::cancellation::none(), [&]() noexcept
            {
                b = 2;
            }));
            tasks.push_back(make_task(dis1, arcana::cancellation::none(), [&]() noexcept
            {
                a = 1;
            }));

            arcana::when_all(gsl::make_span(tasks)).then(dis1, arcana::cancellation::none(), [&]() noexcept
            {
                work.set_value(a + b + c);
            });

            Assert::AreEqual(6, work.get_future().get());
        }

        TEST_METHOD(WhenAllWithExceptions)
        {
            std::promise<int> work;

            arcana::when_all(gsl::span<arcana::task<void, std::exception_ptr>>{}).then(arcana::inline_scheduler, arcana::cancellation::none(), [&]()
            {
                work.set_value(6);
            });

            Assert::AreEqual(6, work.get_future().get());
        }

        TEST_METHOD(EmptyWhenAll)
        {
            arcana::background_dispatcher<32> dis1;

            std::promise<int> work;

            arcana::when_all(gsl::span<arcana::task<void, std::error_code>>{}).then(dis1, arcana::cancellation::none(), [&]() noexcept
            {
                work.set_value(6);
            });

            Assert::AreEqual(6, work.get_future().get());
        }

        TEST_METHOD(WhenAllVariadicWithVoid)
        {
            arcana::background_dispatcher<32> dis1;
            std::promise<int> work;

            arcana::task<void, std::error_code> t1 = make_task(dis1, arcana::cancellation::none(), []() noexcept
            {
            });

            arcana::task<int, std::error_code> t2 = make_task(dis1, arcana::cancellation::none(), []() noexcept
            {
                return 5;
            });

            arcana::task<void, std::error_code> t3 = make_task(dis1, arcana::cancellation::none(), []() noexcept
            {
            });

            arcana::when_all(t1, t2, t3).then(dis1, arcana::cancellation::none(),
                [&](const std::tuple<arcana::void_placeholder, int, arcana::void_placeholder>& args) noexcept
                {
                    work.set_value(std::get<1>(args));
                });

            Assert::AreEqual(5, work.get_future().get());
        }

        TEST_METHOD(SynchronousPendingTaskScope)
        {
            arcana::pending_task_scope<std::error_code> scope;

            scope += arcana::task_from_result<std::error_code>();

            Assert::IsTrue(scope.completed());
        }

        TEST_METHOD(SynchronousPendingTaskScope_WhenAll)
        {
            arcana::pending_task_scope<std::error_code> scope;

            scope += arcana::task_from_result<std::error_code>();

            bool didRun = false;
            scope.when_all().then(arcana::inline_scheduler, arcana::cancellation::none(), [&]() noexcept
            {
                didRun = true;
            });

            Assert::IsTrue(scope.completed());
            Assert::IsTrue(didRun);
        }

        TEST_METHOD(PendingTaskScopeCompletionOrder)
        {
            arcana::pending_task_scope<std::error_code> scope;

            arcana::manual_dispatcher<32> dis1;

            int result = 0;
            auto work = make_task(dis1, arcana::cancellation::none(), [&]() noexcept
            {
                result = 10;
            });

            work.then(arcana::inline_scheduler, arcana::cancellation::none(), [&]() noexcept
            {
                return scope.when_all().then(arcana::inline_scheduler, arcana::cancellation::none(), [&]() noexcept
                {
                    Assert::IsTrue(scope.completed(), L"a continuation on a scope when_all should guarantee that the scope is done");
                });
            });

            scope += work;

            while (dis1.tick(arcana::cancellation::none())) {};

            Assert::IsTrue(scope.completed());
        }

        TEST_METHOD(PendingTaskScopeBubbleError)
        {
            arcana::pending_task_scope<std::error_code> scope;
            const auto error = std::make_error_code(std::errc::owner_dead);
            scope += arcana::task_from_error<void>(error);
            Assert::IsTrue(scope.completed());
            Assert::IsTrue(scope.has_error());
            Assert::IsTrue(scope.error() == error);

            bool taskComplete = false;
            const auto task = scope.when_all().then(arcana::inline_scheduler, arcana::cancellation::none(), [&](arcana::expected<void, std::error_code> previous) noexcept
            {
                taskComplete = true;
                Assert::IsTrue(previous.has_error());
                Assert::IsTrue(previous.error() == error);
            });

            Assert::IsTrue(taskComplete);
        }

        TEST_METHOD(LastMethodAlwaysRuns)
        {
            arcana::cancellation_source cancel;

            arcana::manual_dispatcher<32> dis1;
            arcana::manual_dispatcher<32> dis2;

            int result = -1;

            bool wasCalled = false;

            arcana::task_from_result<std::error_code>(10)
                .then(dis2, arcana::cancellation::none(), [&](int value) noexcept
                {
                    return value;
                }).then(dis2, arcana::cancellation::none(), [&](int /*value*/) noexcept -> arcana::expected<int, std::error_code>
                {
                    return arcana::make_unexpected(std::errc::operation_canceled);
                }).then(dis1, arcana::cancellation::none(), [&](int /*value*/) noexcept
                {
                    wasCalled = true;
                }).then(dis2, arcana::cancellation::none(), [&](const arcana::expected<void, std::error_code>& value) noexcept
                {
                    Assert::IsTrue(value.has_error());
                    result = 15;
                });

            while (dis2.tick(cancel) || dis1.tick(cancel)) {};

            Assert::IsFalse(wasCalled, L"This method shouldn't run because it doesn't care about errors");
            Assert::AreEqual(15, result);
        }

        TEST_METHOD(AutomaticCancellation)
        {
            arcana::cancellation_source cancel;
            arcana::cancellation_source global;

            arcana::manual_dispatcher<32> dis1;
            arcana::manual_dispatcher<32> dis2;

            int hitCount = 0;
            bool wasCalled1 = false, wasCalled2 = false;

            arcana::task_from_result<std::error_code>(10)
                .then(dis2, cancel, [&](int value) noexcept
                {
                    hitCount++;
                    return 2 * value;
                }).then(dis2, cancel, [&](int value) noexcept
                {
                    hitCount++;
                    wasCalled1 = true;
                    return value + 5;
                }).then(dis1, cancel, [&](int /*value*/) noexcept
                {
                    hitCount++;
                    wasCalled2 = true;
                }).then(dis2, arcana::cancellation::none(), [&](const arcana::expected<void, std::error_code>& value) noexcept
                {
                    hitCount++;
                    Assert::IsTrue(value.has_error() && value.error() == std::errc::operation_canceled);
                });

            dis2.tick(global);

            cancel.cancel();

            while (dis2.tick(global) || dis1.tick(global)) {};

            Assert::IsFalse(wasCalled1, L"This method shouldn't run");
            Assert::IsFalse(wasCalled2, L"This method shouldn't run");
            Assert::AreEqual(2, hitCount);
        }

        TEST_METHOD(CancellationOrder_IsReverseOfOrderAdded)
        {
            arcana::cancellation_source root;

            int hitCount = 0;

            auto tick1 = root.add_listener([&]() noexcept
            {
                Assert::AreEqual(1, hitCount);
                hitCount++;
            });

            auto tick2 = root.add_listener([&]() noexcept
            {
                Assert::AreEqual(0, hitCount);
                hitCount++;
            });

            root.cancel();

            Assert::AreEqual(2, hitCount);
        }

        TEST_METHOD(IfErrorThenCancelationReturnError)
        {
            arcana::cancellation_source cancel;
            arcana::cancellation_source global;

            arcana::manual_dispatcher<32> dis1;
            arcana::manual_dispatcher<32> dis2;

            int hitCount = 0;
            bool wasCalled1 = false, wasCalled2 = false;

            arcana::task_from_result<std::error_code>(10)
                .then(dis2, cancel, [&](int /*value*/) noexcept
                {
                    hitCount++;
                    return arcana::expected<int, std::error_code>{ arcana::make_unexpected(std::errc::bad_message) };
                }).then(dis2, cancel, [&](int value) noexcept
                {
                    hitCount++;
                    wasCalled1 = true;
                    return value + 5;
                }).then(dis1, cancel, [&](int /*value*/) noexcept
                {
                    hitCount++;
                    wasCalled2 = true;
                }).then(dis2, arcana::cancellation::none(), [&](const arcana::expected<void, std::error_code>& value) noexcept
                {
                    hitCount++;
                    Assert::IsTrue(value.has_error() && value.error() == std::errc::bad_message);
                });

            dis2.tick(global);

            cancel.cancel();

            while (dis2.tick(global) || dis1.tick(global)) {};

            Assert::IsFalse(wasCalled1, L"This method shouldn't run");
            Assert::IsFalse(wasCalled2, L"This method shouldn't run");
            Assert::AreEqual(2, hitCount);
        }

        TEST_METHOD(ExpectedToValueConversion)
        {
            arcana::cancellation_source cancel;

            arcana::manual_dispatcher<32> dis1;
            arcana::manual_dispatcher<32> dis2;

            int result = -1;

            arcana::task_from_result<std::error_code>(10)
                .then(dis2, arcana::cancellation::none(), [&](int value) noexcept
                {
                    return value;
                }).then(dis2, arcana::cancellation::none(), [&](int value) noexcept -> arcana::expected<int, std::error_code>
                {
                    return value;
                }).then(dis2, arcana::cancellation::none(), [&](const arcana::expected<int, std::error_code>& value) noexcept -> arcana::expected<int, std::error_code>
                {
                    return value.value() + 5;
                }).then(dis1, arcana::cancellation::none(), [&](int value) noexcept
                {
                    result = value;
                });

            while (dis2.tick(cancel) || dis1.tick(cancel)) {};

            Assert::AreEqual(15, result);
        }

        TEST_METHOD(ErrorCodeTasks)
        {
            arcana::cancellation_source cancel;

            arcana::manual_dispatcher<32> dis1;
            arcana::manual_dispatcher<32> dis2;

            arcana::expected<int, std::error_code> result{ arcana::make_unexpected(std::errc::broken_pipe) };

            arcana::task_from_result<std::error_code>(10)
                .then(dis2, arcana::cancellation::none(), [&](int value) noexcept -> arcana::task<int, std::error_code>
                {
                    return make_task(dis1, arcana::cancellation::none(), [value]() noexcept -> arcana::expected<int, std::error_code>
                    {
                        return value + 1;
                    });
                }).then(dis2, arcana::cancellation::none(), [&](const arcana::expected<int, std::error_code>& value) noexcept -> arcana::expected<int, std::error_code>
                {
                    if (!value)
                        return -1;

                    return 10;
                }).then(dis1, arcana::cancellation::none(), [&](const arcana::expected<int, std::error_code>& value) noexcept
                {
                    result = value;
                });

            while (dis2.tick(cancel) || dis1.tick(cancel)) {};

            Assert::IsTrue(result.has_value());
            Assert::AreEqual(10, result.value());

            arcana::task_from_result<std::error_code>(10)
                .then(dis2, arcana::cancellation::none(), [&](int value) noexcept
            {
                return make_task(dis1, arcana::cancellation::none(), [value]() noexcept -> arcana::expected<int, std::error_code>
                {
                    return value + 1;
                });
            }).then(dis2, arcana::cancellation::none(), [&](const arcana::expected<int, std::error_code>& value) noexcept -> arcana::expected<int, std::error_code>
            {
                if (!value || value.value() > 10)
                    return arcana::make_unexpected(std::errc::invalid_argument);

                return value;
            }).then(dis1, arcana::cancellation::none(), [&](const arcana::expected<int, std::error_code>& value) noexcept
            {
                result = value;
            });

            while (dis2.tick(cancel) || dis1.tick(cancel)) {};

            Assert::IsFalse(result.has_value());
            Assert::IsTrue(result.error() == std::errc::invalid_argument);
        }

        // This test fails to compile if task chaining with ErrorT=std::exception_ptr is broken.
        // The true test is whether this compiles; running it doesn't tell you anything useful.
        TEST_METHOD(ChainingTasksWithExceptionPtr)
        {
            arcana::task_from_result<std::exception_ptr>().then(arcana::inline_scheduler, arcana::cancellation::none(), []
            {
                return arcana::task_from_result<std::exception_ptr>();
            });
        }

        TEST_METHOD(ChainingTasksAndExpecteds)
        {
            arcana::cancellation_source cancel;
            arcana::manual_dispatcher<32> dis1;

            int hit = 0;

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4702) // Unreachable code
#endif
            arcana::task_from_result<std::error_code>(std::make_shared<const int>(10))
                .then(dis1, arcana::cancellation::none(), [&](const std::shared_ptr<const int>& i) noexcept
                {
                    hit++;
                    return make_task(dis1, arcana::cancellation::none(), [&, i]() noexcept -> arcana::expected<std::shared_ptr<const double>, std::error_code>
                    {
                        hit++;
                        return arcana::make_unexpected(std::errc::operation_canceled);
                    });
            }).then(dis1, arcana::cancellation::none(), [&](const std::shared_ptr<const double>&) noexcept
            {
                hit++;
                Assert::Fail(L"This should not have run");
                return 10;
            }).then(dis1, arcana::cancellation::none(), [&](const arcana::expected<int, std::error_code>& something) noexcept
            {
                hit++;
                Assert::IsTrue(std::errc::operation_canceled == something.error());
            });
#ifdef _MSC_VER
#pragma warning(pop)
#endif

            while (dis1.tick(cancel)) {};

            Assert::AreEqual(3, hit);
        }

        TEST_METHOD(ChainingTasksAndConvertingToExpecteds)
        {
            arcana::cancellation_source cancel;
            arcana::manual_dispatcher<32> dis1;

            int hit = 0;

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4702) // Unreachable code
#endif
            arcana::task_from_result<std::error_code>(std::make_shared<const int>(10))
                .then(dis1, arcana::cancellation::none(), [&](const std::shared_ptr<const int>& i) noexcept
                {
                    hit++;
                    return make_task(dis1, arcana::cancellation::none(), [&, i]() noexcept -> arcana::expected<std::shared_ptr<const double>, std::error_code>
                    {
                        hit++;
                        return arcana::make_unexpected(std::errc::operation_canceled);
                    });
                }).then(dis1, arcana::cancellation::none(), [&](const std::shared_ptr<const double>&) noexcept
                {
                    hit++;
                    Assert::Fail(L"This should not have run");
                    return 10;
                }).then(dis1, arcana::cancellation::none(), [&](const arcana::expected<int, std::error_code>& something) noexcept
                {
                    hit++;
                    Assert::IsTrue(std::errc::operation_canceled == something.error());
                });
#ifdef _MSC_VER
#pragma warning(pop)
#endif

            while (dis1.tick(cancel)) {};

            Assert::AreEqual(3, hit);
        }

        TEST_METHOD(ChainingTasksAndTryingToGetAroundExpecteds)
        {
            arcana::cancellation_source cancel;
            arcana::manual_dispatcher<32> dis1;

            int hit = 0;

            arcana::task_from_error<std::shared_ptr<const int>>(std::errc::operation_canceled)
                .then(dis1, arcana::cancellation::none(), [&](const arcana::expected<std::shared_ptr<const int>, std::error_code>& i) noexcept
                {
                    hit++;

                    // Here we ignore the prior error, and we just schedule a task
                    // which means all other tasks are going to execute as normal.
                    // This is kindof a gotcha, but if you're taking an expected it's
                    // your job to propagate it.
                    return make_task(dis1, arcana::cancellation::none(), [&, i]() noexcept -> std::shared_ptr<const double>
                    {
                        hit++;
                        return std::make_shared<const double>();
                    });
                }).then(dis1, arcana::cancellation::none(), [&](const std::shared_ptr<const double>&) noexcept
                {
                    hit++;

                    return 10;
                }).then(dis1, arcana::cancellation::none(), [&](const arcana::expected<int, std::error_code>& something) noexcept
                {
                    hit++;
                    Assert::IsTrue(something.has_value());
                });

            while (dis1.tick(cancel)) {};

            Assert::AreEqual(4, hit);
        }

        TEST_METHOD(ChainingTasksAndExpectedsOnError)
        {
            arcana::cancellation_source cancel;
            arcana::manual_dispatcher<32> dis1;

            int hit = 0;

            arcana::task_from_error<std::shared_ptr<const int>>(std::errc::operation_canceled)
                .then(dis1, arcana::cancellation::none(), [&](const arcana::expected<std::shared_ptr<const int>, std::error_code>& i) noexcept
                {
                    hit++;

                    if (i.has_error())
                    {
                        return arcana::task_from_result<std::error_code>(
                            arcana::expected<std::shared_ptr<const double>, std::error_code>{arcana::make_unexpected(i.error())});
                    }

                    return make_task(dis1, arcana::cancellation::none(), [&, i]() noexcept -> std::shared_ptr<const double>
                    {
                        hit++;
                        return std::make_shared<const double>();
                    });
                }).then(dis1, arcana::cancellation::none(), [&](const std::shared_ptr<const double>&) noexcept
                {
                    hit++;

                    return 10;
                }).then(dis1, arcana::cancellation::none(), [&](const arcana::expected<int, std::error_code>& something) noexcept
                {
                    hit++;
                    Assert::IsTrue(something.has_error());
                });

            while (dis1.tick(cancel)) {};

            Assert::AreEqual(2, hit);
        }

        TEST_METHOD(CancellingTasks)
        {
            arcana::cancellation_source cancel;

            arcana::manual_dispatcher<32> dis1;
            arcana::manual_dispatcher<32> dis2;

            arcana::expected<int, std::error_code> result{ arcana::make_unexpected(std::errc::broken_pipe) };

            arcana::task_from_result<std::error_code>(10)
                .then(dis2, arcana::cancellation::none(), [&](int value) noexcept
                {
                    return make_task(dis1, arcana::cancellation::none(), [value]() noexcept -> arcana::expected<int, std::error_code>
                    {
                        return value + 1;
                    });
                }).then(dis2, arcana::cancellation::none(), [&](const arcana::expected<int, std::error_code>& /*value*/) noexcept -> arcana::expected<int, std::error_code>
                {
                    return 10;
                }).then(dis1, arcana::cancellation::none(), [&](const arcana::expected<int, std::error_code>& value) noexcept
                {
                    result = value;
                });

            while (dis2.tick(cancel) || dis1.tick(cancel)) {};

            Assert::IsTrue(result.has_value());
            Assert::AreEqual(10, result.value());

            arcana::task_from_result<std::error_code>(10)
                .then(dis2, arcana::cancellation::none(), [&](int value) noexcept
            {
                return make_task(dis1, arcana::cancellation::none(), [value]() noexcept -> arcana::expected<int, std::error_code>
                {
                    return value + 1;
                });
            }).then(dis2, arcana::cancellation::none(), [&](const arcana::expected<int, std::error_code>& value) noexcept -> arcana::expected<int, std::error_code>
            {
                if (!value || value.value() > 10)
                    return arcana::make_unexpected(std::errc::invalid_argument);

                return value;
            }).then(dis1, arcana::cancellation::none(), [&](const arcana::expected<int, std::error_code>& value) noexcept -> arcana::expected<void, std::error_code>
            {
                result = value;

                return arcana::expected<void, std::error_code>::make_valid();
            });

            while (dis2.tick(cancel) || dis1.tick(cancel)) {};

            Assert::IsFalse(result.has_value());
            Assert::IsTrue(result.error() == std::errc::invalid_argument);
        }

        TEST_METHOD(WhenAllResults)
        {
            // lets do 16 / 8

            arcana::manual_dispatcher<32> dis1;
            arcana::manual_dispatcher<32> dis2;

            int result{};

            auto sixteen = arcana::task_from_result<std::error_code>(16);
            auto eight = arcana::task_from_result<std::error_code>(8);

            when_all(sixteen, eight).then(dis1, arcana::cancellation::none(), [](const std::tuple<int, int>& values) noexcept
            {
                return std::get<0>(values) / std::get<1>(values);
            }).then(dis2, arcana::cancellation::none(), [&](int value) noexcept
            {
                result = value;
            });

            while (dis2.tick(arcana::cancellation::none()) || dis1.tick(arcana::cancellation::none())) {};

            Assert::AreEqual(2, result);
        }

        TEST_METHOD(MultipleWhenAlls)
        {
            // lets do 10 * (4 + 16 / 8)

            arcana::manual_dispatcher<32> dis1;
            arcana::manual_dispatcher<32> dis2;

            int result{};

            auto four = arcana::task_from_result<std::error_code>(4);
            auto sixteen = arcana::task_from_result<std::error_code>(16);
            auto eight = arcana::task_from_result<std::error_code>(8);

            auto div = when_all(sixteen, eight).then(dis2, arcana::cancellation::none(), [](const std::tuple<int, int>& value) noexcept
            {
                return std::get<0>(value) / std::get<1>(value);
            });

            auto sum = when_all(four, div).then(dis1, arcana::cancellation::none(), [](const std::tuple<int, int>& value) noexcept
            {
                return std::get<0>(value) + std::get<1>(value);
            });

            auto mul = arcana::when_all(arcana::task_from_result<std::error_code>(10), sum)
                .then(dis1, arcana::cancellation::none(), [](const std::tuple<int, int>& value) noexcept
                {
                    return std::get<0>(value) * std::get<1>(value);
                });

            mul.then(dis2, arcana::cancellation::none(), [&](int value) noexcept
            {
                result = value;
            });

            while (dis2.tick(arcana::cancellation::none()) || dis1.tick(arcana::cancellation::none())) {};

            Assert::AreEqual(60, result);
        }

        TEST_METHOD(DifferentWhenAllTypes)
        {
            arcana::manual_dispatcher<32> dis1;
            arcana::manual_dispatcher<32> dis2;

            std::string result;

            auto repetitions = arcana::task_from_result<std::error_code>(3);
            auto word = arcana::task_from_result<std::error_code, std::string>("Snaaaaaaake");

            auto div = when_all(repetitions, word).then(dis2, arcana::cancellation::none(), [](const std::tuple<int, std::string>& value) noexcept
            {
                std::stringstream stream{};
                for (int i = 0; i < std::get<0>(value); ++i)
                {
                    stream << std::get<1>(value);
                }
                return stream.str();
            }).then(dis1, arcana::cancellation::none(), [&](const std::string& value) noexcept
            {
                result = value;
            });

            while (dis2.tick(arcana::cancellation::none()) || dis1.tick(arcana::cancellation::none())) {};

            Assert::AreEqual<std::string>("SnaaaaaaakeSnaaaaaaakeSnaaaaaaake", result);
        }

        TEST_METHOD(WhenAll_StringVector)
        {
            arcana::manual_dispatcher<32> dis;

            std::string result = "";
            std::vector<arcana::task<std::string, std::error_code>> tasks
            {
                arcana::task_from_result<std::error_code, std::string>("H"),
                arcana::task_from_result<std::error_code, std::string>("e"),
                arcana::task_from_result<std::error_code, std::string>("l"),
                arcana::task_from_result<std::error_code, std::string>("l"),
                arcana::task_from_result<std::error_code, std::string>("o")
            };

            arcana::when_all(gsl::make_span(tasks))
                .then(dis, arcana::cancellation::none(), [&](const std::vector<std::string>& results) noexcept
                {
                    for (auto i = 0U; i < results.size(); ++i)
                    {
                        result += results[i];
                    }
                    return result;
                });

            while (dis.tick(arcana::cancellation::none())) {};
            Assert::AreEqual<std::string>("Hello", result);
        }

        TEST_METHOD(WhenAll_MathOperations)
        {
            // lets do 10 * (4 + 16 / 8) as the previous tests

            arcana::manual_dispatcher<32> dis1;
            arcana::manual_dispatcher<32> dis2;

            int result{};

            std::vector<arcana::task<int, std::error_code>> divisors = { arcana::task_from_result<std::error_code>(16), arcana::task_from_result<std::error_code>(8) };
            std::vector<arcana::task<int, std::error_code>> addition = { arcana::task_from_result<std::error_code>(4) };
            std::vector<arcana::task<int, std::error_code>> multipliers = { arcana::task_from_result<std::error_code>(10) };

            auto div = when_all(gsl::make_span(divisors)).then(dis2, arcana::cancellation::none(), [](const std::vector<int>& value) noexcept
            {
                return value.at(0) / value.at(1);
            });
            addition.push_back(div); //append answer

            auto sum = when_all(gsl::make_span(addition)).then(dis1, arcana::cancellation::none(), [](const std::vector<int>& value) noexcept
            {
                return value.at(0) + value.at(1);
            });
            multipliers.push_back(sum); //append answer

            auto mul = arcana::when_all(gsl::make_span(multipliers)).then(dis1, arcana::cancellation::none(), [](const std::vector<int>& value) noexcept
            {
                return value.at(0) * value.at(1);
            });

            mul.then(dis2, arcana::cancellation::none(), [&](int value) noexcept
            {
                result = value;
            });

            while (dis2.tick(arcana::cancellation::none()) || dis1.tick(arcana::cancellation::none())) {};
            Assert::AreEqual(60, result);
        }

        TEST_METHOD(WhenAll_BooleanValues)
        {
            arcana::manual_dispatcher<32> dis;

            bool result;
            std::vector<arcana::task<bool, std::error_code>> tasks
            {
                arcana::task_from_result<std::error_code>(true),
                arcana::task_from_result<std::error_code>(true),
                arcana::task_from_result<std::error_code>(true),
                arcana::task_from_result<std::error_code>(true)
            };

            arcana::when_all(gsl::make_span(tasks))
                .then(dis, arcana::cancellation::none(), [&](const std::vector<bool>& results) noexcept
            {
                result = std::all_of(begin(results), end(results), [](const bool& val){ return val; });
            });

            while (dis.tick(arcana::cancellation::none())) {};
            Assert::AreEqual<bool>(true, result);

            tasks.push_back(arcana::task_from_result<std::error_code>(false));

            arcana::when_all(gsl::make_span(tasks))
                .then(dis, arcana::cancellation::none(), [&](const std::vector<bool>& results) noexcept
                {
                    result = std::all_of(begin(results), end(results), [](const bool& val){ return val; });
                });

            while (dis.tick(arcana::cancellation::none())) {};
            Assert::AreEqual<bool>(false, result);
        }

        // this function uses most of the features of the task system.
        // - it tests multiple continuations by making each task consumed in
        // the two next tasks.
        // - tests out that when_all converts to the right tuple types
        // - matches completed tasks with non-completed tasks
        arcana::task<int, std::error_code> fibonnaci(arcana::dispatcher<32>& dis, int n)
        {
            std::vector<arcana::task<int, std::error_code>> fibtasks{ arcana::task_from_result<std::error_code>(0), arcana::task_from_result<std::error_code>(1) };

            for (int i = 2; i <= n; ++i)
            {
                auto nextfib = arcana::when_all(*(fibtasks.rbegin() + 1), *fibtasks.rbegin())
                    .then(dis, arcana::cancellation::none(), [](const std::tuple<int, int>& other) noexcept
                    {
                        return std::get<0>(other) + std::get<1>(other);
                    });

                fibtasks.emplace_back(std::move(nextfib));
            }

            return fibtasks[n];
        }

        TEST_METHOD(MultipleContinuationFibonnaci)
        {
            arcana::manual_dispatcher<32> dis1;

            arcana::task<int, std::error_code> myfib = fibonnaci(dis1, 42);

            int result{};
            myfib.then(dis1, arcana::cancellation::none(), [&](int r) noexcept
            {
                result = r;
            });

            while (dis1.tick(arcana::cancellation::none())) {};

            Assert::AreEqual(267914296, result);
        }

        arcana::task<int, std::exception_ptr> fibonnaciExceptional(arcana::dispatcher<32>& dis, int n)
        {
            std::vector<arcana::task<int, std::exception_ptr>> fibtasks{ arcana::task_from_result<std::exception_ptr>(0), arcana::task_from_result<std::exception_ptr>(1) };

            for (int i = 2; i <= n; ++i)
            {
                auto nextfib = arcana::when_all(*(fibtasks.rbegin() + 1), *fibtasks.rbegin())
                    .then(dis, arcana::cancellation::none(), [](const std::tuple<int, int>& other)
                {
                    return std::get<0>(other) + std::get<1>(other);
                });

                fibtasks.emplace_back(std::move(nextfib));
            }

            return fibtasks[n];
        }

        TEST_METHOD(MultipleContinuationFibonnaciExceptional)
        {
            arcana::manual_dispatcher<32> dis1;

            arcana::task<int, std::exception_ptr> myfib = fibonnaciExceptional(dis1, 42);

            int result{};
            myfib.then(dis1, arcana::cancellation::none(), [&](int r)
            {
                result = r;
            });

            while (dis1.tick(arcana::cancellation::none())) {};

            Assert::AreEqual(267914296, result);
        }

        TEST_METHOD(CancellationStackBuster)
        {
            arcana::task_completion_source<void, std::error_code> signal{};

            arcana::cancellation_source cancellation;

            std::vector<int> depth;

            arcana::task<int, std::error_code> parent = signal.as_task().then(arcana::inline_scheduler, cancellation, []() noexcept
            {
                return -1;
            });

            for (int d = 0; d < 200; ++d)
            {
                parent = parent.then(arcana::inline_scheduler, cancellation, [&, d](int /*old*/) noexcept
                {
                    depth.push_back(d);

                    return d;
                });
            }

            cancellation.cancel();

            signal.complete();
        }

        static arcana::task<void, std::error_code> CreateNestedTaskChain(size_t depth)
        {
            if (depth == 0)
                return arcana::task_from_result<std::error_code>();

            return arcana::make_task(arcana::inline_scheduler, arcana::cancellation::none(), [depth]() noexcept
            {
                return CreateNestedTaskChain(depth - 1);
            });
        }

        TEST_METHOD(LargeNestedSetOfStasks)
        {
            arcana::task_completion_source<void, std::error_code> signal{};

            std::vector<int> depth;

            arcana::task<void, std::error_code> parent = signal.as_task().then(arcana::inline_scheduler, arcana::cancellation::none(), []() noexcept
            {
                return CreateNestedTaskChain(200);
            });

            bool completed = false;
            parent.then(arcana::inline_scheduler, arcana::cancellation::none(), [&]() noexcept
            {
                completed = true;
            });

            signal.complete();

            Assert::IsTrue(completed);
        }

        // This test validates that continuations are properly
        // reparented when unwrapped. When not properly reparented,
        // parents would get destroyed because they were old
        // task_completion_sources and when executed the lock on
        // the weak_ptr would fail.
        TEST_METHOD(CompletionSourceOfCompletionSource)
        {
            arcana::task_completion_source<void, std::error_code> source;

            arcana::expected<void, std::error_code> result{ arcana::make_unexpected(std::errc::broken_pipe) };

            arcana::manual_dispatcher<32> background;

            {
                arcana::make_task(background, arcana::cancellation::none(), [&]() noexcept
                {
                    return source.as_task();
                }).then(background, arcana::cancellation::none(), [&](const arcana::expected<void, std::error_code>& r) noexcept
                {
                    result = r;
                });
            }

            background.tick(arcana::cancellation::none());

            source.complete(arcana::make_unexpected(std::errc::operation_canceled));

            background.tick(arcana::cancellation::none());

            Assert::IsTrue(result.has_error());
            Assert::IsTrue(result.error() == std::errc::operation_canceled);
        }

        arcana::task<void, std::error_code> RunTaskAsGenerator(arcana::manual_dispatcher<32>& background, arcana::cancellation& cancel, int& iterations)
        {
            return arcana::make_task(background, cancel, [&]() noexcept
            {
                iterations++;

                return CreateNestedTaskChain(10).then(arcana::inline_scheduler, arcana::cancellation::none(), [&]() noexcept
                {
                    return RunTaskAsGenerator(background, cancel, iterations);
                });
            });
        }

        TEST_METHOD(GenerateLotsOfTasksRecursively)
        {
            bool completed = false;

            arcana::manual_dispatcher<32> background;

            arcana::cancellation_source cancel;

            arcana::expected<void, std::error_code> result{ arcana::make_unexpected(std::errc::broken_pipe) };

            int iterations = 0;

            {
                arcana::task<void, std::error_code> parent = RunTaskAsGenerator(background, cancel, iterations);

                parent.then(arcana::inline_scheduler, arcana::cancellation::none(), [&](const arcana::expected<void, std::error_code>& r) noexcept
                {
                    completed = true;
                    result = r;
                });
            }

            int count = 1000;
            while (count > 0)
            {
                count--;
                background.tick(arcana::cancellation::none());
            }

            cancel.cancel();

            while (background.tick(arcana::cancellation::none()));

            Assert::IsTrue(completed, L"The chain hasn't completed properly");
            Assert::AreEqual(1000, iterations, L"The chain hasn't completed properly");
            Assert::IsTrue(result.has_error(), L"The result should have completed through cancellation");
            Assert::IsTrue(result.error() == std::errc::operation_canceled, L"The result should have completed through cancellation");
        }

        TEST_METHOD(NestedTaskChain)
        {
            auto task = CreateNestedTaskChain(1);

            bool completed = false;

            task.then(arcana::inline_scheduler, arcana::cancellation::none(), [&](const arcana::expected<void, std::error_code>&) noexcept
            {
                completed = true;
            });

            Assert::IsTrue(completed, L"The chain hasn't completed properly");
        }

        template<
            typename TaskInputT, 
            bool HandlesExpected,
            typename ActualInputType,
            typename ReturnT,
            bool NoThrowInvocable,
            typename ErrorPropagationT,
            typename ExpectedReturnT>
        struct expected_callable_traits
        {
            using task_input_type = TaskInputT;
            using handles_expected = std::bool_constant<HandlesExpected>;
            using input_type = ActualInputType;
            using return_type = ReturnT;
            using is_nothrow_invocable = std::bool_constant<NoThrowInvocable>;
            using error_propagation_type = ErrorPropagationT;
            using expected_return_type = ExpectedReturnT;
        };

        template<typename ExpectedT, typename ReceivedT>
        static void ValidateCallableTraits()
        {
            Assert::AreEqual(typeid(ExpectedT::task_input_type).name(), typeid(ReceivedT::task_input_type).name(), L"Bad raw_input_type");
            Assert::AreEqual(typeid(ExpectedT::handles_expected).name(), typeid(ReceivedT::handles_expected).name(), L"Bad handles_expected");
            Assert::AreEqual(typeid(ExpectedT::input_type).name(), typeid(ReceivedT::input_type).name(), L"Bad input_type");
            Assert::AreEqual(typeid(ExpectedT::return_type).name(), typeid(ReceivedT::return_type).name(), L"Bad return_type");
            Assert::AreEqual(typeid(ExpectedT::is_nothrow_invocable).name(), typeid(ReceivedT::is_nothrow_invocable).name(), L"Bad is_nothrow_invocable");
            Assert::AreEqual(typeid(ExpectedT::error_propagation_type).name(), typeid(ReceivedT::error_propagation_type).name(), L"Bad error propagation type");
            Assert::AreEqual(typeid(ExpectedT::expected_return_type).name(), typeid(ReceivedT::expected_return_type).name(), L"Bad expected_return_type");
        }

        TEST_METHOD(CallableTraitsExceptionalVoidVoid)
        {
            auto callable = [](){};

            using expected = expected_callable_traits<
                void, // TaskInputT,
                false, // HandlesExpected,
                void, // ActualInputType,
                void, // ReturnT,
                false, // NoThrowInvocable,
                std::exception_ptr, // ErrorPropagationT,
                arcana::basic_expected<void, std::exception_ptr> // ExpectedReturnT
            >;

            using received = arcana::internal::callable_traits<decltype(callable), void>;

            ValidateCallableTraits<expected, received>();
        }

        TEST_METHOD(CallableTraitsExceptionalVoidInt)
        {
            auto callable = [](int) {};

            using expected = expected_callable_traits<
                int, // TaskInputT,
                false, // HandlesExpected,
                int, // ActualInputType,
                void, // ReturnT,
                false, // NoThrowInvocable,
                std::exception_ptr, // ErrorPropagationT,
                arcana::basic_expected<void, std::exception_ptr> // ExpectedReturnT
            >;

            using received = arcana::internal::callable_traits<decltype(callable), int>;

            ValidateCallableTraits<expected, received>();
        }

        TEST_METHOD(CallableTraitsExceptionalVoidExpectedInt)
        {
            auto callable = [](const arcana::basic_expected<int, std::exception_ptr>&) {};
        
            using expected = expected_callable_traits<
                int, // TaskInputT,
                true, // HandlesExpected,
                arcana::basic_expected<int, std::exception_ptr>, // ActualInputType,
                void, // ReturnT,
                false, // NoThrowInvocable,
                std::exception_ptr, // ErrorPropagationT,
                arcana::basic_expected<void, std::exception_ptr> // ExpectedReturnT
            >;
        
            using received = arcana::internal::callable_traits<decltype(callable), int>;
        
            ValidateCallableTraits<expected, received>();
        }

        TEST_METHOD(CallableTraitsExceptionalVoidValueExpectedInt)
        {
            auto callable = [](arcana::basic_expected<int, std::exception_ptr>) {};

            using expected = expected_callable_traits<
                int, // TaskInputT,
                true, // HandlesExpected,
                arcana::basic_expected<int, std::exception_ptr>, // ActualInputType,
                void, // ReturnT,
                false, // NoThrowInvocable,
                std::exception_ptr, // ErrorPropagationT,
                arcana::basic_expected<void, std::exception_ptr> // ExpectedReturnT
            >;

            using received = arcana::internal::callable_traits<decltype(callable), int>;

            ValidateCallableTraits<expected, received>();
        }

        TEST_METHOD(CallableTraitsExceptionalIntVoid)
        {
            auto callable = []() -> int { return 0; };

            using expected = expected_callable_traits<
                void, // TaskInputT,
                false, // HandlesExpected,
                void, // ActualInputType,
                int, // ReturnT,
                false, // NoThrowInvocable,
                std::exception_ptr, // ErrorPropagationT,
                arcana::basic_expected<int, std::exception_ptr> // ExpectedReturnT
            >;

            using received = arcana::internal::callable_traits<decltype(callable), void>;

            ValidateCallableTraits<expected, received>();
        }

        TEST_METHOD(CallableTraitsExceptionalIntInt)
        {
            auto callable = [](int) -> int { return 0; };

            using expected = expected_callable_traits<
                int, // TaskInputT,
                false, // HandlesExpected,
                int, // ActualInputType,
                int, // ReturnT,
                false, // NoThrowInvocable,
                std::exception_ptr, // ErrorPropagationT,
                arcana::basic_expected<int, std::exception_ptr> // ExpectedReturnT
            >;

            using received = arcana::internal::callable_traits<decltype(callable), int>;

            ValidateCallableTraits<expected, received>();
        }

        TEST_METHOD(CallableTraitsExceptionalIntExpectedInt)
        {
            auto callable = [](const arcana::basic_expected<int, std::exception_ptr>&) -> int { return 0; };

            using expected = expected_callable_traits<
                int, // TaskInputT,
                true, // HandlesExpected,
                arcana::basic_expected<int, std::exception_ptr>, // ActualInputType,
                int, // ReturnT,
                false, // NoThrowInvocable,
                std::exception_ptr, // ErrorPropagationT,
                arcana::basic_expected<int, std::exception_ptr> // ExpectedReturnT
            >;

            using received = arcana::internal::callable_traits<decltype(callable), int>;

            ValidateCallableTraits<expected, received>();
        }

        TEST_METHOD(CallableTraitsExceptionalIntToErrorCodeInt)
        {
            auto callable = [](const arcana::basic_expected<int, std::exception_ptr>&) noexcept -> arcana::basic_expected<int, std::error_code> { return 0; };

            using expected = expected_callable_traits<
                int, // TaskInputT,
                true, // HandlesExpected,
                arcana::basic_expected<int, std::exception_ptr>, // ActualInputType,
                arcana::basic_expected<int, std::error_code>, // ReturnT,
                true, // NoThrowInvocable,
                std::error_code, // ErrorPropagationT,
                arcana::basic_expected<int, std::error_code> // ExpectedReturnT
            >;

            using received = arcana::internal::callable_traits<decltype(callable), int>;

            ValidateCallableTraits<expected, received>();
        }

        TEST_METHOD(CallableTraitsExceptionalIntToErrorCodeIntNoExpected)
        {
            auto callable = [](const arcana::basic_expected<int, std::exception_ptr>&) noexcept -> int { return 0; };

            using expected = expected_callable_traits<
                int, // TaskInputT,
                true, // HandlesExpected,
                arcana::basic_expected<int, std::exception_ptr>, // ActualInputType,
                int, // ReturnT,
                true, // NoThrowInvocable,
                std::error_code, // ErrorPropagationT,
                arcana::basic_expected<int, std::error_code> // ExpectedReturnT
            >;

            using received = arcana::internal::callable_traits<decltype(callable), int>;

            ValidateCallableTraits<expected, received>();
        }

        TEST_METHOD(ErrorCodeTaskToExceptionalTask)
        {
            namespace arc = arcana;

            arc::task<void, std::error_code> code = arc::make_task(arc::inline_scheduler, arc::cancellation::none(), []() noexcept -> arc::expected<void, std::error_code>
            {
                return arcana::make_unexpected(std::errc::operation_canceled);
            });

            arc::task<void, std::exception_ptr> exc = code.then(arc::inline_scheduler, arc::cancellation::none(), []() {});

            exc.then(arc::inline_scheduler, arc::cancellation::none(), [](const arc::expected<void, std::exception_ptr>& res)
            {
                try
                {
                    std::rethrow_exception(res.error());
                }
                catch (const std::system_error& error)
                {
                    Assert::IsTrue(error.code() == std::errc::operation_canceled, L"wrong error code");

                    throw;
                }
            });

            exc.then(arc::inline_scheduler, arc::cancellation::none(), []()
            {
                throw std::logic_error("don't get hit");
            }).then(arc::inline_scheduler, arc::cancellation::none(), [](const arc::expected<void, std::exception_ptr>& res)
            {
                throw std::runtime_error("and now this");
            }).then(arc::inline_scheduler, arc::cancellation::none(), [](const arc::expected<void, std::exception_ptr>& res)
            {
                try
                {
                    std::rethrow_exception(res.error());
                    Assert::Fail(L"We should have thrown an exception");
                }
                catch (const std::runtime_error&)
                {}
                catch (...)
                {
                    Assert::Fail(L"Wrong exception type");
                }
            });
        }

        TEST_METHOD(CancellationFromTaskBody)
        {
            arcana::cancellation_source source;
            arcana::cancellation& token = source;
            source.cancel();

            auto task = arcana::make_task(arcana::inline_scheduler, arcana::cancellation::none(), [&token]
            {
                token.throw_if_cancellation_requested();
            });

            task.then(arcana::inline_scheduler, arcana::cancellation::none(), [](const arcana::expected<void, std::exception_ptr>& res)
            {
                try
                {
                    std::rethrow_exception(res.error());
                    Assert::Fail(L"Exception was not thrown.");
                }
                catch (const std::system_error& ex)
                {
                    Assert::IsTrue(ex.code() == std::errc::operation_canceled, L"Wrong error code");
                }
                catch (...)
                {
                    Assert::Fail(L"Wrong exception type");
                }
            });
        }
    };
}
