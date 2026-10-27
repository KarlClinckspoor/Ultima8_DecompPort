include_guard(GLOBAL)

# Builds the vendored libmatoya with its own makefiles (it has no CMake project) and exposes the
# archive as Matoya::matoya.
function(u8_add_libmatoya)
    set(_mty_root "${U8_THIRD_PARTY_DIR}/libmatoya")

    if(MINGW)
        # Its makefile wants nmake and Microsoft's shader compiler, so with mingw (a build for
        # Windows from Linux, say) the sources are compiled here. The Direct3D back ends, whose
        # shaders need fxc, are left out (cmake/mingw/d3d-stub.c); such a build draws with
        # Vulkan. cmake/mingw also fills in what mingw-w64's headers lack.
        set(_mty_sources app async crypto dtls file hash http image json list log memory queue
            resample system thread tlocal version gfx/vk/vk gfx/vk/vk-ctx gfx/vk/vk-ui hid/hid
            hid/utils windows/aes-gcm windows/appw windows/compress windows/cryptow
            windows/dialog windows/dtlsw windows/filew windows/hidw windows/imagew
            windows/memoryw windows/request windows/systemw windows/threadw windows/time
            windows/ws windows/webview)
        list(TRANSFORM _mty_sources PREPEND "${_mty_root}/src/")
        list(TRANSFORM _mty_sources APPEND ".c")
        set(_mty_shims "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/mingw")
        add_library(u8_libmatoya STATIC ${_mty_sources} "${_mty_shims}/audio.c" "${_mty_shims}/d3d-stub.c")
        target_include_directories(u8_libmatoya PRIVATE "${_mty_shims}" "${_mty_root}/deps"
            "${_mty_root}/src" "${_mty_root}/src/windows")
        target_include_directories(u8_libmatoya PUBLIC "${_mty_root}/src")
        target_compile_definitions(u8_libmatoya PRIVATE UNICODE WIN32_LEAN_AND_MEAN MTY_VK_WIN32)
        target_compile_options(u8_libmatoya PRIVATE -w)
        target_link_libraries(u8_libmatoya PUBLIC
            advapi32 bcrypt crypt32 dxgi dxguid gdi32 hid imm32 ole32 shcore shell32 shlwapi
            user32 userenv windowscodecs winhttp winmm ws2_32 xinput1_4 uuid)
        add_library(Matoya::matoya ALIAS u8_libmatoya)
        add_custom_target(u8_libmatoya_build)
        return()
    endif()

    if(WIN32)
        find_program(_mty_make NAMES nmake REQUIRED)
        if(CMAKE_SIZEOF_VOID_P EQUAL 8)
            set(_mty_arch x64)
        else()
            set(_mty_arch Win32)
        endif()
        set(_mty_library "${_mty_root}/bin/windows/${_mty_arch}/matoya.lib")
        set(_mty_build_command "${_mty_make}" /F makefile "ARCH=${_mty_arch}" all)
    else()
        find_program(_mty_make NAMES gmake make REQUIRED)
        if(CMAKE_SYSTEM_PROCESSOR)
            set(_mty_arch "${CMAKE_SYSTEM_PROCESSOR}")
        else()
            execute_process(COMMAND uname -m OUTPUT_VARIABLE _mty_arch OUTPUT_STRIP_TRAILING_WHITESPACE)
        endif()
        if(APPLE)
            set(_mty_target macosx)
        else()
            set(_mty_target linux)
        endif()
        set(_mty_library "${_mty_root}/bin/${_mty_target}/${_mty_arch}/libmatoya.a")
        set(_mty_build_command "${CMAKE_COMMAND}" -E env TERM=xterm
            "${_mty_make}" -f GNUmakefile "TARGET=${_mty_target}" "ARCH=${_mty_arch}" all)
    endif()

    add_custom_command(
        OUTPUT "${_mty_library}"
        COMMAND ${_mty_build_command}
        WORKING_DIRECTORY "${_mty_root}"
        COMMENT "Building libmatoya with its own makefile"
        VERBATIM
    )
    add_custom_target(u8_libmatoya_build DEPENDS "${_mty_library}")

    add_library(u8_libmatoya STATIC IMPORTED GLOBAL)
    add_library(Matoya::matoya ALIAS u8_libmatoya)
    set_target_properties(u8_libmatoya PROPERTIES
        IMPORTED_LOCATION "${_mty_library}"
        INTERFACE_INCLUDE_DIRECTORIES "${_mty_root}/src"
    )
    add_dependencies(u8_libmatoya u8_libmatoya_build)

    if(APPLE)
        target_link_libraries(u8_libmatoya INTERFACE
            "-framework AppKit" "-framework AudioToolbox" "-framework Carbon" "-framework IOKit"
            "-framework Metal" "-framework QuartzCore" "-framework WebKit")
    elseif(WIN32)
        target_link_libraries(u8_libmatoya INTERFACE
            advapi32 bcrypt crypt32 d3d11 d3d12 dxgi dxguid gdi32 hid imm32 ole32 shcore shell32
            shlwapi user32 userenv windowscodecs winhttp winmm ws2_32 xinput)
    else()
        target_link_libraries(u8_libmatoya INTERFACE m dl pthread)
    endif()
endfunction()
