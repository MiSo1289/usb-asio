#pragma once

#include <bitset>

#include <libusb.h>

namespace usb_asio
{
    enum class usb_speed
    {
        unknown = ::LIBUSB_SPEED_UNKNOWN,
        low = ::LIBUSB_SPEED_LOW,
        full = ::LIBUSB_SPEED_FULL,
        high = ::LIBUSB_SPEED_HIGH,
        super = ::LIBUSB_SPEED_SUPER,
        super_plus = ::LIBUSB_SPEED_SUPER_PLUS,
        /* super_saiyan_3 = 9001, */
    };

    using usb_supported_speed = std::bitset<32>;

    namespace usb_supported_speed_flags
    {
        inline constexpr auto low = usb_supported_speed{::LIBUSB_LOW_SPEED_OPERATION};
        inline constexpr auto full = usb_supported_speed{::LIBUSB_FULL_SPEED_OPERATION};
        inline constexpr auto high = usb_supported_speed{::LIBUSB_HIGH_SPEED_OPERATION};
        inline constexpr auto super = usb_supported_speed{::LIBUSB_SUPER_SPEED_OPERATION};
    }  // namespace usb_supported_speed_flags

    enum class usb_transfer_type
    {
        control = ::LIBUSB_TRANSFER_TYPE_CONTROL,
        isochronous = ::LIBUSB_TRANSFER_TYPE_ISOCHRONOUS,
        bulk = ::LIBUSB_TRANSFER_TYPE_BULK,
        interrupt = ::LIBUSB_TRANSFER_TYPE_INTERRUPT,
        bulk_stream = ::LIBUSB_TRANSFER_TYPE_BULK_STREAM,
    };

    enum class usb_transfer_direction
    {
        in = ::LIBUSB_ENDPOINT_IN,
        out = ::LIBUSB_ENDPOINT_OUT,
    };

    enum class usb_control_request_recipient
    {
        device = ::LIBUSB_RECIPIENT_DEVICE,
        interface = ::LIBUSB_RECIPIENT_INTERFACE,
        endpoint = ::LIBUSB_RECIPIENT_ENDPOINT,
        other = ::LIBUSB_RECIPIENT_OTHER,
    };

    enum class usb_control_request_type
    {
        standard_request = ::LIBUSB_REQUEST_TYPE_STANDARD,
        class_request = ::LIBUSB_REQUEST_TYPE_CLASS,
        vendor_request = ::LIBUSB_REQUEST_TYPE_VENDOR,
        reserved = ::LIBUSB_REQUEST_TYPE_RESERVED,
    };
}  // namespace usb_asio
