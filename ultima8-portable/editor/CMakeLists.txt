# u8edit: an editor for Ultima VIII's world and game data.
#
# It reads and writes the game's files itself and shares no code with the game build, so it is a
# project of its own:
#
#   cmake -S editor -B build/editor -DCMAKE_BUILD_TYPE=Release
#   cmake --build build/editor
#
# It needs Qt 6 (Widgets), 6.2 or newer. -DU8EDIT_TESTS=ON also builds u8edit-tests (needs Qt
# Test), which edits a copy of the game data given in U8_DATA.
cmake_minimum_required(VERSION 3.19)
project(u8edit LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

option(U8EDIT_TESTS "Build u8edit-tests" OFF)

find_package(Qt6 6.2 REQUIRED COMPONENTS Widgets)

add_library(u8edit_core STATIC
	src/data/flex.cpp
	src/data/flex.h
	src/data/gametables.cpp
	src/data/gametables.h
	src/data/itemcache.cpp
	src/data/language.cpp
	src/data/language.h
	src/data/itemcache.h
	src/data/itemnames.cpp
	src/data/itemnames.h
	src/data/shapes.cpp
	src/data/shapes.h
	src/data/ucode.cpp
	src/data/ucode.h
	src/data/ucvm.cpp
	src/data/ucvm.h
	src/data/intrinsics.inc
	src/data/usecode.cpp
	src/data/usecode.h
	src/data/world.cpp
	src/data/world.h
	src/ui/edits.cpp
	src/ui/edits.h
	src/ui/flagsview.cpp
	src/ui/flagsview.h
	src/ui/gamedata.cpp
	src/ui/gamedata.h
	src/ui/graphview.cpp
	src/ui/graphview.h
	src/ui/itempanel.cpp
	src/ui/itempanel.h
	src/ui/mainwindow.cpp
	src/ui/mainwindow.h
	src/ui/maprender.cpp
	src/ui/maprender.h
	src/ui/mapview.cpp
	src/ui/mapview.h
	src/ui/minimap.cpp
	src/ui/names.cpp
	src/ui/names.h
	src/ui/searchpanel.cpp
	src/ui/searchpanel.h
	src/ui/minimap.h
	src/ui/shapebrowser.cpp
	src/ui/shapebrowser.h
	src/ui/translatedialog.cpp
	src/ui/translatedialog.h
)
target_include_directories(u8edit_core PUBLIC src)
target_link_libraries(u8edit_core PUBLIC Qt6::Widgets)

add_executable(u8edit WIN32 MACOSX_BUNDLE src/main.cpp)
target_link_libraries(u8edit PRIVATE u8edit_core)

# The translations the editor comes with (File > Apply a Translation): the tables and their
# font patches from tools/lang, copied to languages/ beside the program.
set(U8EDIT_LANGUAGES tools/lang/pt_BR.json tools/lang/u8fonts-pt.json)
foreach(f ${U8EDIT_LANGUAGES})
	get_filename_component(name ${f} NAME)
	add_custom_command(TARGET u8edit POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:u8edit>/languages
		COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/${f}
			$<TARGET_FILE_DIR:u8edit>/languages/${name})
endforeach()

foreach(target u8edit_core u8edit)
	if(MSVC)
		target_compile_options(${target} PRIVATE /W4 /permissive-)
	else()
		target_compile_options(${target} PRIVATE -Wall -Wextra)
	endif()
endforeach()

if(U8EDIT_TESTS)
	find_package(Qt6 REQUIRED COMPONENTS Test)
	add_executable(u8edit-tests tests/tests.cpp)
	target_link_libraries(u8edit-tests PRIVATE u8edit_core Qt6::Test)
	target_compile_definitions(u8edit-tests PRIVATE U8EDIT_LANG_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tools/lang")
	add_dependencies(u8edit-tests u8edit)
endif()
