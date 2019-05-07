//
// Copyright (C) Microsoft Corporation. All rights reserved.
//

#include <arcana/threading/task_schedulers.h>
#include <arcana/threading/task.h>
#include <CppUnitTest.h>

#include <future>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace UnitTests
{
    TEST_CLASS(TaskSchedulersTest)
    {
    public:
        TEST_METHOD(GivenMakeTask_WhenScheduledOnThreadPool_ExecutesOnDifferentThread)
        {
            std::promise<std::thread::id> promise;

            const auto foregroundThreadId = std::this_thread::get_id();

            make_task(arcana::threadpool_scheduler, arcana::cancellation::none(), [&]() noexcept
            {
                promise.set_value(std::this_thread::get_id());
            });

            const auto backgroundThreadId = promise.get_future().get();

            Assert::AreNotEqual(std::hash<std::thread::id>{}(foregroundThreadId), std::hash<std::thread::id>{}(backgroundThreadId));
        }
    };
}
