from conans import ConanFile, CMake


class UsbAsio(ConanFile):
    name = "usb_asio"
    version = "0.1.0"
    revision_mode = "scm"
    generators = "cmake"
    exports_sources = (
        "include/*",
    )
    options = {
        "asio": ["boost", "standalone"],
        "examples": [True, False],
    }
    default_options = {
        "asio": "boost",
        "examples": False,
    }
    requires = (
        "libusb/1.0.23",
    )

    def imports(self):
        with open("version.txt", "w") as version_file:
            version_file.write(str(self.version))

    def requirements(self):
        if self.options.asio == "boost":
            self.requires("boost/1.74.0")
        else:
            self.requires("asio/1.17.0")

        if self.options.examples:
            self.requires("fmt/7.0.1")

    def build(self):
        if self.options.examples:
            cmake = CMake(self)
            cmake.definitions["USB_ASIO_USE_STANDALONE_ASIO"] \
                = self.options.asio == "standalone"
            cmake.configure()
            cmake.build()

    def package(self):
        self.copy("*.hpp", dst="include", src="include")

    def package_id(self):
        del self.options.examples

        self.info.header_only()

    def package_info(self):
        self.cpp_info.defines = ["USB_ASIO_USE_STANDALONE_ASIO"]
