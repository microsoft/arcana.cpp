//
// Copyright (C) Microsoft Corporation. All rights reserved.
//

#include <arcana/threading/task_conversions.h>
#include <CppUnitTest.h>
#include <winrt/Windows.Devices.Enumeration.h>
#include <winrt/Windows.Storage.h>

#include <future>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace UnitTests
{
    TEST_CLASS(TaskConversionsTest)
    {
    public:
        TEST_METHOD(GivenAsyncOperation_WhenOperationSuceeds_TaskAlsoSucceeds)
        {
            using namespace winrt::Windows::Devices::Enumeration;

            std::promise<std::optional<arcana::expected<DeviceInformationCollection, std::error_code>>> promise;

            const auto asyncOperation = DeviceInformation::FindAllAsync(DeviceClass::All);
            auto task = arcana::create_task<std::error_code>(asyncOperation);

            task.then(arcana::inline_scheduler, arcana::cancellation::none(),
                [&](const arcana::expected<DeviceInformationCollection, std::error_code>& result) noexcept
                {
                    promise.set_value(result);
                });

            Assert::IsTrue(promise.get_future().get()->value() != nullptr);
        }

        TEST_METHOD(GivenAsyncOperation_WhenOperationFails_TaskAlsoFails)
        {
            using namespace winrt::Windows::Storage;

            std::promise<std::optional<arcana::expected<StorageFolder, std::error_code>>> promise;

            const auto asyncOperation = StorageFolder::GetFolderFromPathAsync(std::wstring(L"not a valid path"));
            auto task = arcana::create_task<std::error_code>(asyncOperation);

            task.then(arcana::inline_scheduler, arcana::cancellation::none(),
                [&](const arcana::expected<StorageFolder, std::error_code>& result) noexcept
                {
                    promise.set_value(result);
                });

            Assert::AreEqual(E_INVALIDARG, static_cast<HRESULT>(promise.get_future().get()->error().value()));
        }

        TEST_METHOD(GivenAsyncOperation_WhenOperationIsCanceled_TaskAlsoIsCanceled)
        {
            using namespace winrt::Windows::Devices::Enumeration;

            std::promise<std::optional<arcana::expected<DeviceInformationCollection, std::error_code>>> promise;

            const auto asyncOperation = DeviceInformation::FindAllAsync();
            asyncOperation.Cancel();
            auto task = arcana::create_task<std::error_code>(asyncOperation);

            task.then(arcana::inline_scheduler, arcana::cancellation::none(),
                [&](const arcana::expected<DeviceInformationCollection, std::error_code>& result) noexcept
                {
                    promise.set_value(result);
                });

            const auto result = promise.get_future().get();

            Assert::IsTrue(result->has_error());
            Assert::IsTrue(result->error() == std::errc::operation_canceled);
        }
    };
}
