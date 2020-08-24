#pragma once

#include <condition_variable>
#include <mutex>
#include <thread>

#include <libusb.h>
#include "usb_asio/asio.hpp"
#include "usb_asio/error.hpp"
#include "usb_asio/libusb_ptr.hpp"

namespace usb_asio
{
    class usb_service final : public asio::execution_context::service
    {
      public:
        using handle_type = ::libusb_context*;
        using unique_handle_type = libusb_ptr<::libusb_context, &::libusb_exit>;
        using key_type = usb_service;

        explicit usb_service(asio::execution_context& context)
          : asio::execution_context::service{context}
          , handle_{create()}
          , usb_event_thread_{[this](auto const& stop_token) {
              run_usb_event_thread(stop_token);
          }}
          , blocking_op_thread_{[this]() { blocking_op_ioc_.run(); }}
          , blocking_op_executor_{
                asio::require(
                    blocking_op_ioc_.get_executor(),
                    asio::execution::outstanding_work_t::tracked),
            }
        {
        }

        usb_service(usb_service const&) = delete;

        usb_service(usb_service&&) = delete;

        void shutdown() noexcept override
        {
            usb_event_thread_.request_stop();
            usb_event_loop_cv_.notify_one();
        }

        [[nodiscard]] auto handle() const noexcept -> handle_type
        {
            return handle_.get();
        }

        [[nodiscard]] auto blocking_op_executor() noexcept
        {
            return blocking_op_executor_;
        }

        void notify_dev_opened()
        {
            auto lock = std::unique_lock{usb_event_loop_mutex_};
            if (open_devices_.fetch_add(1) == 0)
            {
                lock.unlock();
                usb_event_loop_cv_.notify_one();
            }
        }

        void notify_dev_closed() noexcept
        {
            --open_devices_;
        }

        auto operator=(usb_service const&) = delete;

        auto operator=(usb_service&&) = delete;

        friend auto operator<=>(usb_service const& lhs, usb_service const& rhs) noexcept
        {
            return lhs.handle() <=> rhs.handle();
        }

      private:
        unique_handle_type handle_;
        std::atomic<std::size_t> open_devices_ = 0;
        std::mutex usb_event_loop_mutex_;
        std::condition_variable usb_event_loop_cv_;
        std::jthread usb_event_thread_;
        asio::io_context blocking_op_ioc_;
        std::jthread blocking_op_thread_;
        asio::any_io_executor blocking_op_executor_;

        void run_usb_event_thread(std::stop_token const& stop_token) noexcept
        {
            while (true)
            {
                {
                    auto lock = std::unique_lock{usb_event_loop_mutex_};
                    usb_event_loop_cv_.wait(lock, [&]() {
                        return open_devices_ > 0 || stop_token.stop_requested();
                    });
                }

                if (stop_token.stop_requested())
                {
                    break;
                }

                ::libusb_handle_events(handle());
            }
        }

        [[nodiscard]] static auto create() -> unique_handle_type
        {
            auto handle = handle_type{};
            libusb_try(&::libusb_init, &handle);
            return unique_handle_type{handle};
        }
    };
}  // namespace usb_asio
