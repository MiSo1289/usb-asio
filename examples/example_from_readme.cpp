#include <cstdint>
#include <future>
#include <iostream>
#include <string_view>

#include <usb_asio/usb_asio.hpp>

#ifdef USB_ASIO_USE_STANDALONE_ASIO

#include <asio/awaitable.hpp>
#include <asio/co_spawn.hpp>
#include <asio/use_awaitable.hpp>
#include <asio/use_future.hpp>

#else

#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/use_future.hpp>

#endif

namespace asio = usb_asio::asio;

auto main() -> int
{
    auto ctx = asio::io_context{};

    auto future = asio::co_spawn(
        ctx.get_executor(),
        [&]() -> asio::awaitable<int> {
            auto dev = usb_asio::usb_device{ctx};

            for (auto const& dev_info : usb_asio::list_usb_devices(ctx))
            {
                auto const desc = dev_info.device_descriptor();
                if (desc.idVendor == 0xABCDu && desc.idProduct == 0x1234u)
                {
                    dev.open(dev_info);
                }
            }

            if (dev.is_open())
            {
                auto const interface_number = std::uint8_t{1};
                // Claim the interface
                auto interface = usb_asio::usb_interface{
                    dev, interface_number};

                auto const alt_setting_number = std::uint8_t{2};
                // Set interface alt setting
                co_await interface.async_set_alt_setting(
                    alt_setting_number, asio::use_awaitable);

                auto const endpoint_number = std::uint8_t{3};
                auto const timeout = std::chrono::seconds{1};
                auto transfer = usb_asio::usb_in_bulk_transfer{
                    dev, endpoint_number, timeout};

                auto buff = std::array<char, 512>{};
                // Read from the bulk endpoint
                auto const n = co_await transfer.async_read_some(
                    asio::buffer(buff), asio::use_awaitable);

                std::cout << std::string_view{buff.data(), n} << "\n";

                // Unclaim the interface (otherwise the destructor would
                // do this synchronously, blocking the executor).
                // If you do not care about resetting the interface,
                // you can call interface.release() instead.
                co_await interface.async_unclaim(asio::use_awaitable);

                co_return EXIT_SUCCESS;
            }

            co_return EXIT_FAILURE;
        },
        asio::use_future);

    ctx.run();

    return future.get();
}
