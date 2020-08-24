cmake_minimum_required(VERSION 3.17)
file(READ "${CMAKE_CURRENT_BINARY_DIR}/version.txt" USB_ASIO_VERSION)
project(UsbAsio VERSION "${USB_ASIO_VERSION}")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_BINARY_DIR}")
include(conanbuildinfo)

conan_basic_setup(TARGETS)

find_package(Doxygen)
if (DOXYGEN_FOUND)
  set(DOXYGEN_OUTPUT_DIRECTORY "docs")
  set(DOXYGEN_EXCLUDE_SYMBOLS "detail")
  set(DOXYGEN_JAVADOC_AUTOBRIEF YES)
  doxygen_add_docs(usb_asio_doxygen "include")
endif ()

option(USB_ASIO_USE_STANDALONE_ASIO "Use standalone asio instead of boost::asio" OFF)

add_library(usb_asio INTERFACE)
add_library(usb_asio::usb_asio ALIAS usb_asio)
target_compile_features(usb_asio INTERFACE cxx_std_20)
target_include_directories(usb_asio INTERFACE "include")
target_link_libraries(usb_asio INTERFACE CONAN_PKG::libusb)

if (USB_ASIO_USE_STANDALONE_ASIO)
  target_compile_definitions(usb_asio INTERFACE "USB_ASIO_USE_STANDALONE_ASIO")
  target_link_libraries(usb_asio INTERFACE CONAN_PKG::asio)
else ()
  target_link_libraries(usb_asio INTERFACE CONAN_PKG::boost)
endif ()

add_subdirectory(examples)
