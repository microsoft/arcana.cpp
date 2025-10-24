//
// Copyright (C) Microsoft Corporation. All rights reserved.
//

#include <arcana/hresult.h>
#include <arcana/type_traits.h>
#include <future>
#include <gtest/gtest.h>
#include <bitset>

namespace
{
    constexpr unsigned int bits(int bytes)
    {
        return bytes * 8;
    }
}

TEST(HresultTest, ConvertHresult)
{
    const arcana::hresult failure = arcana::hresult::dxgi_error_device_removed;
    const std::error_code code = arcana::error_code_from_hr(failure);
    EXPECT_EQ(arcana::underlying_cast(failure), arcana::hr_from_error_code(code));
}

TEST(HresultTest, ConvertStandardErrorCode)
{
    const auto code = make_error_code(std::errc::argument_out_of_domain);
    const int32_t hresult = arcana::hr_from_error_code(code);
    EXPECT_TRUE(code == arcana::error_code_from_hr(hresult));
}

TEST(HresultTest, VerifyCustomHResultFailureBitIsSet)
{
    auto cameraError = make_error_code(std::errc::broken_pipe);
    auto hresultError = arcana::hr_from_error_code(cameraError);

    ::std::bitset<bits(sizeof(int32_t))> hresultBitset(hresultError);

    EXPECT_TRUE(hresultBitset.test(31));
}

TEST(HresultTest, VerifyCustomHResultCustomerBitIsSet)
{
    auto cameraError = make_error_code(std::errc::broken_pipe);
    auto hresultError = arcana::hr_from_error_code(cameraError);

    ::std::bitset<bits(sizeof(int32_t))> hresultBitset(hresultError);

    EXPECT_TRUE(hresultBitset.test(29));
}

TEST(HresultTest, VerifyCustomHResultCategoryIsDifferent)
{
    auto genericError = arcana::hr_from_error_code(make_error_code(std::errc::bad_file_descriptor));
    auto futureError = arcana::hr_from_error_code(make_error_code(std::future_errc::no_state));

    ::std::bitset<bits(sizeof(int32_t))> genericErrorBits(genericError);
    ::std::bitset<bits(sizeof(int32_t))> futureErrorBits(futureError);
    ::std::bitset<bits(sizeof(int32_t))> maxErrC = (0xFFFF);

    // verify bits 17 through 26 are not identical between two different categories (they should be unique)
    auto filteredGenericBits = (genericErrorBits | maxErrC) ^ maxErrC;
    auto filteredarcanaBits = (futureErrorBits | maxErrC) ^ maxErrC;

    EXPECT_TRUE(filteredGenericBits != filteredarcanaBits);
}

TEST(HresultTest, VerifyCanGetGenericErrorFromHResult)
{
    auto genericError = arcana::hr_from_error_code(make_error_code(std::errc::broken_pipe));
    auto category = arcana::get_category_from_hresult(genericError);

    EXPECT_FALSE(category == nullptr);
    EXPECT_TRUE(*category == std::generic_category());
}

TEST(HresultTest, VerifyStandardHResultDoesNotReturnCategory)
{
    auto category = arcana::get_category_from_hresult(E_FAIL);
    EXPECT_TRUE(category == nullptr);
}

TEST(HresultTest, VerifyConvertToFromHResult)
{
    auto cameraError = make_error_code(std::errc::broken_pipe);
    auto hresultError = arcana::hr_from_error_code(cameraError);
    auto newCameraError = arcana::error_code_from_hr(hresultError);

    EXPECT_TRUE(cameraError.category() == newCameraError.category());
    EXPECT_TRUE(cameraError.value() == newCameraError.value());
}
