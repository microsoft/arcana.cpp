//
// Copyright (C) Microsoft Corporation. All rights reserved.
//

#include "set_thread_name.h"
#include <windows.h>

namespace arcana
{
    // See: "How to: Set a Thread Name in Native Code"
    // https://docs.microsoft.com/en-us/visualstudio/debugger/how-to-set-a-thread-name-in-native-code
    //
    // Usage: set_thread_name ((DWORD)-1, "MainThread");
    //
    const DWORD MS_VC_EXCEPTION = 0x406D1388;
#pragma pack(push,8)
    typedef struct tagTHREADNAME_INFO
    {
        DWORD type; // Must be 0x1000.
        LPCSTR name; // Pointer to name (in user addr space).
        DWORD threadId; // Thread ID (-1=caller thread).
        DWORD flags; // Reserved for future use, must be zero.
    } THREADNAME_INFO;
#pragma pack(pop)
    void set_thread_name(DWORD threadId, gsl::czstring threadName)
    {
        THREADNAME_INFO info;
        info.type = 0x1000;
        info.name = threadName;
        info.threadId = threadId;
        info.flags = 0;
#pragma warning(push)
#pragma warning(disable: 6320 6322)
        __try
        {
            RaiseException(MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(ULONG_PTR), (ULONG_PTR*)&info);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
        }
#pragma warning(pop)
    }
}
