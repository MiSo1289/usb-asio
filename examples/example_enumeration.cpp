#include <cstdlib>

#include <map>
#include <ranges>

#include <fmt/core.h>
#include <fmt/ranges.h>
#include <usb_asio/usb_asio.hpp>

using boost::asio::io_context;
using usb_asio::usb_device_info;
using usb_asio::list_usb_devices;

namespace
{

void print_device_tree(
    std::multimap<usb_device_info, usb_device_info> const& device_tree,
    usb_device_info const& dev,
    int const depth = 0)
{
    auto const indent = std::string(2 * depth, ' ');

    fmt::print(
        "{}- Bus {}; Port path {}; Address {}; Speed {}\n",
        indent,
        dev.bus_number(),
        dev.port_numbers(),
        dev.device_address(),
        static_cast<int>(dev.device_speed()));

    auto const dev_descriptor = dev.device_descriptor();
    fmt::print(
        "{}  VID {:#06x}; PID {:#06x}\n",
        indent,
        dev_descriptor.idVendor,
        dev_descriptor.idProduct);

    auto const [begin, end] = device_tree.equal_range(dev);
    for (auto const& child : std::ranges::subrange{begin, end}
                                 | std::views::values)
    {
        print_device_tree(device_tree, child, depth + 1);
    }
}

}  // namespace

auto main() -> int
{
    auto ioc = io_context{};
    auto devices = list_usb_devices(ioc);

    fmt::print("Found {} devices:\n", devices.size());

    auto device_tree = std::multimap<usb_device_info, usb_device_info>{};
    auto root_devices = std::vector<usb_device_info>{};

    for (auto const& dev : devices)
    {
        if (auto parent = dev.parent())
        {
            device_tree.emplace(std::move(*parent), dev);
        }
        else
        {
            root_devices.push_back(dev);
        }
    }

    for (auto& dev : root_devices)
    {
        print_device_tree(device_tree, dev);
    }

    return EXIT_SUCCESS;
}