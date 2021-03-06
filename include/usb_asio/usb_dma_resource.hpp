#pragma once

#include <algorithm>
#include <cstddef>
#include <memory_resource>
#include <vector>

#include <libusb.h>
#include "usb_asio/usb_device.hpp"

namespace usb_asio
{
    class usb_dma_resource final : public std::pmr::memory_resource
    {
      public:
        using device_handle_type = ::libusb_device_handle*;

        template <typename Executor>
        explicit usb_dma_resource(basic_usb_device<Executor>& device)
            : usb_dma_resource{device, std::pmr::get_default_resource()} { }

        template <typename Executor>
        usb_dma_resource(
            basic_usb_device<Executor>& device,
            std::pmr::memory_resource* const upstream_resource)
          : usb_dma_resource{device, upstream_resource, upstream_resource} { }

        template <typename Executor>
        usb_dma_resource(
            basic_usb_device<Executor>& device,
            std::pmr::memory_resource* const upstream_resource,
            std::pmr::memory_resource* const backup_resource)
          : device_handle_{device.handle()}
          , allocated_dma_chunks_(upstream_resource)
          , backup_resource_{backup_resource} { }

        [[nodiscard]] auto device_handle() const noexcept -> device_handle_type {
            return device_handle_;
        }

      private:
        device_handle_type device_handle_;
        std::pmr::vector<void*> allocated_dma_chunks_;
        std::pmr::memory_resource* backup_resource_;

        [[nodiscard]] auto do_allocate(
            std::size_t const bytes,
            std::size_t const alignment) -> void* override
        {
            auto ptr = ::libusb_dev_mem_alloc(device_handle(), bytes);

            if (ptr != nullptr)
            {
                // Returned pointer should be page aligned (libusb implementation on linux just calls mmap).
                // But it's better to be safe than sorry.
                // Requested alignment is guaranteed to be a power of 2.
                if (reinterpret_cast<std::uintptr_t>(ptr) & (alignment - 1u))
                {
                    // Alright then, keep your unaligned DMA buffer.
                    ::libusb_dev_mem_free(
                        device_handle(),
                        std::exchange(ptr, nullptr),
                        bytes);
                }
                else
                {
                    if (backup_resource_ != nullptr)
                    {
                        // Store the pointer, so we know that it didn't come from the backup resource.
                        allocated_dma_chunks_.push_back(ptr);
                    }

                    return ptr;
                }
            }

            if (backup_resource_ != nullptr)
            {
                return backup_resource_->allocate(bytes, alignment);
            }

            throw std::bad_alloc{};
        }

        void do_deallocate(
            void* const ptr,
            std::size_t const bytes,
            std::size_t const alignment) noexcept override
        {
            if (backup_resource_ != nullptr)
            {
                if (auto const iter = std::ranges::find(allocated_dma_chunks_, ptr);
                    iter != allocated_dma_chunks_.end())
                {
                    allocated_dma_chunks_.erase(iter);
                }
                else
                {
                    backup_resource_->deallocate(ptr, bytes, alignment);
                    return;
                }
            }

            ::libusb_dev_mem_free(
                device_handle(),
                static_cast<unsigned char*>(ptr),
                bytes);
        }

        [[nodiscard]] auto do_is_equal(
            std::pmr::memory_resource const& other) const noexcept
            -> bool override
        {
            return static_cast<std::pmr::memory_resource const*>(this)
                   == &other;
        }
    };
}  // namespace usb_asio
