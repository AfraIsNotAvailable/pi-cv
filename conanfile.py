from conan import ConanFile
from conan.tools.cmake import CMake, cmake_layout

class ImageProcessingTemplate(ConanFile):
    name = "image_processing_template"
    version = "1.0"
    package_type = "application"
    
    settings = "os", "compiler", "build_type", "arch"
    requires = (
        "spdlog/1.14.1",
        # "opencv/4.5.3", # insane build times, included separately via system package
    )
    generators = "CMakeToolchain", "CMakeDeps"

    def layout(self):
        cmake_layout(self)

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()
