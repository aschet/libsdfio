# SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
#
# SPDX-License-Identifier: MIT

"""Conan 2 recipe building libsdfio from this working tree.

Use it to consume the library from a local checkout::

    conan create .

The recipe published to Conan Center is a different one, built from a release
tarball and maintained in the conan-center-index repository.
"""

import os
import re

from conan import ConanFile
from conan.errors import ConanException
from conan.tools.cmake import CMake, CMakeToolchain, cmake_layout
from conan.tools.files import copy, load, rmdir

required_conan_version = ">=2.0"


class SdfioConan(ConanFile):
    name = "sdfio"
    package_type = "library"
    license = "MIT"
    homepage = "https://github.com/aschet/libsdfio"
    url = homepage
    description = "Reader and writer for ISO 25178-71 SDF surface data files in C"
    topics = ("sdf", "iso-25178", "surface-metrology", "depth-image", "c")

    settings = "os", "compiler", "build_type", "arch"
    options = {
        "shared": [True, False],
        "fPIC": [True, False],
    }
    default_options = {
        "shared": False,
        "fPIC": True,
    }
    implements = ["auto_shared_fpic"]

    exports_sources = (
        "CMakeLists.txt",
        "LICENSE",
        "cmake/*",
        "include/*",
        "src/*",
        "tests/*",
        "examples/*",
    )

    def set_version(self):
        # The project declares the version once, in CMakeLists.txt.
        text = load(self, os.path.join(self.recipe_folder, "CMakeLists.txt"))
        match = re.search(r"project\(.*?VERSION\s+(\d+\.\d+\.\d+)", text, re.S)
        if match is None:
            raise ConanException("no project version found in CMakeLists.txt")
        self.version = match.group(1)

    def configure(self):
        # A C library: the C++ settings do not matter.
        self.settings.rm_safe("compiler.cppstd")
        self.settings.rm_safe("compiler.libcxx")

    def layout(self):
        cmake_layout(self)

    def generate(self):
        toolchain = CMakeToolchain(self)
        toolchain.cache_variables["BUILD_TESTING"] = False
        toolchain.cache_variables["SDFIO_BUILD_EXAMPLES"] = False
        toolchain.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()
        copy(
            self,
            "LICENSE",
            src=self.source_folder,
            dst=os.path.join(self.package_folder, "licenses"),
        )
        # Conan generates its own package files from the metadata below.
        for unused in ("lib/cmake", "share/licenses"):
            rmdir(self, os.path.join(self.package_folder, unused))

    def package_info(self):
        self.cpp_info.libs = ["sdfio"]
        self.cpp_info.set_property("cmake_file_name", "sdfio")
        self.cpp_info.set_property("cmake_target_name", "sdfio::sdfio")
        if not self.options.shared:
            self.cpp_info.defines.append("SDFIO_STATIC_DEFINE")
        if self.settings.os in ("Linux", "FreeBSD"):
            self.cpp_info.system_libs.append("m")
