//
// Copyright (C) Microsoft Corporation. All rights reserved.
//

#include <arcana/threading/task_schedulers.h>
#include <arcana/threading/task.h>
#include <gtest/gtest.h>

#include <future>

TEST(TaskSchedulersTest, GivenMakeTask_WhenScheduledOnThreadPool_ExecutesOnDifferentThread)
{
    std::promise<std::thread::id> promise;

    const auto foregroundThreadId = std::this_thread::get_id();

    make_task(arcana::threadpool_scheduler, arcana::cancellation::none(), [&]() noexcept
    {
        promise.set_value(std::this_thread::get_id());
    });

    const auto backgroundThreadId = promise.get_future().get();

    EXPECT_NE(std::hash<std::thread::id>{}(foregroundThreadId), std::hash<std::thread::id>{}(backgroundThreadId));
}
