add_executable(example_enumeration)
target_link_libraries(
  example_enumeration

  PRIVATE
  CONAN_PKG::fmt
  usb_asio::usb_asio
)
target_sources(example_enumeration PRIVATE example_enumeration.cpp)

if (USB_ASIO_USE_STANDALONE_ASIO)
  target_compile_definitions(example_enumeration PRIVATE "ASIO_NO_TS_EXECUTORS")
else ()
  target_compile_definitions(example_enumeration PRIVATE "BOOST_ASIO_NO_TS_EXECUTORS")
endif ()
