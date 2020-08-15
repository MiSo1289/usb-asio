#pragma once

#include <concepts>
#include <cstdint>
#include <span>
#include <utility>

#include <boost/asio/async_result.hpp>
#include <boost/asio/execution_context.hpp>
#include <boost/asio/executor.hpp>
#include <boost/asio/post.hpp>
#include <libusb.h>
#include "usb_asio/error.hpp"
#include "usb_asio/libusb_ptr.hpp"
#include "usb_asio/usb_device.hpp"
#include "usb_asio/usb_service.hpp"

namespace usb_asio
{
    template <typename Executor = boost::asio::executor>
    class basic_usb_interface
    {
      public:
        using device_handle_type = ::libusb_device_handle*;
        using executor_type = Executor;
        using service_type = usb_service;

        explicit basic_usb_interface(executor_type const& executor)
          : executor_{executor}
          , service_{&boost::asio::use_service<service_type>(executor.context())}
        {
        }

        template <typename ExecutionContext>
        explicit basic_usb_interface(ExecutionContext& context)
          : basic_usb_interface{context.get_executor()}
        {
        }

        template <typename OtherExecutor>
        basic_usb_interface(
            executor_type const& executor,
            basic_usb_device<OtherExecutor>& device,
            std::uint8_t const number,
            bool const detach_kernel_driver = true)
          : basic_usb_interface{executor}
        {
            claim(device, number, detach_kernel_driver);
        }

        template <typename ExecutionContext, typename OtherExecutor>
        basic_usb_interface(
            ExecutionContext& context,
            basic_usb_device<OtherExecutor>& device,
            std::uint8_t const number,
            bool const detach_kernel_driver = true)
          : basic_usb_interface{
              context.get_executor(),
              device,
              number,
              detach_kernel_driver,
          }
        {
        }

        template <std::convertible_to<executor_type> OtherExecutor>
        basic_usb_interface(
            basic_usb_device<OtherExecutor>& device,
            std::uint8_t const number,
            bool const detach_kernel_driver = true)
          : basic_usb_interface{
              device.get_executor(),
              device,
              number,
              detach_kernel_driver,
          }
        {
        }

        basic_usb_interface(basic_usb_interface const&) = delete;

        template <std::convertible_to<executor_type> OtherExecutor>
        basic_usb_interface(basic_usb_interface<OtherExecutor>&& other) noexcept
          : device_handle_{std::exchange(other.device_, nullptr)}
          , number_{std::exchange(other.number_, 0)}
          , executor_{other.executor_}
          , service_{other.service_}
        {
        }

        ~basic_usb_interface() noexcept
        {
            auto ec = std::error_code{};
            unclaim(ec);
        }

        template <typename OtherExecutor>
        void claim(
            basic_usb_device<OtherExecutor>& device,
            std::uint8_t const number,
            bool const detach_kernel_driver = true)
        {
            try_with_ec([&](auto& ec) {
                claim(device, number, ec);
            });
        }

        template <typename OtherExecutor>
        void claim(
            basic_usb_device<OtherExecutor>& device,
            std::uint8_t const number,
            std::error_code& ec) noexcept
        {
            claim(device, number, true, ec);
        }

        template <typename OtherExecutor>
        void claim(
            basic_usb_device<OtherExecutor>& device,
            std::uint8_t const number,
            bool const detach_kernel_driver,
            std::error_code& ec) noexcept
        {
            unclaim(ec);
            if (ec) { return; }

            if (detach_kernel_driver)
            {
                ::libusb_detach_kernel_driver(
                    device.handle(),
                    number);
            }

            libusb_try(
                ec,
                ::libusb_claim_interface,
                device.handle(),
                number);
            if (ec) { return; }

            device_handle_ = device.handle();
            number_ = number;
        }

        void unclaim(bool const reattach_kernel_driver = true)
        {
            try_with_ec([&](auto& ec) {
                unclaim(reattach_kernel_driver, ec);
            });
        }

        void unclaim(std::error_code& ec) noexcept
        {
            unclaim(true, ec);
        }

        // Avoiding the libusb name 'release'
        // to avoid confusion with unique_ptr-like release (here called detach).
        void unclaim(bool const reattach_kernel_driver, std::error_code& ec) noexcept
        {
            ec.clear();

            if (!is_claimed()) { return; }

            libusb_try(
                ec,
                ::libusb_release_interface,
                device_handle(),
                number());
            if (ec) { return; }

            if (reattach_kernel_driver)
            {
                ::libusb_attach_kernel_driver(
                    device_handle(),
                    number());
            }

            detach();
        }

        template <typename CompletionToken = boost::asio::default_completion_token_t<executor_type>>
        auto async_unclaim(CompletionToken&& token = {})
        {
            return async_unclaim(true, std::forward<CompletionToken>(token));
        }

        template <typename CompletionToken = boost::asio::default_completion_token_t<executor_type>>
        auto async_unclaim(
            bool const reattach_kernel_driver,
            CompletionToken&& token = {})
        {
            async_try_blocking_with_ec(
                executor_,
                service_->blocking_op_executor(),
                std::forward<CompletionToken>(token),
                [this, reattach_kernel_driver](auto& ec) {
                    unclaim(reattach_kernel_driver, ec);
                });
        }

        void set_alt_setting(std::uint8_t const alt_setting)
        {
            try_with_ec([&](auto& ec) {
                set_alt_setting(alt_setting, ec);
            });
        }

        void set_alt_setting(
            std::uint8_t const alt_setting,
            std::error_code& ec) noexcept
        {
            libusb_try(
                ec,
                &::libusb_set_interface_alt_setting,
                device_handle(),
                number(),
                alt_setting);
        }

        template <typename CompletionToken = boost::asio::default_completion_token_t<executor_type>>
        auto async_set_alt_setting(
            std::uint8_t const alt_setting,
            CompletionToken&& token = {})
        {
            async_try_blocking_with_ec(
                executor_,
                service_->blocking_op_executor(),
                std::forward<CompletionToken>(token),
                [this, alt_setting](auto& ec) {
                    set_alt_setting(alt_setting, ec);
                });
        }

        void detach() noexcept
        {
            device_handle_ = nullptr;
            number_ = 0;
        }

        [[nodiscard]] auto device_handle() const noexcept -> device_handle_type
        {
            return device_handle_;
        }

        [[nodiscard]] auto number() const noexcept -> std::uint8_t
        {
            return number_;
        }

        [[nodiscard]] auto get_executor() const noexcept -> executor_type
        {
            return executor_;
        }

        [[nodiscard]] auto is_claimed() const noexcept -> bool
        {
            return device_handle_ == nullptr;
        }

        auto operator=(basic_usb_interface const&) = delete;

        template <std::convertible_to<executor_type> OtherExecutor>
        auto operator=(basic_usb_interface<OtherExecutor>&& other) noexcept -> basic_usb_interface&
        {
            auto ec = std::error_code{};
            unclaim(ec);

            device_handle_ = std::exchange(other.device_handle_, nullptr);
            number_ = std::exchange(other.number_, 0);
            executor_ = other.executor_;
            service_ = other.service_;

            return *this;
        }

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return is_claimed();
        }

        friend auto operator<=>(basic_usb_interface const&, basic_usb_interface const&) = default;

      private:
        device_handle_type device_handle_;
        std::uint8_t number_ = 0;
        executor_type executor_;
        service_type* service_;
    };

}  // namespace usb_asio
