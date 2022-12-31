#pragma once

#include <algorithm>
#include <chrono>
#include <concepts>
#include <cstddef>
#include <memory_resource>
#include <ranges>
#include <span>
#include <stdexcept>
#include <vector>

#include <libusb.h>
#include "usb_asio/asio.hpp"
#include "usb_asio/error.hpp"
#include "usb_asio/usb_device.hpp"

namespace usb_asio
{
    class usb_control_transfer_buffer
    {
      public:
        explicit usb_control_transfer_buffer(std::size_t const size)
          : usb_control_transfer_buffer{size, std::pmr::get_default_resource()} { }

        usb_control_transfer_buffer(
            std::size_t const size,
            std::pmr::memory_resource* const mem_resource)
          : data_(((size - 1u) / 2u) + 1u + LIBUSB_CONTROL_SETUP_SIZE, mem_resource) { }

        [[nodiscard]] auto payload() noexcept -> std::span<std::byte>
        {
            return std::as_writable_bytes(std::span{data_})
                .subspan(LIBUSB_CONTROL_SETUP_SIZE);
        }

        [[nodiscard]] auto payload() const noexcept -> std::span<std::byte const>
        {
            return std::as_bytes(std::span{data_})
                .subspan(LIBUSB_CONTROL_SETUP_SIZE);
        }

        [[nodiscard]] auto data() noexcept -> std::byte*
        {
            return payload().data();
        }

        [[nodiscard]] auto data() const noexcept -> std::byte const*
        {
            return payload().data();
        }

        [[nodiscard]] auto size() const noexcept -> std::size_t
        {
            return payload().size();
        }

      private:
        std::pmr::vector<std::uint16_t> data_;
    };

    inline constexpr auto usb_no_timeout = std::chrono::milliseconds{0};

    struct usb_iso_packet_transfer_result
    {
        std::size_t transferred;
        error_code ec;
    };

    template <
        usb_transfer_type transfer_type,
        usb_transfer_direction transfer_direction>
    struct usb_transfer_traits
    {
        using result_type = std::size_t;
        struct result_storage_type
        {
        };
    };

    template <usb_transfer_direction transfer_direction>
    struct usb_transfer_traits<usb_transfer_type::isochronous, transfer_direction>
    {
        using result_type = std::span<usb_iso_packet_transfer_result const>;
        using result_storage_type = std::vector<usb_iso_packet_transfer_result>;
    };

    template <
        usb_transfer_type transfer_type_,
        usb_transfer_direction transfer_direction_,
        typename Executor = asio::any_io_executor>
    class basic_usb_transfer
    {
      public:
        using handle_type = ::libusb_transfer*;
        using unique_handle_type = libusb_ptr<::libusb_transfer, &::libusb_free_transfer>;
        using executor_type = Executor;
        using traits_type = usb_transfer_traits<transfer_type_, transfer_direction_>;
        using result_type = typename traits_type::result_type;
        using completion_handler_sig = void(error_code, result_type);

        static constexpr auto transfer_type = transfer_type_;
        static constexpr auto transfer_direction = transfer_direction_;

        // clang-format off
        template <typename OtherExecutor>
        basic_usb_transfer(
            executor_type const& executor,
            basic_usb_device<OtherExecutor>& device,
            std::chrono::milliseconds const timeout = usb_no_timeout)
        requires (transfer_type == usb_transfer_type::control)
          // clang-format on
          : handle_{::libusb_alloc_transfer(0)}
          , executor_{executor}
          , completion_context_{std::make_unique<completion_context>()}
        {
            check_is_constructed();

            ::libusb_fill_control_transfer(
                handle(),
                device.handle(),
                nullptr,
                &completion_callback,
                completion_context_.get(),
                static_cast<unsigned>(timeout.count()));
        }

        // clang-format off
        template <std::convertible_to<executor_type> OtherExecutor>
        explicit basic_usb_transfer(
            basic_usb_device<OtherExecutor>& device,
            std::chrono::milliseconds const timeout = usb_no_timeout)
        requires (transfer_type == usb_transfer_type::control)
          // clang-format on
          : basic_usb_transfer{device.get_executor(), timeout}
        {
        }

