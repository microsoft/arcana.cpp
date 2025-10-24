//
// Copyright (C) Microsoft Corporation. All rights reserved.
//

#include <gtest/gtest.h>

#include <arcana/experimental/array.h>

TEST(ArrayUnitTest, GivenArray_WhenConstructedWithMakeArray_VerifyElementsAreInArray)
{
    // Auto-deduced type should be the same as the explicitly defined type.
    auto arr1 = arcana::make_array(1, 2, 3);
    std::array<int, 3> arr2{ 1, 2, 3 };

    static_assert(std::is_same<decltype(arr1), decltype(arr2)>::value, "Differing array types");
    static_assert(arr1.size() == arr2.size(), "Differing array lengths");

    EXPECT_TRUE(std::equal(arr1.begin(), arr1.end(), arr2.begin(), arr2.end())) << "Different elements";
}
