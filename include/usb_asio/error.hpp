#pragma once

#include <concepts>
#include <functional>
#include <system_error>
#include <type_traits>

#include <libusb.h>
#include "usb_asio/asio.hpp"

namespace usb_asio
{
    enum class usb_errc : int
    {
        io = ::LIBUSB_ERROR_IO,
        invalid_param = ::LIBUSB_ERROR_INVALID_PARAM,
        access = ::LIBUSB_ERROR_ACCESS,
        no_device = ::LIBUSB_ERROR_NO_DEVICE,
        not_found = ::LIBUSB_ERROR_NOT_FOUND,
        busy = ::LIBUSB_ERROR_BUSY,
        timeout = ::LIBUSB_ERROR_TIMEOUT,
        overflow = ::LIBUSB_ERROR_OVERFLOW,
        pipe = ::LIBUSB_ERROR_PIPE,
        interrupted = ::LIBUSB_ERROR_INTERRUPTED,
        no_mem = ::LIBUSB_ERROR_NO_MEM,
        not_supported = ::LIBUSB_ERROR_NOT_SUPPORTED,
        other = ::LIBUSB_ERROR_OTHER,
    };

    enum class usb_transfer_errc : int
    {
        error = ::LIBUSB_TRANSFER_ERROR,
        timeout = ::LIBUSB_TRANSFER_TIMED_OUT,
        cancelled = ::LIBUSB_TRANSFER_CANCELLED,
        stall = ::LIBUSB_TRANSFER_STALL,
        no_device = ::LIBUSB_TRANSFER_NO_DEVICE,
        overflow = ::LIBUSB_TRANSFER_OVERFLOW,
    };
}  // namespace usb_asio

#if USB_ASIO_USE_STANDALONE_ASIO
template <>
struct std::is_error_code_enum<usb_asio::usb_errc>
  : std::true_type
{
};

template <>
struct std::is_error_code_enum<usb_asio::usb_transfer_errc>
  : std::true_type
{
};
#else
template <>
struct boost::system::is_error_code_enum<usb_asio::usb_errc>
  : std::true_type
{
};

template <>
struct boost::system::is_error_code_enum<usb_asio::usb_transfer_errc>
  : std::true_type
{
};
#endif

namespace usb_asio
{
    [[nodiscard]] inline auto usb_category() noexcept -> error_category const&
    {
        class usb_error_category final : public error_category
        {
          public:
            [[nodiscard]] auto name() const noexcept -> const char* override
            {
                return "usb";
            }

            [[nodiscard]] auto message(int const ev) const -> std::string override
            {
                return ::libusb_strerror(static_cast<::libusb_error>(ev));
            }
        };

        static auto category = usb_error_category{};
        return category;
    }

    [[nodiscard]] inline auto make_error_code(usb_errc const errc) noexcept
        -> error_code
    {
        return error_code{static_cast<int>(errc), usb_category()};
    }

    [[nodiscard]] inline auto usb_transfer_category() noexcept -> error_category const&
    {
        class usb_transfer_error_category final : public error_category
        {
          public:
            [[nodiscard]] auto name() const noexcept -> const char* override
            {
                return "usb transfer";
            }

            [[nodiscard]] auto message(int const ev) const -> std::string override
            {
                switch (static_cast<usb_transfer_errc>(ev))
                {
                case usb_transfer_errc::error:
                    return "Transfer failed";
                case usb_transfer_errc::timeout:
                    return "Transfer timed out";
                case usb_transfer_errc::cancelled:
                    return "Transfer was cancelled";
                case usb_transfer_errc::stall:
                    return "Halt condition detected or control request not supported";
                case usb_transfer_errc::no_device:
                    return "Device was disconnected";
                case usb_transfer_errc::overflow:
                    return "Device sent more data than requested";
                default:
                    return {};
                }
            }
        };

        static auto category = usb_transfer_error_category{};
        return category;
    }

    [[nodiscard]] inline auto make_error_code(usb_transfer_errc const errc) noexcept
        -> error_code
    {
        return error_code{static_cast<int>(errc), usb_transfer_category()};
    }

