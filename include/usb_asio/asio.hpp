#pragma once

#ifdef USB_ASIO_USE_STANDALONE_ASIO

#include <system_error>

#include <asio/any_io_executor.hpp>
#include <asio/async_result.hpp>
#include <asio/buffer.hpp>
#include <asio/execution_context.hpp>
#include <asio/io_context.hpp>
#include <asio/post.hpp>

#else

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/async_result.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/execution_context.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>
#include <boost/system/error_code.hpp>
#include <boost/system/system_error.hpp>

#endif

namespace usb_asio
{
#ifdef USB_ASIO_USE_STANDALONE_ASIO
    namespace asio = ::asio;

    using error_code = std::error_code;
    using error_category = std::error_category;
    using system_error = std::system_error;
#else
    namespace asio = boost::asio;

    using error_code = boost::system::error_code;
    using error_category = boost::system::error_category;
    using system_error = boost::system::system_error;
#endif
}  // namespace usb_asio
