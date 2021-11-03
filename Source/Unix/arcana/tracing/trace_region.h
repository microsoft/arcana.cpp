//
// Copyright (C) Microsoft Corporation. All rights reserved.
//

#pragma once

namespace arcana
{
    // TODO
    class trace_region final
    {
    public:
        trace_region() = delete;
        trace_region(const trace_region&) = delete;
        trace_region& operator=(const trace_region&) = delete;

        trace_region(const char*)
        {
        }

        trace_region(trace_region&&) = default;

        ~trace_region()
        {
        }

        trace_region& operator=(trace_region&&) = default;

        static void enable()
        {
        }

        static void disable()
        {
        }
    };
}
