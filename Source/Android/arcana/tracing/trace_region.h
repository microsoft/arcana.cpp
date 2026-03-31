//
// Copyright (C) Microsoft Corporation. All rights reserved.
//

//
// Android trace_region implementation using the NDK ATrace API.
//
// This uses ATrace_beginAsyncSection/ATrace_endAsyncSection (API 29+) which pair
// begin/end events by name + cookie, allowing trace regions to be moved across
// threads (e.g. when an RAII trace_region is captured into an async continuation).
// The sync API (ATrace_beginSection/ATrace_endSection) uses a per-thread stack,
// which breaks when begin and end occur on different threads.
//
// When targeting minSdkVersion < 29, the async functions are resolved at runtime
// via dlsym. If unavailable (device below API 29), ATrace is silently skipped
// (logcat output at trace_level::log still works).
//
// NOTE: Async trace sections may not be visible in Android Studio's Profiler UI.
// To view them, capture a trace with Perfetto:
//
//   adb shell atrace --async_start -a <your.package.name> -c
//   # ... interact with the app ...
//   adb shell atrace --async_stop -o /data/local/tmp/trace.txt
//   adb pull /data/local/tmp/trace.txt
//
// Then open the trace file at https://ui.perfetto.dev
//

#pragma once

#include <atomic>
#include <string>
#include <android/trace.h>
#include <android/log.h>
#if __ANDROID_MIN_SDK_VERSION__ < 29
#include <dlfcn.h>
#endif

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
            m_cookie{s_enabled ? s_nextCookie.fetch_add(1, std::memory_order_relaxed) : 0},
            m_name{m_cookie != 0 ? name : ""}
        {
            if (m_cookie != 0)
            {
                if (s_logEnabled)
                {
                    __android_log_print(ANDROID_LOG_DEBUG, TRACE_TAG, "[trace_region] BEGIN %s (cookie=%d, this=%p)", m_name.c_str(), m_cookie, static_cast<const void*>(this));
                }
                traceBegin(m_name.c_str(), m_cookie);
            }
        }

        // Move constructor transfers ownership; the moved-from region becomes inactive
        // (cookie set to 0) so its destructor won't emit a spurious end event.
        trace_region(trace_region&& other) :
            m_cookie{other.m_cookie},
            m_name{std::move(other.m_name)}
        {
            other.m_cookie = 0;
        }

        ~trace_region()
        {
            if (m_cookie != 0)
            {
                if (s_logEnabled)
                {
                    __android_log_print(ANDROID_LOG_DEBUG, TRACE_TAG, "[trace_region] END (cookie=%d, this=%p)", m_cookie, static_cast<const void*>(this));
                }
                traceEnd(m_name.c_str(), m_cookie);
            }
        }

        trace_region& operator=(trace_region&& other)
        {
            if (m_cookie != 0)
            {
                if (s_logEnabled)
                {
                    __android_log_print(ANDROID_LOG_DEBUG, TRACE_TAG, "[trace_region] END (move) (cookie=%d, this=%p)", m_cookie, static_cast<const void*>(this));
                }
                traceEnd(m_name.c_str(), m_cookie);
            }

            m_cookie = other.m_cookie;
            m_name = std::move(other.m_name);
            other.m_cookie = 0;

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
        static void traceBegin(const char* name, int32_t cookie)
        {
#if __ANDROID_MIN_SDK_VERSION__ >= 29
            ATrace_beginAsyncSection(name, cookie);
#else
            if (s_beginAsync)
                s_beginAsync(name, cookie);
#endif
        }

        static void traceEnd(const char* name, int32_t cookie)
        {
#if __ANDROID_MIN_SDK_VERSION__ >= 29
            ATrace_endAsyncSection(name, cookie);
#else
            if (s_endAsync)
                s_endAsync(name, cookie);
#endif
        }

        static inline std::atomic<bool> s_enabled{false};
        static inline std::atomic<bool> s_logEnabled{false};
        static inline std::atomic<int32_t> s_nextCookie{1};

#if __ANDROID_MIN_SDK_VERSION__ < 29
        // Resolve async trace functions at load time. These are available on
        // devices running API 29+, even when the app targets a lower minSdkVersion.
        using AsyncTraceFunc = void (*)(const char*, int32_t);
        static inline const AsyncTraceFunc s_beginAsync{
            reinterpret_cast<AsyncTraceFunc>(dlsym(RTLD_DEFAULT, "ATrace_beginAsyncSection"))};
        static inline const AsyncTraceFunc s_endAsync{
            reinterpret_cast<AsyncTraceFunc>(dlsym(RTLD_DEFAULT, "ATrace_endAsyncSection"))};
#endif

        // Cookie uniquely identifies this trace interval for the async API, analogous
        // to os_signpost_id_t on Apple. A cookie of 0 means the region is inactive.
        int32_t m_cookie;
        // Name is stored as std::string (not const char*) because callers may pass
        // c_str() from a temporary std::string, and ATrace_endAsyncSection needs the
        // name to match the corresponding begin call.
        std::string m_name;
    };
}