        // clang-format off
        template <typename OtherExecutor, typename PacketSizeRange>
        basic_usb_transfer(
            executor_type const& executor,
            basic_usb_device<OtherExecutor>& device,
            std::uint8_t const endpoint,
            PacketSizeRange&& packet_sizes,
            std::chrono::milliseconds const timeout = usb_no_timeout)
        requires (transfer_type == usb_transfer_type::isochronous)
            && std::ranges::input_range<PacketSizeRange>
            && std::ranges::sized_range<PacketSizeRange>
            && std::unsigned_integral<std::ranges::range_value_t<PacketSizeRange>>
          // clang-format on
          : handle_{::libusb_alloc_transfer(static_cast<int>(std::ranges::size(packet_sizes)))},
            executor_{executor}, completion_context_{std::make_unique<completion_context>()}
        {
            check_is_constructed();

            auto const num_packets = std::ranges::size(packet_sizes);
            completion_context_->result_storage.resize(num_packets);

            auto packet = std::size_t{0};
            for (auto const packet_size : packet_sizes)
            {
                handle()->iso_packet_desc[packet++].length = static_cast<unsigned>(packet_size);
            }

            ::libusb_fill_iso_transfer(
                handle(),
                device.handle(),
                endpoint,
                nullptr,
                0,
                static_cast<int>(num_packets),
                &completion_callback,
                completion_context_.get(),
                static_cast<unsigned>(timeout.count()));
        }

        // clang-format off
        template <typename OtherExecutor>
        basic_usb_transfer(
            executor_type const& executor,
            basic_usb_device<OtherExecutor>& device,
            std::uint8_t const endpoint,
            std::size_t const num_packets,
            std::size_t const packet_size,
            std::chrono::milliseconds const timeout = usb_no_timeout)
        requires (transfer_type == usb_transfer_type::isochronous)
          // clang-format on
          : basic_usb_transfer{
              executor,
              device,
              endpoint,
              std::views::iota(std::size_t{0}, num_packets)
                  | std::views::transform([&](auto) { return packet_size; }),
              timeout,
          }
        {
        }

        // clang-format off
        template <std::convertible_to<executor_type> OtherExecutor, typename PacketSizeRange>
        basic_usb_transfer(
            basic_usb_device<OtherExecutor>& device,
            std::uint8_t const endpoint,
            PacketSizeRange&& packet_sizes,
            std::chrono::milliseconds const timeout = usb_no_timeout)
        requires (transfer_type == usb_transfer_type::isochronous)
                 && std::ranges::input_range<PacketSizeRange>
                 && std::ranges::sized_range<PacketSizeRange>
                 && std::unsigned_integral<std::ranges::range_value_t<PacketSizeRange>>
          // clang-format on
          : basic_usb_transfer{
                device.get_executor(),
                device,
                endpoint,
                std::forward<PacketSizeRange>(packet_sizes),
                timeout,
            }
        {
        }

        // clang-format off
        template <std::convertible_to<executor_type> OtherExecutor>
        basic_usb_transfer(
            basic_usb_device<OtherExecutor>& device,
            std::uint8_t const endpoint,
            std::size_t const num_packets,
            std::size_t const packet_size,
            std::chrono::milliseconds const timeout = usb_no_timeout)
        requires (transfer_type == usb_transfer_type::isochronous)
          // clang-format on
          : basic_usb_transfer{
                device.get_executor(),
                device,
                endpoint,
                num_packets,
                packet_size,
                timeout,
            }
        {
        }

        // clang-format off
        template <typename OtherExecutor>
        basic_usb_transfer(
            executor_type const& executor,
            basic_usb_device<OtherExecutor>& device,
            std::uint8_t const endpoint,
            std::chrono::milliseconds const timeout = usb_no_timeout)
        requires (transfer_type == usb_transfer_type::bulk)
          // clang-format on
          : handle_{::libusb_alloc_transfer(0)}
          , executor_{executor}
          , completion_context_{std::make_unique<completion_context>()}
        {
            check_is_constructed();

            ::libusb_fill_bulk_transfer(
                handle(),
                device.handle(),
                endpoint,
                nullptr,
                0,
                &completion_callback,
                completion_context_.get(),
                static_cast<unsigned>(timeout.count()));
        }

