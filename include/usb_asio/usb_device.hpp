#pragma once

#include <concepts>
#include <compare>
#include <cstdint>
#include <span>

#include <libusb.h>
#include "usb_asio/asio.hpp"
#include "usb_asio/error.hpp"
#include "usb_asio/libusb_ptr.hpp"
#include "usb_asio/usb_device_info.hpp"
#include "usb_asio/usb_service.hpp"

namespace usb_asio
{
    template <typename Executor = asio::any_io_executor>
    class basic_usb_device
    {
      public:
        using handle_type = ::libusb_device_handle*;
        using unique_handle_type = libusb_ptr<::libusb_device_handle, &::libusb_close>;
        using executor_type = Executor;
        using service_type = usb_service;

        explicit basic_usb_device(executor_type const& executor)
          : executor_{executor}
          , service_{&asio::use_service<service_type>(asio::query(executor, asio::execution::context))}
        {
        }

        template <std::derived_from<asio::execution_context> ExecutionContext>
        explicit basic_usb_device(ExecutionContext& context)
          : basic_usb_device{context.get_executor()}
        {
        }

        basic_usb_device(executor_type const& executor, usb_device_info const& info)
          : basic_usb_device{executor}
        {
            open(info);
        }

        template <std::derived_from<asio::execution_context> ExecutionContext>
        basic_usb_device(ExecutionContext& context, usb_device_info const& info)
          : basic_usb_device{context.get_executor(), info}
        {
        }

        template <std::convertible_to<executor_type> OtherExecutor>
        basic_usb_device(basic_usb_device<OtherExecutor>&& other) noexcept
          : handle_{std::exchange(other.handle_, nullptr)}
          , executor_{other.executor_}
          , service_{other.service_}
        {
        }

        void open(usb_device_info const& info) noexcept
        {
            try_with_ec([&](auto& ec) {
                open(info, ec);
            });
        }

        void open(usb_device_info const& info, error_code& ec)
        {
            close();

            auto handle = handle_type{};
            libusb_try(ec, &::libusb_open, info.handle(), &handle);
            if (ec) { return; }

            handle_ = unique_handle_type{handle};
            service_->notify_dev_opened();
        }

        void close() noexcept
        {
            if (is_open())
            {
                service_->notify_dev_closed();
                handle_.reset();
            }
        }

        void set_configuration(std::uint8_t const configuration)
        {
            try_with_ec([&](auto& ec) {
                set_configuration(configuration, ec);
            });
        }

        void set_configuration(
            std::uint8_t const configuration,
            error_code& ec) noexcept
        {
            libusb_try(ec, &::libusb_set_configuration, handle(), configuration);
        }

        template <typename CompletionToken = asio::default_completion_token_t<executor_type>>
        auto async_set_configuration(
            std::uint8_t const configuration,
            CompletionToken&& token = {})
        {
            return async_try_blocking_with_ec(
                executor_,
                service_->blocking_op_executor(),
                std::forward<CompletionToken>(token),
                [configuration, handle = handle()](auto& ec) {
                    libusb_try(ec, &::libusb_set_configuration, handle, configuration);
                });
        }

        void clear_halt(std::uint8_t const endpoint)
        {
            try_with_ec([&](auto& ec) {
                clear_halt(endpoint, ec);
            });
        }

        void clear_halt(
            std::uint8_t const endpoint,
            error_code& ec) noexcept
        {
            libusb_try(ec, &::libusb_clear_halt, handle(), endpoint);
        }

        template <typename CompletionToken = asio::default_completion_token_t<executor_type>>
        auto async_clear_halt(
            std::uint8_t const endpoint,
            CompletionToken&& token = {})
        {
            return async_try_blocking_with_ec(
                executor_,
                service_->blocking_op_executor(),
                std::forward<CompletionToken>(token),
                [endpoint, handle = handle()](auto& ec) {
                    libusb_try(ec, &::libusb_clear_halt, handle, endpoint);
                });
        }

        void reset_device()
        {
            try_with_ec([&](auto& ec) {
                reset_device(ec);
            });
        }

        void reset_device(error_code& ec) noexcept
        {
            libusb_try(ec, &::libusb_reset_device, handle());
        }

        template <typename CompletionToken = asio::default_completion_token_t<executor_type>>
        auto async_reset_device(CompletionToken&& token = {})
        {
            return async_try_blocking_with_ec(
                executor_,
                service_->blocking_op_executor(),
                std::forward<CompletionToken>(token),
                [handle = handle()](auto& ec) {
                    libusb_try(ec, &::libusb_reset_device, handle);
                });
        }

        void alloc_streams(
            std::uint32_t const num_streams,
            std::span<std::uint8_t const> const endpoints)
        {
            try_with_ec([&](auto& ec) {
                alloc_streams(num_streams, endpoints, ec);
            });
        }

        void alloc_streams(
            std::uint32_t const num_streams,
            std::span<std::uint8_t const> const endpoints,
            error_code& ec) noexcept
        {
            libusb_try(
                &::libusb_alloc_streams,
                handle(),
                num_streams,
                const_cast<unsigned char*>(
                    reinterpret_cast<unsigned char const*>(endpoints.data())),
                static_cast<int>(endpoints.size()));
        }

        void free_streams(
            std::span<std::uint8_t const> const endpoints)
        {
            try_with_ec([&](auto& ec) {
                free_streams(endpoints, ec);
            });
        }

        void free_streams(
            std::span<std::uint8_t const> const endpoints,
            error_code& ec) noexcept
        {
            libusb_try(
                &::libusb_free_streams,
                handle(),
                const_cast<unsigned char*>(
                    reinterpret_cast<unsigned char const*>(endpoints.data())),
                static_cast<int>(endpoints.size()));
        }

        [[nodiscard]] auto handle() const noexcept -> handle_type
        {
            return handle_.get();
        }

        [[nodiscard]] auto get_executor() const noexcept -> executor_type
        {
            return executor_;
        }

        [[nodiscard]] auto is_open() const noexcept -> bool
        {
            return handle_ == nullptr;
        }

        template <std::convertible_to<executor_type> OtherExecutor>
        auto operator=(basic_usb_device<OtherExecutor>&& other) noexcept -> basic_usb_device&
        {
            handle_ = std::exchange(other.handle_, nullptr);
            executor_ = other.executor_;
            service_ = other.service_;

            return *this;
        }

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return is_open();
        }

        [[nodiscard]] friend auto operator<=>(
            basic_usb_device const& lhs,
            basic_usb_device const& rhs) noexcept
            -> std::strong_ordering
        {
            return lhs.handle() <=> rhs.handle();
        }

      private:
        unique_handle_type handle_;
        executor_type executor_;
        service_type* service_;
    };

    using usb_device = basic_usb_device<>;
}  // namespace usb_asio
