import os
from conans import ConanFile, tools, CMake


class DiligentCoreConan(ConanFile):
    name = "diligent-core"
    version = "2.5"
    url = "https://github.com/DiligentGraphics/DiligentCore/"
    homepage = "https://github.com/DiligentGraphics/DiligentCore/tree/v2.5"
    description = "Diligent Core is a modern cross-platfrom low-level graphics API."
    license = ("Apache 2.0")
    topics = ("graphics")
    settings = "os", "compiler", "build_type", "arch"
    options = {"shared": [True, False], 
    "fPIC":         [True, False],
    "with_glslang": [True, False],
    }
    default_options = {"shared": False, 
    "fPIC": True,
    "with_glslang" : True
    }
    generators = "cmake_find_package", "cmake"
    exports_sources = ["*", "!build/*", "!conanfile.py", "!doc/*", "!Tests/*", "!.github/*"]
    _cmake = None
    short_paths=True

    @property
    def _source_subfolder(self):
        return "source_subfolder"

    @property
    def _build_subfolder(self):
        return "build_subfolder"

    #def source(self):
    #    tools.get(**self.conan_data["sources"][self.version], strip_root=True, destination=self._source_subfolder)

    def config_options(self):
        if self.settings.os == "Windows":
            del self.options.fPIC

    def configure(self):
        if self.options.shared:
            del self.options.fPIC

    def _patch_sources(self):
        pass
        #for patch in self.conan_data["patches"][self.version]:
        #    tools.patch(**patch)

    def requirements(self):
        self.requires("libjpeg/9d")
        self.requires("libtiff/4.2.0")
        self.requires("zlib/1.2.11")
        self.requires("libpng/1.6.37")

        #self.requires("vulkan-memory-allocator/2.3.0")
        #self.requires("vulkan-loader/1.2.172")
        #self.requires("vulkan-headers/1.2.172")

        self.requires("glew/2.2.0")
        #self.requires("stb/20200203")
        
        if self.settings.os in ["Linux", "FreeBSD"]:
            self.requires("xorg/system")
            if not tools.cross_building(self, skip_x64_x86=True):
                self.requires("xkbcommon/1.3.0")        

    def _configure_cmake(self):
        if self._cmake:
            return self._cmake
        self._cmake = CMake(self)
        self._cmake.definitions["DILIGENT_BUILD_SAMPLES"] = False
        self._cmake.definitions["DILIGENT_NO_FORMAT_VALIDATION"] = True
        self._cmake.definitions["DILIGENT_BUILD_TESTS"] = False
        self._cmake.definitions["DILIGENT_NO_GLSLANG"] = not self.options.with_glslang
        self._cmake.definitions["DILIGENT_NO_DIRECT3D11"] = True
        self._cmake.definitions["DILIGENT_NO_DIRECT3D12"] = True
        self._cmake.definitions["DILIGENT_NO_DXC"] = True

        return self._cmake

    def build(self):
        #self._patch_sources()
        cmake = self._configure_cmake()
        cmake.configure(build_folder=self._build_subfolder)
        cmake.build()

    def package(self):
        cmake = self._configure_cmake()
        cmake.install()
        self.copy("License.txt", dst="licenses", src=self._source_subfolder)
        
        #self.copy("*.h", src="ThirdParty/")
        self.copy("*.hpp", src="ThirdParty/", dst="ThirdParty/")
        self.copy("*.h", src="ThirdParty/", dst="ThirdParty/")

    def package_info(self):
        if self.settings.build_type == "Debug":
            self.cpp_info.libdirs.append("lib/Debug")
        if self.settings.build_type == "Release":
            self.cpp_info.libdirs.append("lib/Release")

        self.cpp_info.includedirs.append('include')
        self.cpp_info.includedirs.append('ThirdParty')
        self.cpp_info.includedirs.append('ThirdParty/glslang')

        self.cpp_info.includedirs.append('ThirdParty/SPIRV-Headers/include/')
        self.cpp_info.includedirs.append('ThirdParty/SPIRV-Cross/')
        self.cpp_info.includedirs.append('ThirdParty/SPIRV-Cross/include')
        self.cpp_info.includedirs.append('ThirdParty/SPIRV-Tools/include')
        self.cpp_info.includedirs.append('ThirdParty/Vulkan-Headers/include')
        
        if self.settings.os == "Windows":
            if self.settings.build_type == "Debug":
                self.cpp_info.libs = ['GraphicsEngineVk_64d', 'GraphicsEngineOpenGL_64d', 'DiligentCore', 'MachineIndependentd', 'glslangd', 'HLSLd', 'OGLCompilerd', 'OSDependentd', 'spirv-cross-cored', 'SPIRVd', 'SPIRV-Tools-opt', 'SPIRV-Tools', 'glew-static', 'GenericCodeGend']
            if self.settings.build_type == "Release":
                self.cpp_info.libs = ['GraphicsEngineVk_64', 'GraphicsEngineOpenGL_64', 'DiligentCore', 'MachineIndependent', 'glslang', 'HLSL', 'OGLCompiler', 'OSDependent', 'spirv-cross-core', 'SPIRV', 'SPIRV-Tools-opt', 'SPIRV-Tools', 'glew-static', 'GenericCodeGen']
        else:
            self.cpp_info.libs = ['DiligentCore', 'MachineIndependent', 'glslang', 'HLSL', 'OGLCompiler', 'OSDependent', 'spirv-cross-core', 'SPIRV', 'SPIRV-Tools-opt', 'SPIRV-Tools', 'glew-static', 'GenericCodeGen']
        self.cpp_info.defines.append("SPIRV_CROSS_NAMESPACE_OVERRIDE=diligent_spirv_cross")
        if self.settings.os in ["Macos", "Linux"]:
            self.cpp_info.system_libs = ["dl", "pthread"]
        if self.settings.os == 'Macos':
            self.cpp_info.frameworks = ["CoreFoundation", 'Cocoa']