        // clang-format off
        template <std::convertible_to<executor_type> OtherExecutor>
        basic_usb_transfer(
            basic_usb_device<OtherExecutor>& device,
            std::uint8_t const endpoint,
            std::chrono::milliseconds const timeout = usb_no_timeout)
        requires (transfer_type == usb_transfer_type::bulk)
          // clang-format on
          : basic_usb_transfer{
              device.get_executor(),
              device,
              endpoint,
              timeout,
          }
        {
        }

        // clang-format off
        template <typename OtherExecutor>
        basic_usb_transfer(
            executor_type const& executor,
            basic_usb_device<OtherExecutor>& device,
            std::uint8_t const endpoint,
            std::chrono::milliseconds const timeout = usb_no_timeout)
        requires (transfer_type == usb_transfer_type::interrupt)
          // clang-format on
          : handle_{::libusb_alloc_transfer(0)}
          , executor_{executor}
          , completion_context_{std::make_unique<completion_context>()}
        {
            check_is_constructed();

            ::libusb_fill_interrupt_transfer(
                handle(),
                device.handle(),
                endpoint,
                nullptr,
                0,
                &completion_callback,
                completion_context_.get(),
                static_cast<unsigned>(timeout.count()));
        }

        // clang-format off
        template <std::convertible_to<executor_type> OtherExecutor>
        basic_usb_transfer(
            basic_usb_device<OtherExecutor>& device,
            std::uint8_t const endpoint,
            std::chrono::milliseconds const timeout = usb_no_timeout)
        requires (transfer_type == usb_transfer_type::interrupt)
          // clang-format on
          : basic_usb_transfer{
              device.get_executor(),
              device,
              endpoint,
              timeout,
          }
        {
        }

        // clang-format off
        template <typename OtherExecutor>
        basic_usb_transfer(
            executor_type const& executor,
            basic_usb_device<OtherExecutor>& device,
            std::uint8_t const endpoint,
            std::uint32_t const stream_id,
            std::chrono::milliseconds const timeout = usb_no_timeout)
        requires (transfer_type == usb_transfer_type::bulk_stream)
          // clang-format on
          : handle_{::libusb_alloc_transfer(0)}
          , executor_{executor}
          , completion_context_{std::make_unique<completion_context>()}
        {
            check_is_constructed();

            ::libusb_fill_bulk_stream_transfer(
                handle(),
                device.handle(),
                endpoint,
                stream_id,
                nullptr,
                0,
                &completion_callback,
                completion_context_.get(),
                static_cast<unsigned>(timeout.count()));
        }

        // clang-format off
        template <std::convertible_to<executor_type> OtherExecutor>
        basic_usb_transfer(
            basic_usb_device<OtherExecutor>& device,
            std::uint8_t const endpoint,
            std::uint32_t const stream_id,
            std::chrono::milliseconds const timeout = usb_no_timeout)
        requires (transfer_type == usb_transfer_type::bulk_stream)
          // clang-format on
          : basic_usb_transfer{
              device.get_executor(),
              device,
              endpoint,
              stream_id,
              timeout,
          }
        {
        }

        [[nodiscard]] auto handle() const noexcept -> handle_type
        {
            return handle_.get();
        }

        void cancel()
        {
            try_with_ec([&](auto& ec) {
                cancel(ec);
            });
        }

        void cancel(error_code& ec) noexcept
        {
            libusb_try(ec, ::libusb_cancel_transfer, handle());
        }

        // clang-format off
        template <typename CompletionToken = asio::default_completion_token_t<executor_type>>
        auto async_read_some(asio::mutable_buffer const buffer, CompletionToken&& token = {})
        requires (transfer_direction == usb_transfer_direction::in)
            && (transfer_type != usb_transfer_type::control)
        // clang-format on
        {
            handle()->buffer = static_cast<unsigned char*>(buffer.data());
            handle()->length = static_cast<int>(buffer.size());

            return async_submit_impl(std::forward<CompletionToken>(token));
        }

