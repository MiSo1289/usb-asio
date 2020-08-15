#pragma once

#include <memory>

namespace usb_asio
{
    template <typename LibUsbType, void (*delete_libusb_fn)(LibUsbType*)>
    struct libusb_deleter
    {
        void operator()(LibUsbType* const libusb_handle) noexcept
        {
            delete_libusb_fn(libusb_handle);
        }
    };

    template <typename LibUsbType, void (*delete_libusb_fn)(LibUsbType*)>
    using libusb_ptr = std::unique_ptr<LibUsbType, libusb_deleter<LibUsbType, delete_libusb_fn>>;

    struct adopt_ref_t
    {
    };
    inline constexpr auto adopt_ref = adopt_ref_t{};

    template <
        typename LibUsbType,
        auto (*ref_libusb_fn)(LibUsbType*)->LibUsbType*,
        void (*unref_libusb_fn)(LibUsbType*)>
    class libusb_ref_ptr
    {
      public:
        using pointer = LibUsbType*;

        libusb_ref_ptr() noexcept = default;

        libusb_ref_ptr(std::nullptr_t) noexcept
          : libusb_ref_ptr{} { }

        libusb_ref_ptr(pointer const ptr) noexcept
          : ptr_{ref_libusb_fn(ptr)}
        {
        }

        libusb_ref_ptr(pointer const ptr, adopt_ref_t) noexcept
          : ptr_{ptr}
        {
        }

        libusb_ref_ptr(libusb_ref_ptr const& other) noexcept
          : libusb_ref_ptr{other.ptr_.get()}
        {
        }

        libusb_ref_ptr(libusb_ref_ptr&& other) noexcept = default;

        auto operator=(libusb_ref_ptr other) noexcept -> libusb_ref_ptr&
        {
            std::swap(ptr_, other.ptr_);
            return *this;
        }

        [[nodiscard]] auto get() const noexcept -> pointer
        {
            return ptr_.get();
        }

        [[nodiscard]] auto release() const noexcept -> pointer
        {
            return ptr_.release();
        }

        void reset() noexcept
        {
            ptr_.reset();
        }

        friend auto operator<=>(libusb_ref_ptr const&, libusb_ref_ptr const&) = default;

      private:
        libusb_ptr<LibUsbType, unref_libusb_fn> ptr_;
    };
}
