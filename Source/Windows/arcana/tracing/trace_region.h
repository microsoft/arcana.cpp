//
// Copyright (C) Microsoft Corporation. All rights reserved.
//

//
// Windows trace_region implementation using ETW TraceLogging.
//
// Events are emitted as ETW activity start/stop pairs via TraceLoggingWriteActivity
// under the "Arcana.TraceRegion" provider. Each trace region gets a unique activity
// GUID (derived from an integer cookie), allowing WPA to correlate begin/end events
// into duration spans — even across threads.
//
// TRACELOGGING_DEFINE_PROVIDER_STORAGE creates static (TU-local) provider
// storage with no external symbols, avoiding linker conflicts when included
// from multiple translation units. The handle is defined as a C++17 inline
// variable so all TUs share a single provider pointer.
//
// To capture and view traces (from an elevated command prompt):
//   1. Start an ETW trace session:
//      logman create trace ArcanaTrace -p "{B2A07E6E-A49F-4C4F-B9D2-8D3E5C7F1A2B}" -o arcana.etl -ets
//   2. Run the application
//   3. Stop the trace:
//      logman stop ArcanaTrace -ets
//   4. Open arcana.etl in Windows Performance Analyzer (WPA) or PerfView.
//      In WPA, trace regions appear as duration spans in the Regions of Interest
//      activity view under "Arcana.TraceRegion".
//
// Debug log output (at trace_level::log) goes to OutputDebugStringA,
// visible in the Visual Studio Output window or DebugView (SysInternals).
//

#pragma once

#include <atomic>
#include <cstdio>
#include <memory>
#include <string>
#include <windows.h>
#include <evntrace.h>
#include <TraceLoggingProvider.h>

#pragma comment(lib, "advapi32.lib")

namespace arcana
{
    namespace detail
    {
        // Static per-TU storage — no external symbols, no linker conflicts.
        TRACELOGGING_DEFINE_PROVIDER_STORAGE(
            s_traceRegionProviderStorage,
            "Arcana.TraceRegion",
            // {B2A07E6E-A49F-4C4F-B9D2-8D3E5C7F1A2B}
            (0xB2A07E6E, 0xA49F, 0x4C4F, 0xB9, 0xD2, 0x8D, 0x3E, 0x5C, 0x7F, 0x1A, 0x2B));

        // C++17 inline — linker picks one definition, all TUs share the same handle.
        inline TraceLoggingHProvider const g_traceRegionProvider = &s_traceRegionProviderStorage;
    }

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
            m_cookie{s_enabled ? s_nextCookie.fetch_add(1, std::memory_order_relaxed) : 0},
            m_name{m_cookie != 0 ? name : ""},
            m_activityId{cookieToGuid(m_cookie)},
            m_registration{m_cookie != 0 ? s_registration : nullptr}
        {
            if (m_cookie != 0)
            {
                if (s_logEnabled)
                {
                    char buf[256];
                    std::snprintf(buf, sizeof(buf), "[trace_region] BEGIN %s (cookie=%d)\n", m_name.c_str(), m_cookie);
                    OutputDebugStringA(buf);
                }
                TraceLoggingWriteActivity(detail::g_traceRegionProvider,
                    "TraceRegion",
                    &m_activityId,
                    nullptr,
                    TraceLoggingOpcode(EVENT_TRACE_TYPE_START),
                    TraceLoggingString(m_name.c_str(), "Name"),
                    TraceLoggingInt32(m_cookie, "Cookie"));
            }
        }

        // Move constructor transfers ownership; the moved-from region becomes inactive
        // (cookie set to 0) so its destructor won't emit a spurious stop event.
        trace_region(trace_region&& other) :
            m_cookie{other.m_cookie},
            m_name{std::move(other.m_name)},
            m_activityId{other.m_activityId},
            m_registration{std::move(other.m_registration)}
        {
            other.m_cookie = 0;
        }