        // clang-format off
        template <typename CompletionToken = asio::default_completion_token_t<executor_type>>
        auto async_write_some(asio::const_buffer const buffer, CompletionToken&& token = {})
        requires (transfer_direction == usb_transfer_direction::out)
            && (transfer_type != usb_transfer_type::control)
        // clang-format on
        {
            handle()->buffer = static_cast<unsigned char*>(const_cast<void*>(buffer.data()));
            handle()->length = static_cast<int>(buffer.size());

            return async_submit_impl(std::forward<CompletionToken>(token));
        }

        // clang-format off
        template <typename CompletionToken = asio::default_completion_token_t<executor_type>>
        auto async_control(
            usb_control_request_recipient const recipient,
            usb_control_request_type const type,
            std::uint8_t const request,
            std::uint16_t const value,
            std::uint8_t const index,
            usb_control_transfer_buffer& buffer,
            CompletionToken&& token = {})
        requires (transfer_type == usb_transfer_type::control)
        // clang-format on
        {
            handle()->buffer = reinterpret_cast<unsigned char*>(buffer.data());
            handle()->length = static_cast<int>(buffer.size() + LIBUSB_CONTROL_SETUP_SIZE);

            ::libusb_fill_control_setup(
                reinterpret_cast<unsigned char*>(buffer.data() - LIBUSB_CONTROL_SETUP_SIZE),
                static_cast<std::uint8_t>(
                    static_cast<unsigned>(recipient)
                    | static_cast<unsigned>(type)
                    | static_cast<unsigned>(transfer_direction)),
                request,
                value,
                index,
                buffer.size());

            return async_submit_impl(std::forward<CompletionToken>(token));
        }

      private:
        class completion_handler_t
        {
          public:
            completion_handler_t() = default;

            template <
                std::invocable<error_code, result_type> T>
            completion_handler_t(Executor const& executor, T&& handler)
            {
                auto const trackedEx = asio::prefer(executor, asio::execution::outstanding_work.tracked);
                auto const trackedCompletionEx = asio::prefer(
                    asio::get_associated_executor(handler, executor),
                    asio::execution::outstanding_work.tracked);

                impl_ = std::make_unique<handler_impl<
                    T,
                    std::decay_t<decltype(trackedEx)>,
                    std::decay_t<decltype(trackedCompletionEx)>>>(
                    trackedEx,
                    trackedCompletionEx,
                    std::move(handler));
            }

            void operator()(error_code const ec, result_type result)
            {
                (*impl_)(ec, std::move(result));
            }

            void reset() noexcept
            {
                impl_.reset();
            }

          private:
            struct erased_handler
            {
                virtual ~erased_handler() noexcept = default;

                virtual void operator()(error_code ec, result_type&& result) = 0;
            };

            template <std::invocable<error_code, result_type> T,
                      typename TrackedExecutor,
                      typename TrackedCompletionExecutor>
            struct handler_impl final : erased_handler
            {
                TrackedExecutor executor;
                TrackedCompletionExecutor completion_executor;
                T handler;

                handler_impl(
                    TrackedExecutor const& executor,
                    TrackedCompletionExecutor const& completion_executor,
                    T&& handler)
                  : executor{executor}
                  , completion_executor{completion_executor}
                  , handler{std::move(handler)} { }

                void operator()(error_code const ec, result_type&& result) override
                {
                    asio::post(
                        completion_executor,
                        std::bind_front(std::move(handler), ec, std::move(result)));
                }
            };

            std::unique_ptr<erased_handler> impl_;
        };

        struct completion_context
        {
            [[no_unique_address]] typename traits_type::result_storage_type result_storage = {};
            completion_handler_t handler = {};
        };

        unique_handle_type handle_;
        executor_type executor_;
        std::unique_ptr<completion_context> completion_context_;

        static void completion_callback(handle_type const handle) noexcept
        {
            auto const ec = error_code{
                static_cast<usb_transfer_errc>(handle->status),
            };
            auto& context = *static_cast<completion_context*>(handle->user_data);

            auto const result = [&]() {
                if constexpr (transfer_type == usb_transfer_type::isochronous)
                {
                    std::ranges::transform(
                        std::span{
                            handle->iso_packet_desc,
                            static_cast<std::size_t>(handle->num_iso_packets),
                        },
                        context.result_storage.begin(),
                        [](auto const& packet_desc) {
                            return usb_iso_packet_transfer_result{
                                static_cast<std::size_t>(packet_desc.actual_length),
                                static_cast<usb_transfer_errc>(packet_desc.status),
                            };
                        });
                    return std::span{context.result_storage};
                }
                else
                {
                    return handle->actual_length;
                }
            }();

            context.handler(ec, result);
            context.handler.reset();
        }

