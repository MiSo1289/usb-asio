#pragma once

#include <algorithm>
#include <span>
#include <thread>
#include <vector>

#include <boost/asio/async_result.hpp>
#include <boost/asio/execution_context.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>
#include <libusb.h>
#include "usb_asio/error.hpp"
#include "usb_asio/libusb_ptr.hpp"
#include "usb_asio/usb_device_info.hpp"

namespace usb_asio
{
    class usb_service final : public boost::asio::execution_context::service
    {
      public:
        using handle_type = ::libusb_context*;
        using unique_handle_type = libusb_ptr<::libusb_context, &::libusb_exit>;
        using blocking_op_executor_type = boost::asio::io_context::executor_type;
        using key_type = usb_service;

        explicit usb_service(boost::asio::execution_context& context)
          : boost::asio::execution_context::service{context}
          , handle_{create()}
          , blocking_op_thread_{[this]() { blocking_op_ioc_.run(); }}
          , blocking_op_work_guard_{blocking_op_ioc_.get_executor()}
        {
        }

        usb_service(usb_service const&) = delete;

        usb_service(usb_service&&) = delete;

        void shutdown() noexcept override
        {
        }

        [[nodiscard]] auto handle() const noexcept -> handle_type
        {
            return handle_.get();
        }

        [[nodiscard]] auto blocking_op_executor() noexcept -> blocking_op_executor_type
        {
            return blocking_op_ioc_.get_executor();
        }

        [[nodiscard]] auto list_devices() -> std::vector<usb_device_info>
        {
            return try_with_ec([&](auto& ec) {
                return list_devices(ec);
            });
        }

        [[nodiscard]] auto list_devices(std::error_code& ec)
            -> std::vector<usb_device_info>
        {
            auto device_handles = static_cast<usb_device_info::handle_type*>(nullptr);
            auto const num_devices = libusb_try(
                ec,
                &::libusb_get_device_list,
                handle_.get(),
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

        void notify_dev_opened()
        {
            if (open_devices_.fetch_add(1) == 0)
            {
                auto lock = std::scoped_lock{usb_event_thread_startup_mutex_};
                if (usb_event_thread_.joinable())
                {
                    usb_event_thread_.join();
                }

                usb_event_thread_ = std::jthread{&run_usb_event_thread, handle_.get()};
            }
        }

        void notify_dev_closed() noexcept
        {
            --open_devices_;
        }

        auto operator=(usb_service const&) = delete;

        auto operator=(usb_service&&) = delete;

        friend auto operator<=>(usb_service const& lhs, usb_service const& rhs) noexcept
            -> std::strong_ordering
        {
            return lhs.handle_ <=> rhs.handle_;
        }

      private:
        unique_handle_type handle_;
        std::atomic<std::size_t> open_devices_ = 0;
        std::mutex usb_event_thread_startup_mutex_;
        std::jthread usb_event_thread_;
        boost::asio::io_context blocking_op_ioc_;
        std::jthread blocking_op_thread_;
        boost::asio::executor_work_guard<blocking_op_executor_type>
            blocking_op_work_guard_;

        static void run_usb_event_thread(
            std::stop_token const& stop_token,
            ::libusb_context* const libusb_context) noexcept
        {
            while (!stop_token.stop_requested())
            {
                ::libusb_handle_events(libusb_context);
            }
        }

        [[nodiscard]] static auto create() -> unique_handle_type
        {
            auto handle = handle_type{};
            libusb_try(&::libusb_init, &handle);
            return unique_handle_type{handle};
        }
    };

    [[nodiscard]] auto list_usb_devices(boost::asio::execution_context& context)
        -> std::vector<usb_device_info>
    {
        return boost::asio::use_service<usb_service>(context).list_devices();
    }

    [[nodiscard]] auto list_usb_devices(
        boost::asio::execution_context& context,
        std::error_code& ec)
        -> std::vector<usb_device_info>
    {
        return boost::asio::use_service<usb_service>(context).list_devices(ec);
    }
}  // namespace usb_asio
