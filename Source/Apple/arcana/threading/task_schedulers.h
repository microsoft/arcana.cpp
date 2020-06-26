//
// Copyright (C) Microsoft Corporation. All rights reserved.
//

#pragma once

#include <CoreFoundation/CFRunLoop.h>

#include <thread>

namespace arcana
{
    // NOTE: These task schedulers are for the arcana task system.

    namespace
    {
        // TOOD: this is a stop gap for platforms that don't have a threadpool implementation
        constexpr struct
        {
            template<typename CallableT>
            void operator()(CallableT&& callable) const
            {
                std::thread([callable{ std::forward<CallableT>(callable) }]() { callable(); }).detach();
            }
        } threadpool_scheduler{};
    }

    class run_loop_scheduler final
    {
    public:
        explicit run_loop_scheduler(CFRunLoopRef runLoop)
            : m_runLoop{ runLoop }
        {
            CFRetain(m_runLoop);
        }

        ~run_loop_scheduler()
        {
            CFRelease(m_runLoop);
        }

        template<typename CallableT>
        void operator()(CallableT&& callable) const
        {
            CallableT _callable{ std::forward<CallableT>(callable) };
            CFRunLoopPerformBlock(m_runLoop, kCFRunLoopCommonModes, ^{
                _callable();
            });
            
            // In case the run loop is idle, we need to wake it up and drain the queue
            CFRunLoopWakeUp(m_runLoop);
        }

        static run_loop_scheduler get_for_current_thread()
        {
            CFRunLoopRef runLoop = CFRunLoopGetCurrent();
            if (runLoop == nullptr)
            {
                throw std::runtime_error("No run loop associated with the current thread");
            }

            return run_loop_scheduler{ std::move(runLoop) };
        }

    private:
        CFRunLoopRef m_runLoop{};
    };
}
