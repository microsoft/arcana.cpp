//
// Copyright (C) Microsoft Corporation. All rights reserved.
//

#pragma once

#include <intsafe.h>
#include <gsl/gsl>

namespace arcana
{
    void set_thread_name(DWORD threadId, gsl::czstring threadName);
}
