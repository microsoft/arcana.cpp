//
// Copyright (C) Microsoft Corporation. All rights reserved.
//

#include <arcana/hresult.h>
#include <arcana/type_traits.h>
#include <future>
#include <CppUnitTest.h>
#include <bitset>

using Assert = Microsoft::VisualStudio::CppUnitTestFramework::Assert;

namespace UnitTests
{
    namespace
    {
        constexpr unsigned int bits(int bytes)
        {
            return bytes * 8;
        }
    }

    TEST_CLASS(HresultTest)
    {
        TEST_METHOD(ConvertHresult)
        {
            const arcana::hresult failure = arcana::hresult::dxgi_error_device_removed;
            const std::error_code code = arcana::error_code_from_hr(failure);
            Assert::AreEqual(arcana::underlying_cast(failure), arcana::hr_from_error_code(code));
        }

        TEST_METHOD(ConvertStandardErrorCode)
        {
            const auto code = make_error_code(std::errc::argument_out_of_domain);
            const int32_t hresult = arcana::hr_from_error_code(code);
            Assert::IsTrue(code == arcana::error_code_from_hr(hresult));
        }

        TEST_METHOD(VerifyCustomHResultFailureBitIsSet)
        {
            auto cameraError = make_error_code(std::errc::broken_pipe);
            auto hresultError = arcana::hr_from_error_code(cameraError);

            ::std::bitset<bits(sizeof(int32_t))> hresultBitset(hresultError);

            Assert::IsTrue(hresultBitset.test(31));
        }

        TEST_METHOD(VerifyCustomHResultCustomerBitIsSet)
        {
            auto cameraError = make_error_code(std::errc::broken_pipe);
            auto hresultError = arcana::hr_from_error_code(cameraError);

            ::std::bitset<bits(sizeof(int32_t))> hresultBitset(hresultError);

            Assert::IsTrue(hresultBitset.test(29));
        }

        TEST_METHOD(VerifyCustomHResultCategoryIsDifferent)
        {
            auto genericError = arcana::hr_from_error_code(make_error_code(std::errc::bad_file_descriptor));
            auto futureError = arcana::hr_from_error_code(make_error_code(std::future_errc::no_state));

            ::std::bitset<bits(sizeof(int32_t))> genericErrorBits(genericError);
            ::std::bitset<bits(sizeof(int32_t))> futureErrorBits(futureError);
            ::std::bitset<bits(sizeof(int32_t))> maxErrC = (0xFFFF);

            // verify bits 17 through 26 are not identical between two different categories (they should be unique)
            auto filteredGenericBits = (genericErrorBits | maxErrC) ^ maxErrC;
            auto filteredarcanaBits = (futureErrorBits | maxErrC) ^ maxErrC;

            Assert::IsTrue(filteredGenericBits != filteredarcanaBits);
        }

        TEST_METHOD(VerifyCanGetGenericErrorFromHResult)
        {
            auto genericError = arcana::hr_from_error_code(make_error_code(std::errc::broken_pipe));
            auto category = arcana::get_category_from_hresult(genericError);

            Assert::IsFalse(category == nullptr);
            Assert::IsTrue(*category == std::generic_category());
        }

        TEST_METHOD(VerifyStandardHResultDoesNotReturnCategory)
        {
            auto category = arcana::get_category_from_hresult(E_FAIL);
            Assert::IsTrue(category == nullptr);
        }

        TEST_METHOD(VerifyConvertToFromHResult)
        {
            auto cameraError = make_error_code(std::errc::broken_pipe);
            auto hresultError = arcana::hr_from_error_code(cameraError);
            auto newCameraError = arcana::error_code_from_hr(hresultError);

            Assert::IsTrue(cameraError.category() == newCameraError.category());
            Assert::IsTrue(cameraError.value() == newCameraError.value());
        }
    };
}
