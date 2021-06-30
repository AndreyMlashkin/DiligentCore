cmake_minimum_required (VERSION 3.6)

# cmake_minimum_required(VERSION) resets all policies to their default
# values for the VERSION, so calling cmake_policy(SET CMP0063 NEW) has
# no effect on subdirectories. We have to set CMAKE_POLICY_DEFAULT_CMP0063
# instead:
set(CMAKE_POLICY_DEFAULT_CMP0063 NEW)

if(VULKAN_SUPPORTED OR METAL_SUPPORTED)
    # We only need to have SPIRV-Headers folder, but we
    # do not really need any target from the project.
    #add_subdirectory(SPIRV-Headers)
    #set_directory_root_folder("SPIRV-Headers" "ThirdParty/DiligentCore/SPIRV-Headers")
    install(FILES SPIRV-Headers/LICENSE DESTINATION "Licenses/ThirdParty/${DILIGENT_CORE_DIR}" RENAME SPIRV-Headers-License.txt)

    install(FILES Vulkan-Headers/LICENSE.txt DESTINATION "Licenses/ThirdParty/${DILIGENT_CORE_DIR}" RENAME Vulkan-Headers-License.txt)

    option(DILIGENT_NO_GLSLANG "Do NOT build glslang compiler" OFF)
    option(DILIGENT_IMPROVE_SPIRV_TOOLS_DEBUG_PERF "Enable some optimizations for SPIRV-Tools, glslang, SPIRV-Cross and related projects in debug build to improve performance" ON)

    if(NOT (${DILIGENT_NO_GLSLANG} AND ${DILIGENT_NO_HLSL}))
        set(SPIRV-Headers_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/SPIRV-Headers")
        set(SPIRV_SKIP_TESTS ON CACHE BOOL "Skip SPIRV tests")
        set(SPIRV_BUILD_COMPRESSION OFF CACHE BOOL "Do not build SPIRV compressing codec")
        set(SPIRV_SKIP_EXECUTABLES ON CACHE BOOL "Skip building SPIRV executable and tests along with the library")
        set(SKIP_SPIRV_TOOLS_INSTALL ON CACHE BOOL "Skip SPIRV tools installation")
        set(SPIRV_WERROR OFF CACHE BOOL "Do not enable error on warning")
        if(${DILIGENT_IMPROVE_SPIRV_TOOLS_DEBUG_PERF})
            set(SPIRV_CHECK_CONTEXT OFF CACHE BOOL "Do not check if the IR context is in a valid state")
        endif()
        set(THIRD_PARTY_LIBS "CONAN_PKG::spirv-tools")
    endif()

    if(NOT ${DILIGENT_NO_GLSLANG})
        set(ENABLE_GLSLANG_BINARIES OFF CACHE BOOL "Do not build glslang binaries")
        set(ENABLE_SPVREMAPPER OFF CACHE BOOL "Do not build SPIRV remapper")
        set(SKIP_GLSLANG_INSTALL ON CACHE BOOL "Skip glslang installation")
        set(ENABLE_CTEST OFF CACHE BOOL "Disable testing")
        add_subdirectory(glslang)
        set_directory_root_folder("glslang" "DiligentCore/ThirdParty/glslang")

        # glslang "kindly" sets global CMAKE_DEBUG_POSTFIX to "d", which we have to unset now
        unset(CMAKE_DEBUG_POSTFIX CACHE)

        list(APPEND THIRD_PARTY_LIBS "CONAN_PKG::glslang" "CONAN_PKG::spirv-tools")
    endif()

    set(SPIRV_CROSS_CLI OFF CACHE BOOL "Build the CLI binary. Requires SPIRV_CROSS_STATIC.")
    set(SPIRV_CROSS_ENABLE_TESTS OFF CACHE BOOL "Enable SPIRV-Cross tests.")
    set(SPIRV_CROSS_ENABLE_MSL ${METAL_SUPPORTED} CACHE BOOL "Enable MSL target support.")
    # MSL support requires GLSL
    set(SPIRV_CROSS_ENABLE_GLSL ${SPIRV_CROSS_ENABLE_MSL} CACHE BOOL "Enable GLSL support.")
    set(SPIRV_CROSS_ENABLE_HLSL OFF CACHE BOOL "Enable HLSL target support.")
    set(SPIRV_CROSS_ENABLE_CPP OFF CACHE BOOL "Enable C++ target support.")
    set(SPIRV_CROSS_ENABLE_REFLECT OFF CACHE BOOL "Enable JSON reflection target support.")
    set(SPIRV_CROSS_ENABLE_C_API OFF CACHE BOOL "Enable C API wrapper support in static library.")
    set(SPIRV_CROSS_ENABLE_UTIL OFF CACHE BOOL "Enable util module support.")
    set(SPIRV_CROSS_FORCE_PIC ON CACHE BOOL "Force position-independent code.")
    set(SPIRV_CROSS_SKIP_INSTALL ON CACHE BOOL "Skip SPIRV-Cross installation.")
    set(SPIRV_CROSS_NAMESPACE_OVERRIDE diligent_spirv_cross CACHE STRING "Override the namespace used in the C++ API.")
    add_subdirectory(SPIRV-Cross)
    set_directory_root_folder("SPIRV-Cross" "DiligentCore/ThirdParty/SPIRV-Cross")

    list(APPEND THIRD_PARTY_LIBS "CONAN_PKG::spirv-cross")
endif()

if(${DILIGENT_BUILD_TESTS})
    set(INSTALL_GTEST OFF CACHE BOOL "Do not install googletest")
    set(BUILD_GMOCK OFF CACHE BOOL "Do not build gmock")
    set(gtest_force_shared_crt ON CACHE BOOL "Use shared (DLL) run-time lib even when Google Test is built as static lib.")
    add_subdirectory(googletest)
    set_directory_root_folder("googletest" "DiligentCore/ThirdParty/googletest")
    set_target_properties(gtest gtest_main PROPERTIES
        CXX_VISIBILITY_PRESET hidden # -fvisibility=hidden
        C_VISIBILITY_PRESET hidden # -fvisibility=hidden
        VISIBILITY_INLINES_HIDDEN TRUE
     )
endif()

