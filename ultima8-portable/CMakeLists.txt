cmake_minimum_required(VERSION 3.20)
project(ultima8_portable VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(WIN32)
    # The game builds with clang; on Windows that is clang-cl (Visual Studio's "C++ Clang
    # tools for Windows").
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR "On Windows, build with clang-cl: -DCMAKE_C_COMPILER=clang-cl "
            "-DCMAKE_CXX_COMPILER=clang-cl")
    endif()
    # libmatoya's makefile builds with the static runtime (/MT); everything else matches it.
    set(CMAKE_MSVC_RUNTIME_LIBRARY MultiThreaded)
endif()

set(U8_GAME_DIR "${CMAKE_CURRENT_SOURCE_DIR}/game")
set(U8_PLATFORM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/platform")
set(U8_THIRD_PARTY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party")
set(U8_LINK_FILE "${CMAKE_CURRENT_SOURCE_DIR}/../lib/U8.LNK")

option(U8_BACKEND_MATOYA "Build the libmatoya backend" ON)
set(U8_MT32_ROM_DIR "" CACHE PATH
    "Folder with the MT-32 control and PCM ROMs; the U8_MT32_ROMS variable overrides it at run time")

file(GLOB_RECURSE U8_GAME_SOURCES CONFIGURE_DEPENDS "${U8_GAME_DIR}/*.cpp")

# Borland's startup code ran static constructors of equal priority from the end of the link
# order backwards. The linker runs them in object order, so the objects are listed in reverse
# U8.LNK order; the port's own files, which have no place in U8.LNK, go last.
set(U8_ORDERED_SOURCES "")
if(EXISTS "${U8_LINK_FILE}")
    file(STRINGS "${U8_LINK_FILE}" U8_LINK_LINES REGEX "^OBJ\\\\")
    foreach(line IN LISTS U8_LINK_LINES)
        string(REGEX REPLACE "^OBJ\\\\([A-Za-z0-9_]+)\\.OBJ.*$" "\\1" module "${line}")
        string(TOLOWER "${module}" module)
        foreach(source IN LISTS U8_GAME_SOURCES)
            get_filename_component(stem "${source}" NAME_WE)
            if(stem STREQUAL module)
                list(PREPEND U8_ORDERED_SOURCES "${source}")
                list(REMOVE_ITEM U8_GAME_SOURCES "${source}")
                break()
            endif()
        endforeach()
    endforeach()
endif()
list(APPEND U8_ORDERED_SOURCES ${U8_GAME_SOURCES})

add_library(u8_game OBJECT ${U8_ORDERED_SOURCES})

# The game's headers sit beside its sources in many folders, and the sources name them
# without paths, as the DOS build's include path let them.
file(GLOB_RECURSE U8_GAME_DIRS LIST_DIRECTORIES true "${U8_GAME_DIR}/*")
set(U8_INCLUDE_DIRS "${U8_GAME_DIR}")
foreach(dir IN LISTS U8_GAME_DIRS)
    if(IS_DIRECTORY "${dir}")
        list(APPEND U8_INCLUDE_DIRS "${dir}")
    endif()
endforeach()
target_include_directories(u8_game PUBLIC ${U8_INCLUDE_DIRS} "${U8_PLATFORM_DIR}")

# The code expects signed char, keeps null tests after a dereference and after new (Borland's
# new gave 0 when out of memory), reads buffers through pointers of other types, expects signed
# arithmetic to wrap, and was written for a compiler that accepted much that clang warns of.
set(U8_GAME_WARNINGS
    -Wno-parentheses -Wno-dangling-else -Wno-pragma-pack -Wno-writable-strings
    -Wno-address-of-temporary -Wno-switch -Wno-unused-value -Wno-unused-variable
    -Wno-unused-but-set-variable -Wno-logical-op-parentheses -Wno-bitwise-op-parentheses
    -Wno-shift-op-parentheses -Wno-tautological-constant-out-of-range-compare
    -Wno-c++11-narrowing -Wno-deprecated-declarations -Wno-invalid-offsetof
    -Wno-new-returns-null -Wno-format -Wno-char-subscripts -Wno-implicit-const-int-float-conversion
    -Wno-constant-conversion -Wno-delete-non-abstract-non-virtual-dtor -Wno-mismatched-new-delete
    -Werror=return-type)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    # clang-cl: char is already signed on Windows.
    target_compile_options(u8_game PRIVATE -fno-delete-null-pointer-checks /clang:-fcheck-new
        /clang:-fwrapv /clang:-fno-strict-aliasing ${U8_GAME_WARNINGS})
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # U8_INLINE (u8port.h) has each file emit the inline functions it defines, for files that
    # call them without a definition; clang would emit only the base-object variant of a
    # constructor or destructor, so it is told to emit the complete-object one too.
    target_compile_options(u8_game PRIVATE -fsigned-char -fno-delete-null-pointer-checks -fcheck-new
        -fwrapv -fno-strict-aliasing "SHELL:-Xclang -mno-constructor-aliases" ${U8_GAME_WARNINGS})
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(u8_game PRIVATE -fsigned-char -fno-delete-null-pointer-checks -fcheck-new
        -fwrapv -fno-strict-aliasing -fkeep-inline-functions -fpermissive -w)
endif()

if(U8_BACKEND_MATOYA)
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
    include(Libmatoya)
    u8_add_libmatoya()

    set(libmt32emu_SHARED OFF CACHE BOOL "Build static mt32emu" FORCE)
    set(libmt32emu_C_INTERFACE ON CACHE BOOL "Expose mt32emu C interface" FORCE)
    set(libmt32emu_CPP_INTERFACE ON CACHE BOOL "Required internally by static mt32emu" FORCE)
    set(libmt32emu_BUILD_TESTING OFF CACHE BOOL "No mt32emu tests" FORCE)
    set(libmt32emu_WITH_INTERNAL_RESAMPLER ON CACHE BOOL "Use bundled resampler" FORCE)
    add_subdirectory("${U8_THIRD_PARTY_DIR}/munt/mt32emu" mt32emu EXCLUDE_FROM_ALL)

    file(GLOB U8_MATOYA_SOURCES CONFIGURE_DEPENDS "${U8_PLATFORM_DIR}/matoya/*.c")
    add_executable(u8 ${U8_MATOYA_SOURCES} "${U8_THIRD_PARTY_DIR}/nuked-opl3/opl3.c")
    # Ultima8(.exe), so it can sit in the game folder beside the DOS U8.EXE without clashing.
    set_target_properties(u8 PROPERTIES OUTPUT_NAME Ultima8)
    target_include_directories(u8 PRIVATE "${U8_PLATFORM_DIR}" "${U8_THIRD_PARTY_DIR}/nuked-opl3")
    target_link_libraries(u8 PRIVATE u8_game Matoya::matoya mt32emu)
    if(WIN32)
        # A windowed program, so no console opens with the game; main() stays the entry point.
        # Started from a terminal, it attaches to that console for its messages (main.c).
        set_target_properties(u8 PROPERTIES WIN32_EXECUTABLE TRUE)
        target_link_options(u8 PRIVATE /ENTRY:mainCRTStartup)
    endif()
    if(U8_MT32_ROM_DIR)
        target_compile_definitions(u8 PRIVATE U8_DEFAULT_ROM_DIR="${U8_MT32_ROM_DIR}")
    endif()
endif()
