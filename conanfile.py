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
        "examples": [True, False]
    }
    default_options = {
        "examples": False,
    }
    requires = (
        "boost/1.73.0",
        "libusb/1.0.23",
    )

    def imports(self):
        with open("version.txt", "w") as version_file:
            version_file.write(str(self.version))

    def requirements(self):
        if self.options.examples:
            self.requires("fmt/7.0.1")

    def build(self):
        if self.options.examples:
            cmake = CMake(self)
            cmake.configure()
            cmake.build()

    def package(self):
        self.copy("*.hpp", dst="include", src="include")

    def package_id(self):
        del self.options.examples

        self.info.header_only()
