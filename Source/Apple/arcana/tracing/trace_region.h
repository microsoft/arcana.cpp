//
// Copyright (C) Microsoft Corporation. All rights reserved.
//

#pragma once

#include <os/signpost.h>

#define SIGNPOST_NAME "trace_region"

namespace arcana
{
    class trace_region final
    {
    public:
        trace_region() = delete;
        trace_region(const trace_region&) = delete;
        trace_region& operator=(const trace_region&) = delete;

        trace_region(const char* name) :
            m_id{s_enabled ? os_signpost_id_generate(s_log) : OS_SIGNPOST_ID_NULL}
        {
            if (m_id != OS_SIGNPOST_ID_NULL)
            {
                os_signpost_interval_begin(s_log, m_id, SIGNPOST_NAME, "%s", name);
            }
        }

        trace_region(trace_region&& other) :
            m_id{other.m_id}
        {
            other.m_id = OS_SIGNPOST_ID_NULL;
        }

        ~trace_region()
        {
            if (m_id != OS_SIGNPOST_ID_NULL)
            {
                os_signpost_interval_end(s_log, m_id, SIGNPOST_NAME);
            }
        }

        trace_region& operator=(trace_region&& other)
        {
            if (m_id != OS_SIGNPOST_ID_NULL)
            {
                os_signpost_interval_end(s_log, m_id, SIGNPOST_NAME);
            }

            m_id = other.m_id;
            other.m_id = OS_SIGNPOST_ID_NULL;

            return *this;
        }

        static void enable()
        {
            s_enabled = true;
        }

        static void disable()
        {
            s_enabled = false;
        }

    private:
        static inline std::atomic<bool> s_enabled{false};
        static inline os_log_t s_log{os_log_create("arcana", OS_LOG_CATEGORY_POINTS_OF_INTEREST)};
        os_signpost_id_t m_id;
    };
}
