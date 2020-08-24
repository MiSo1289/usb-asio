## usb-asio
A libusb wrapper for Asio. Header-only, supports both standalone and boost versions of Asio. Requires a C++20 compiler (only tested on gcc-10). This is an early version, some features might not work as expected.

 ### Using with standalone asio
 - When using as a conan package, add `-o usb_asio:asio=standalone`.
 - When using as a cmake subproject, add `-DUSB_ASIO_USE_STANDALONE_ASIO=ON`
 - Otherwise, define `USB_ASIO_USE_STANDALONE_ASIO`.
 
 ### Example
 Find a device with a given VID and PID, and read some data from the bulk endpoint 3 at interface 1 with alt setting 2.
 ```c++
#include <usb_asio/usb_asio.hpp>

auto main() -> int {
    auto ctx = asio::io_context{};
    auto const worker = std::jthread{[&]() { ctx.run(); }};
    
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
                    device, endpoint_number, timeout};
            
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
    
    return future.get();
}
```