        ~trace_region()
        {
            if (m_cookie != 0)
            {
                if (s_logEnabled)
                {
                    char buf[256];
                    std::snprintf(buf, sizeof(buf), "[trace_region] END (cookie=%d)\n", m_cookie);
                    OutputDebugStringA(buf);
                }
                TraceLoggingWriteActivity(detail::g_traceRegionProvider,
                    "TraceRegion",
                    &m_activityId,
                    nullptr,
                    TraceLoggingOpcode(EVENT_TRACE_TYPE_STOP),
                    TraceLoggingString(m_name.c_str(), "Name"),
                    TraceLoggingInt32(m_cookie, "Cookie"));
            }
        }

        trace_region& operator=(trace_region&& other)
        {
            if (this == &other)
            {
                return *this;
            }

            if (m_cookie != 0)
            {
                if (s_logEnabled)
                {
                    char buf[256];
                    std::snprintf(buf, sizeof(buf), "[trace_region] END (move) (cookie=%d)\n", m_cookie);
                    OutputDebugStringA(buf);
                }
                TraceLoggingWriteActivity(detail::g_traceRegionProvider,
                    "TraceRegion",
                    &m_activityId,
                    nullptr,
                    TraceLoggingOpcode(EVENT_TRACE_TYPE_STOP),
                    TraceLoggingString(m_name.c_str(), "Name"),
                    TraceLoggingInt32(m_cookie, "Cookie"));
            }

            m_cookie = other.m_cookie;
            m_name = std::move(other.m_name);
            m_activityId = other.m_activityId;
            m_registration = std::move(other.m_registration);
            other.m_cookie = 0;

            return *this;
        }

        static void enable(trace_level level = trace_level::mark)
        {
            TraceLoggingRegister(detail::g_traceRegionProvider);
            // Create a shared_ptr whose custom deleter calls TraceLoggingUnregister.
            // Each active trace_region holds a copy, so the provider stays registered
            // until disable() is called AND all outstanding regions are destroyed.
            s_registration = std::make_shared<ProviderRegistration>();
            s_enabled = true;
            s_logEnabled = level == trace_level::log;
        }

        static void disable()
        {
            s_enabled = false;
            s_logEnabled = false;
            // Release our ref. If active trace_regions still hold copies,
            // the provider stays registered until their destructors run.
            s_registration.reset();
        }

    private:
        // RAII guard for provider registration. When the last shared_ptr to this
        // object is released, the provider is unregistered. This ensures the
        // provider stays registered as long as any trace_region is alive.
        struct ProviderRegistration
        {
            ~ProviderRegistration()
            {
                TraceLoggingUnregister(detail::g_traceRegionProvider);
            }
        };

        // Derives a deterministic GUID from the cookie for ETW activity correlation.
        // Only needs to be unique within a single trace session.
        static GUID cookieToGuid(int32_t cookie)
        {
            // Use the provider GUID as a base, with the cookie in Data1.
            return {static_cast<unsigned long>(cookie), 0xA49F, 0x4C4F,
                    {0xB9, 0xD2, 0x8D, 0x3E, 0x5C, 0x7F, 0x1A, 0x2B}};
        }

        static inline std::atomic<bool> s_enabled{false};
        static inline std::atomic<bool> s_logEnabled{false};
        static inline std::atomic<int32_t> s_nextCookie{1};
        static inline std::shared_ptr<ProviderRegistration> s_registration;

        // Cookie uniquely identifies this trace interval, analogous to
        // os_signpost_id_t on Apple. A cookie of 0 means the region is inactive.
        int32_t m_cookie;
        // Name is stored as std::string (not const char*) because callers may pass
        // c_str() from a temporary std::string, and the stop event needs the name
        // to match the corresponding start event for correlation.
        std::string m_name;
        // Activity GUID derived from cookie, used by ETW to correlate start/stop
        // events into duration spans in WPA.
        GUID m_activityId;
        // Ref-counted handle to the provider registration. Ensures the provider
        // stays registered until this region emits its stop event, even if
        // disable() is called while regions are still alive.
        std::shared_ptr<ProviderRegistration> m_registration;
    };
}