        template <typename CompletionToken>
        auto async_submit_impl(CompletionToken&& token)
        {
            return asio::async_initiate<CompletionToken, completion_handler_sig>(
                [](auto completion_handler, auto const handle, auto* const context, auto const& executor) {
                    context->handler = completion_handler_t{executor, std::move(completion_handler)};

                    auto ec = error_code{};
                    libusb_try(ec, &::libusb_submit_transfer, handle);

                    if (ec)
                    {
                        // Error in submission
                        context->handler(ec, result_type{});
                        context->handler.reset();
                    }
                },
                std::forward<CompletionToken>(token),
                handle(),
                completion_context_.get(),
                executor_);
        }

        void check_is_constructed() const
        {
            if (handle_ == nullptr)
            {
                throw std::bad_alloc{};
            }
        }
    };

    template <typename Executor = asio::any_io_executor>
    using basic_usb_out_control_transfer = basic_usb_transfer<
        usb_transfer_type::control,
        usb_transfer_direction::out>;
    using usb_out_control_transfer = basic_usb_out_control_transfer<>;

    template <typename Executor = asio::any_io_executor>
    using basic_usb_in_control_transfer = basic_usb_transfer<
        usb_transfer_type::control,
        usb_transfer_direction::in>;
    using usb_in_control_transfer = basic_usb_in_control_transfer<>;

    template <typename Executor = asio::any_io_executor>
    using basic_usb_out_isochronous_transfer = basic_usb_transfer<
        usb_transfer_type::isochronous,
        usb_transfer_direction::out>;
    using usb_out_isochronous_transfer = basic_usb_out_isochronous_transfer<>;

    template <typename Executor = asio::any_io_executor>
    using basic_usb_in_isochronous_transfer = basic_usb_transfer<
        usb_transfer_type::isochronous,
        usb_transfer_direction::in>;
    using usb_in_isochronous_transfer = basic_usb_in_isochronous_transfer<>;

    template <typename Executor = asio::any_io_executor>
    using basic_usb_out_bulk_transfer = basic_usb_transfer<
        usb_transfer_type::bulk,
        usb_transfer_direction::out>;
    using usb_out_bulk_transfer = basic_usb_out_bulk_transfer<>;

    template <typename Executor = asio::any_io_executor>
    using basic_usb_in_bulk_transfer = basic_usb_transfer<
        usb_transfer_type::bulk,
        usb_transfer_direction::in>;
    using usb_in_bulk_transfer = basic_usb_in_bulk_transfer<>;

    template <typename Executor = asio::any_io_executor>
    using basic_usb_out_interrupt_transfer = basic_usb_transfer<
        usb_transfer_type::interrupt,
        usb_transfer_direction::out>;
    using usb_out_interrupt_transfer = basic_usb_out_interrupt_transfer<>;

    template <typename Executor = asio::any_io_executor>
    using basic_usb_in_interrupt_transfer = basic_usb_transfer<
        usb_transfer_type::interrupt,
        usb_transfer_direction::in>;
    using usb_in_interrupt_transfer = basic_usb_in_interrupt_transfer<>;

    template <typename Executor = asio::any_io_executor>
    using basic_usb_out_bulk_stream_transfer = basic_usb_transfer<
        usb_transfer_type::bulk_stream,
        usb_transfer_direction::out>;
    using usb_out_bulk_stream_transfer = basic_usb_out_bulk_stream_transfer<>;

    template <typename Executor = asio::any_io_executor>
    using basic_usb_in_bulk_stream_transfer = basic_usb_transfer<
        usb_transfer_type::bulk_stream,
        usb_transfer_direction::in>;
    using usb_in_bulk_stream_transfer = basic_usb_in_bulk_stream_transfer<>;
}  // namespace usb_asio
