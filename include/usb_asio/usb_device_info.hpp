#pragma once

#include <cstdint>
#include <vector>

#include <libusb.h>
#include "usb_asio/error.hpp"
#include "usb_asio/flags.hpp"
#include "usb_asio/libusb_ptr.hpp"

namespace usb_asio
{
    class usb_device_info
    {
      public:
        using handle_type = ::libusb_device*;
        using ref_handle_type = libusb_ref_ptr<
            ::libusb_device,
            &::libusb_ref_device,
            &::libusb_unref_device>;
        // Not bothering with wrapping the descriptor structures for now
        using config_descriptor_ptr = libusb_ptr<
            ::libusb_config_descriptor,
            &::libusb_free_config_descriptor>;

        explicit usb_device_info(handle_type const handle) noexcept
          : handle_{handle} { }

        usb_device_info(handle_type const handle, adopt_ref_t) noexcept
          : handle_{handle, adopt_ref} { }

        [[nodiscard]] auto handle() const noexcept -> handle_type
        {
            return handle_.get();
        }

        [[nodiscard]] auto bus_number() const noexcept -> std::uint8_t
        {
            return ::libusb_get_bus_number(handle());
        }

        [[nodiscard]] auto port_number() const noexcept -> std::uint8_t
        {
            return ::libusb_get_port_number(handle());
        }

        template <typename Alloc = std::allocator<std::uint8_t>>
        [[nodiscard]] auto port_numbers(Alloc const& alloc = {}) const -> std::vector<std::uint8_t, Alloc>
        {
            // As per USB 3.0 specs and libusb documentation.
            constexpr auto max_depth = std::size_t{7};

            auto ports = std::vector<std::uint8_t, Alloc>(max_depth, alloc);
            auto ec = error_code{};

            while (true)
            {
                auto const num_ports = libusb_try(
                    ec,
                    ::libusb_get_port_numbers,
                    handle(),
                    ports.data(),
                    static_cast<int>(ports.size()));

                if (ec)
                {
                    // This should not happen
                    if (ec == usb_errc::overflow)
                    {
                        ports.resize(2 * ports.size());
                        continue;
                    }

                    // This definitely should not happen
                    throw system_error{ec};
                }

                ports.resize(num_ports);

                return ports;
            }
        }

        [[nodiscard]] auto parent() const noexcept -> std::optional<usb_device_info>
        {
            if (auto const parent_handle = ::libusb_get_parent(handle()))
            {
                return usb_device_info{parent_handle};
            }

            return std::nullopt;
        }

        [[nodiscard]] auto device_address() const noexcept -> std::uint8_t
        {
            return ::libusb_get_device_address(handle());
        }

        [[nodiscard]] auto device_speed() const noexcept -> usb_speed
        {
            return static_cast<usb_speed>(::libusb_get_device_speed(handle()));
        }

        [[nodiscard]] auto max_iso_packet_size(std::uint8_t const endpoint) const
            -> std::size_t
        {
            return try_with_ec([&](auto& ec) {
                return max_iso_packet_size(endpoint, ec);
            });
        }

        [[nodiscard]] auto max_iso_packet_size(
            std::uint8_t const endpoint,
            error_code& ec) const noexcept
            -> std::size_t
        {
            return libusb_try(
                ec, &::libusb_get_max_iso_packet_size, handle(), endpoint);
        }

        [[nodiscard]] auto device_descriptor() const
            -> ::libusb_device_descriptor
        {
            return try_with_ec([&](auto& ec) {
                return device_descriptor(ec);
            });
        }

        [[nodiscard]] auto device_descriptor(error_code& ec) const noexcept
            -> ::libusb_device_descriptor
        {
            auto descriptor = ::libusb_device_descriptor{};
            libusb_try(ec, &::libusb_get_device_descriptor, handle(), &descriptor);
            return descriptor;
        }

        [[nodiscard]] auto active_config_descriptor() const
            -> config_descriptor_ptr
        {
            return try_with_ec([&](auto& ec) {
                return active_config_descriptor(ec);
            });
        }

        [[nodiscard]] auto active_config_descriptor(
            error_code& ec) const noexcept
            -> config_descriptor_ptr
        {
            auto descriptor = config_descriptor_ptr::pointer{};
            libusb_try(
                ec,
                &::libusb_get_active_config_descriptor,
                handle(),
                &descriptor);

            return config_descriptor_ptr{descriptor};
        }

        [[nodiscard]] auto config_descriptor(
            std::uint8_t const config_index) const
            -> config_descriptor_ptr
        {
            return try_with_ec([&](auto& ec) {
                return config_descriptor(config_index, ec);
            });
        }

        [[nodiscard]] auto config_descriptor(
            std::uint8_t const config_index,
            error_code& ec) const noexcept
            -> config_descriptor_ptr
        {
            auto descriptor = config_descriptor_ptr::pointer{};
            libusb_try(
                ec,
                &::libusb_get_config_descriptor,
                handle(),
                config_index,
                &descriptor);

            return config_descriptor_ptr{descriptor};
        }

        [[nodiscard]] auto config_descriptor_by_id_value(
            std::uint8_t const config_id_value) const
            -> config_descriptor_ptr
        {
            return try_with_ec([&](auto& ec) {
                return config_descriptor_by_id_value(config_id_value, ec);
            });
        }

        [[nodiscard]] auto config_descriptor_by_id_value(
            std::uint8_t const config_id_value,
            error_code& ec) const noexcept
            -> config_descriptor_ptr
        {
            auto descriptor = config_descriptor_ptr::pointer{};
            libusb_try(
                ec,
                &::libusb_get_config_descriptor_by_value,
                handle(),
                config_id_value,
                &descriptor);

            return config_descriptor_ptr{descriptor};
        }

        friend auto operator<=>(usb_device_info const&, usb_device_info const&) = default;

      private:
        ref_handle_type handle_;
    };
}  // namespace usb_asio
