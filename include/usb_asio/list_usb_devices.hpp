#pragma once

#include <algorithm>
#include <memory>
#include <span>
#include <vector>

#include "usb_asio/asio.hpp"
#include "usb_asio/error.hpp"
#include "usb_asio/usb_device_info.hpp"
#include "usb_asio/usb_service.hpp"

namespace usb_asio
{
    [[nodiscard]] inline auto list_usb_devices(
        asio::execution_context& context,
        error_code& ec)
        -> std::vector<usb_device_info>
    {
        auto& service = asio::use_service<usb_service>(context);

        auto device_handles = static_cast<usb_device_info::handle_type*>(nullptr);
        auto const num_devices = libusb_try(
            ec,
            &::libusb_get_device_list,
            service.handle(),
            &device_handles);
        if (ec) { return {}; }

        auto handles_deleter = [](auto const device_handles) {
            ::libusb_free_device_list(device_handles, true);
        };
        auto handles_owner = std::unique_ptr<usb_device_info::handle_type[], decltype(handles_deleter)>{
            device_handles,
            handles_deleter,
        };

        auto result = std::vector<usb_device_info>{};
        result.reserve(num_devices);
        std::ranges::transform(
            std::span{device_handles, num_devices},
            std::back_inserter(result),
            [](auto const handle) { return usb_device_info{handle}; });

        return result;
    }

    [[nodiscard]] inline auto list_usb_devices(asio::execution_context& context)
        -> std::vector<usb_device_info>
    {
        return try_with_ec([&](auto& ec) {
            return list_usb_devices(context, ec);
        });
    }
}  // namespace usb_asio
