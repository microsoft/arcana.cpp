//
// Copyright (C) Microsoft Corporation. All rights reserved.
//

#pragma once

#include <exception>
#include <Windows.h>

namespace arcana
{
    class win32_exception : public std::exception
    {
    public:
        win32_exception(DWORD errorCode)
            : m_errorCode(errorCode)
        {
        }

        DWORD errorCode()
        {
            return m_errorCode;
        }

    private:
        DWORD m_errorCode;
    };
}
