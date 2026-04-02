//
// Copyright (C) Microsoft Corporation. All rights reserved.
//

//
// Apple trace_region implementation using os_signpost.
//
// Signpost intervals are logged to the OS_LOG_CATEGORY_POINTS_OF_INTEREST category,
// which makes them appear on the "Points of Interest" timeline in Instruments.
// Each interval is identified by a unique os_signpost_id_t, allowing begin/end
// pairs to be matched even across threads or when multiple regions overlap.
//
// To capture and view traces in Instruments:
//   1. Open Instruments (Xcode → Open Developer Tool → Instruments, or ⌘I from Xcode)
//   2. Choose the "Blank" template, then add the "os_signpost" instrument
//      (click "+", search for "os_signpost", and add it)
//   3. Select your app as the target and click Record
//   4. Interact with the app, then stop recording
//   5. Trace regions appear on the "Points of Interest" timeline as labeled intervals
//   6. Click on an interval to see its name and duration in the detail pane
//

#pragma once

#include <atomic>
#include <os/signpost.h>
#include <os/log.h>

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
            m_id{s_enabled ? os_signpost_id_generate(s_log) : OS_SIGNPOST_ID_NULL}
        {
            if (m_id != OS_SIGNPOST_ID_NULL)
            {
                if (s_logEnabled)
                {
                    os_log_debug(s_log, "[trace_region] BEGIN %s (id=%llu, this=%p)", name, m_id, this);
                }
                os_signpost_interval_begin(s_log, m_id, "trace_region", "%s", name);
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
                if (s_logEnabled)
                {
                    os_log_debug(s_log, "[trace_region] END (id=%llu, this=%p)", m_id, this);
                }
                os_signpost_interval_end(s_log, m_id, "trace_region");
            }
        }

        trace_region& operator=(trace_region&& other)
        {
            if (this == &other)
            {
                return *this;
            }

            if (m_id != OS_SIGNPOST_ID_NULL)
            {
                if (s_logEnabled)
                {
                    os_log_debug(s_log, "[trace_region] END (move) (id=%llu, this=%p)", m_id, this);
                }
                os_signpost_interval_end(s_log, m_id, "trace_region");
            }

            m_id = other.m_id;
            other.m_id = OS_SIGNPOST_ID_NULL;

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
        static inline os_log_t s_log{os_log_create("arcana", OS_LOG_CATEGORY_POINTS_OF_INTEREST)};
        os_signpost_id_t m_id;
    };
}
