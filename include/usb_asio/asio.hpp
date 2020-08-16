#pragma once

#ifndef USB_ASIO_USE_STANDALONE_ASIO
#define USB_ASIO_USE_STANDALONE_ASIO 0
#endif

#if USB_ASIO_USE_STANDALONE_ASIO

#include <system_error>

#include <asio/async_result.hpp>
#include <asio/buffer.hpp>
#include <asio/execution_context.hpp>
#include <asio/executor.hpp>
#include <asio/executor_work_guard.hpp>
#include <asio/io_context.hpp>
#include <asio/post.hpp>

#else

#include <boost/asio/async_result.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/execution_context.hpp>
#include <boost/asio/executor.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>
#include <boost/system/error_code.hpp>

#endif

namespace usb_asio
{
#if USB_ASIO_USE_STANDALONE_ASIO
    namespace asio = ::asio;

    using error_code = std::error_code;
    using error_category = std::error_category;
#else
    namespace asio = boost::asio;

    using error_code = boost::system::error_code;
    using error_category = boost::system::error_category;
#endif
}  // namespace usb_asio