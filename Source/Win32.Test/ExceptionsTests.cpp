//
// Copyright (C) Microsoft Corporation. All rights reserved.
//

#include <arcana/win32_exception.h>
#include <CppUnitTest.h>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace UnitTests
{
    TEST_CLASS(ExceptionsTest)
    {
    public:
        TEST_METHOD(VerifyWin32Exception)
        {
            try
            {
                throw arcana::win32_exception{ ERROR_FILE_NOT_FOUND };
            }
            catch (arcana::win32_exception exception)
            {
                Assert::AreEqual<DWORD>(exception.errorCode(), ERROR_FILE_NOT_FOUND);
            }
        }
    };
}
