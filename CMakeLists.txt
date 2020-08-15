cmake_minimum_required(VERSION 3.17)
file(READ "${CMAKE_CURRENT_BINARY_DIR}/version.txt" USB_ASIO_VERSION)
project(UsbAsio VERSION "${USB_ASIO_VERSION}")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_BINARY_DIR}")
include(conanbuildinfo)

conan_basic_setup(TARGETS)

find_package(Doxygen)
if(DOXYGEN_FOUND)
  set(DOXYGEN_OUTPUT_DIRECTORY "docs")
  set(DOXYGEN_EXCLUDE_SYMBOLS "detail")
  set(DOXYGEN_JAVADOC_AUTOBRIEF YES)
  doxygen_add_docs(usb_asio_doxygen "include")
endif()

add_library(usb_asio INTERFACE)
add_library(usb_asio::usb_asio ALIAS usb_asio)
target_compile_features(usb_asio INTERFACE cxx_std_20)
target_include_directories(usb_asio INTERFACE "include")
target_link_libraries(
  usb_asio

  INTERFACE
  CONAN_PKG::boost
  CONAN_PKG::libusb
)

add_subdirectory(examples)
