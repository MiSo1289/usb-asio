add_library(example_base INTERFACE)
target_link_libraries(
  example_base

  INTERFACE
  CONAN_PKG::fmt
  usb_asio::usb_asio
)
target_compile_options(
  example_base

  INTERFACE
  -fcoroutines
)

if (USB_ASIO_USE_STANDALONE_ASIO)
  target_compile_definitions(example_base INTERFACE "ASIO_NO_TS_EXECUTORS")
else ()
  target_compile_definitions(example_base INTERFACE "BOOST_ASIO_NO_TS_EXECUTORS")
endif ()

add_executable(example_enumeration)
target_link_libraries(example_enumeration PRIVATE example_base)
target_sources(example_enumeration PRIVATE example_enumeration.cpp)

add_executable(example_from_readme)
target_link_libraries(example_from_readme PRIVATE example_base)
target_sources(example_from_readme PRIVATE example_from_readme.cpp)
