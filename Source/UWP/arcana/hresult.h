//
// Copyright (C) Microsoft Corporation. All rights reserved.
//

#pragma once

#include <winerror.h>
#include <system_error>

namespace arcana
{
    enum class hresult : int32_t
    {
        e_abort = E_ABORT,
        e_accessdenied = E_ACCESSDENIED,
        e_fail = E_FAIL,
        e_handle = E_HANDLE,
        e_invalidarg = E_INVALIDARG,
        e_nointerface = E_NOINTERFACE,
        e_notimpl = E_NOTIMPL,
        e_outofmemory = E_OUTOFMEMORY,
        e_pointer = E_POINTER,
        e_unexpected = E_UNEXPECTED,
        e_pending = E_PENDING,

        dxgi_error_device_removed = DXGI_ERROR_DEVICE_REMOVED
    };

    class hresult_error_category : public std::error_category
    {
    public:
        virtual const char* name() const noexcept override;
        virtual std::string message(int evt) const override;

        void add_category(const std::error_category& category);
    };

    const std::error_category& hresult_category();

    std::error_code error_code_from_hr(hresult hresult);
    std::error_code error_code_from_hr(int32_t hresult);
    int32_t hr_from_error_code(const std::error_code& error_code);
    const std::error_category* get_category_from_hresult(int32_t hresult);

    inline std::error_code make_error_code(hresult e)
    {
        return std::error_code(static_cast<int>(e), hresult_category());
    }
}

namespace std
{
    template <>
    struct is_error_code_enum<arcana::hresult> : public true_type {};
}
