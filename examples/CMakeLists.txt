add_executable(example_enumeration)
target_link_libraries(
  example_enumeration

  PRIVATE
  CONAN_PKG::fmt
  usb_asio::usb_asio
)
target_sources(example_enumeration PRIVATE example_enumeration.cpp)
