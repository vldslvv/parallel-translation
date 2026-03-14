from conan import ConanFile
from conan.tools.cmake import cmake_layout

class ParallelTranslation(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeToolchain", "CMakeDeps"

    def requirements(self):
        self.requires("cli11/2.4.2")
        self.requires("catch2/3.7.1")
        self.requires("cpp-httplib/0.18.1")
        self.requires("nlohmann_json/3.11.3")
        self.requires("tomlplusplus/3.4.0")
        self.requires("spdlog/1.15.1")
        self.requires("libharu/2.4.4")
        self.requires("poppler/25.11.0")

    def layout(self):
        cmake_layout(self)