    // clang-format off
    template <typename Fn>
    void try_with_ec(Fn&& fn)
    requires std::invocable<Fn&&, error_code&>
        && std::is_void_v<std::invoke_result_t<Fn&&, error_code&>>
    // clang-format on
    {
        auto ec = error_code{};
        std::invoke(std::forward<Fn>(fn), ec);
        if (ec) { throw std::system_error{ec}; }
    }

    // clang-format off
    template <typename Fn>
    auto try_with_ec(Fn&& fn)
    requires std::invocable<Fn&&, error_code&>
    // clang-format on
    {
        auto ec = error_code{};
        auto result = std::invoke(std::forward<Fn>(fn), ec);
        if (ec) { throw std::system_error{ec}; }

        return result;
    }

    // clang-format off
    template <
        typename CompletionHandlerSig,
        typename Executor,
        typename BlockingOpExecutor,
        typename CompletionToken,
        typename BlockingFn>
    auto async_try_blocking_with_ec(
        Executor&& executor,
        BlockingOpExecutor&& blocking_op_executor,
        CompletionToken&& token,
        BlockingFn&& blocking_fn)
    requires std::invocable<BlockingFn&&, error_code&>
    // clang-format on
    {
        auto completion = asio::async_completion<
            CompletionToken,
            CompletionHandlerSig>{token};

        asio::post(
            std::forward<BlockingOpExecutor>(blocking_op_executor),
            [completion_handler = std::move(completion.completion_handler),
             executor = asio::prefer(
                 std::forward<Executor>(executor),
                 asio::execution::outstanding_work_t::tracked),
             blocking_fn = std::forward<BlockingFn>(blocking_fn)]() mutable {
                auto ec = error_code{};
                auto result = std::invoke(std::move(blocking_fn), ec);

                asio::post(
                    std::move(executor),
                    std::bind_front(std::move(completion_handler), ec, std::move(result)));
            });

        return completion.result.get();
    }

    // clang-format off
    template <
        typename CompletionHandlerSig,
        typename Executor,
        typename BlockingOpExecutor,
        typename CompletionToken,
        typename BlockingFn>
    auto async_try_blocking_with_ec(
        Executor&& executor,
        BlockingOpExecutor&& blocking_op_executor,
        CompletionToken&& token,
        BlockingFn&& blocking_fn)
    requires std::invocable<BlockingFn&&, error_code&>
        && std::is_void_v<std::invoke_result_t<BlockingFn&&, error_code&>>
    // clang-format on
    {
        auto completion = asio::async_completion<
            CompletionToken,
            CompletionHandlerSig>{token};

        asio::post(
            std::forward<BlockingOpExecutor>(blocking_op_executor),
            [completion_handler = std::move(completion.completion_handler),
                executor = asio::prefer(
                    std::forward<Executor>(executor),
                    asio::execution::outstanding_work_t::tracked),
             blocking_fn = std::forward<BlockingFn>(blocking_fn)]() mutable {
                auto ec = error_code{};
                std::invoke(std::move(blocking_fn), ec);

                asio::post(
                    std::move(executor),
                    std::bind_front(std::move(completion_handler), ec));
            });

        return completion.result.get();
    }

    // clang-format off
    template <
        std::signed_integral RetCode,
        typename... FnArgs,
        typename... Args>
    auto libusb_try(
        error_code& ec,
        auto (*const fn)(FnArgs...)->RetCode,
        Args const... args) noexcept
        -> std::make_unsigned_t<RetCode>
    requires (std::is_nothrow_convertible_v<Args, FnArgs> && ...)
    // clang-format on
    {
        ec.clear();

        auto const ret_code = fn(args...);
        if (ret_code < 0)
        {
            ec = make_error_code(static_cast<usb_errc>(ret_code));

            return {};
        }
        return static_cast<std::make_unsigned_t<RetCode>>(ret_code);
    }

    // clang-format off
    template <
        std::signed_integral RetCode,
        typename... FnArgs,
        typename... Args>
    auto libusb_try(auto (*const fn)(FnArgs...)->RetCode, Args const... args)
        -> std::make_unsigned_t<RetCode>
    requires (std::is_nothrow_convertible_v<Args, FnArgs> && ...)
    // clang-format on
    {
        return try_with_ec([&](auto& ec) {
            return libusb_try(ec, fn, args...);
        });
    }

}  // namespace usb_asio
