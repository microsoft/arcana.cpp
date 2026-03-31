//
// Copyright (C) Microsoft Corporation. All rights reserved.
//

#pragma once

#include <atomic>
#include <android/trace.h>
#include <android/log.h>

#define TRACE_TAG "trace_region"

namespace arcana
{
    enum class trace_level
    {
        mark,
        log,
    };

    class trace_region final
    {
    public:
        trace_region() = delete;
        trace_region(const trace_region&) = delete;
        trace_region& operator=(const trace_region&) = delete;

        trace_region(const char* name) :
                m_active{s_enabled.load()}
        {
            if (m_active)
            {
                if (s_logEnabled)
                {
                    __android_log_print(ANDROID_LOG_DEBUG, TRACE_TAG, "[trace_region] BEGIN %s (this=%p)", name, static_cast<const void*>(this));
                }
                ATrace_beginSection(name);
            }
        }

        trace_region(trace_region&& other) :
                m_active{other.m_active}
        {
            other.m_active = false;
        }

        ~trace_region()
        {
            if (m_active)
            {
                if (s_logEnabled)
                {
                    __android_log_print(ANDROID_LOG_DEBUG, TRACE_TAG, "[trace_region] END (this=%p)", static_cast<const void*>(this));
                }
                ATrace_endSection();
            }
        }

        trace_region& operator=(trace_region&& other)
        {
            if (m_active)
            {
                if (s_logEnabled)
                {
                    __android_log_print(ANDROID_LOG_DEBUG, TRACE_TAG, "[trace_region] END (move) (this=%p)", static_cast<const void*>(this));
                }
                ATrace_endSection();
            }

            m_active = other.m_active;
            other.m_active = false;

            return *this;
        }

        static void enable(trace_level level = trace_level::mark)
        {
            s_enabled = true;
            s_logEnabled = level == trace_level::log;
        }

        static void disable()
        {
            s_enabled = false;
            s_logEnabled = false;
        }

    private:
        static inline std::atomic<bool> s_enabled{false};
        static inline std::atomic<bool> s_logEnabled{false};
        bool m_active;
    };
}
