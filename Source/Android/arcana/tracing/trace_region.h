//
// Copyright (C) Microsoft Corporation. All rights reserved.
//

#pragma once

namespace arcana
{
    // TODO: https://developer.android.com/topic/performance/tracing/custom-events-native
    //       https://developer.android.com/ndk/reference/group/tracing
    class trace_region final
    {
    public:
        trace_region() = delete;
        trace_region& operator=(const trace_region&) = delete;
        trace_region(const trace_region&) = delete;
        trace_region& operator=(trace_region&&) = delete;

        trace_region(const char*)
        {
        }

        trace_region(trace_region&& other)
        {
        }

        ~trace_region()
        {
        }

        static void enable()
        {
        }

        static void disable()
        {
        }
    };
}
