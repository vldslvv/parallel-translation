import os

from conan import ConanFile
from conan.tools.cmake import CMake, CMakeToolchain, cmake_layout
from conan.tools.files import copy
from conan.tools.scm import Git


class MorpheusConan(ConanFile):
    name = "morpheus"
    version = "0.0.3"
    package_type = "application"

    settings = "os", "arch", "compiler", "build_type"

    def package_id(self):
        self.info.settings.compiler.rm_safe("cppstd")
        self.info.settings.compiler.rm_safe("libcxx")

    def layout(self):
        cmake_layout(self)

    def source(self):
        git = Git(self)
        git.clone(url="https://github.com/vldslvv/morpheus.git", target=".")
        git.checkout(f"v{self.version}")

    def generate(self):
        tc = CMakeToolchain(self)
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()
        copy(self, "LICENSE*", self.source_folder, os.path.join(self.package_folder, "licenses"))

    def package_info(self):
        stemlib = os.path.join(self.package_folder, "res", "stemlib")
        libexec = os.path.join(self.package_folder, "libexec", "morpheus")

        self.cpp_info.includedirs = []
        self.cpp_info.libdirs = []
        self.cpp_info.bindirs = ["bin"]
        self.cpp_info.resdirs = ["res"]

        self.runenv_info.define_path("MORPHLIB", stemlib)
        self.runenv_info.prepend_path("PATH", libexec)
        self.buildenv_info.define_path("MORPHLIB", stemlib)
        self.buildenv_info.prepend_path("PATH", libexec)
