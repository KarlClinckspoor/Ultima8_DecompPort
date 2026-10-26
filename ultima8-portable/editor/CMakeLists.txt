# u8edit: a world viewer for Ultima VIII's data, the start of an editor.
#
# It reads the game's files itself and shares no code with the game build, so it is a project
# of its own:
#
#   cmake -S editor -B build/editor -DCMAKE_BUILD_TYPE=Release
#   cmake --build build/editor
#
# It needs Qt 6 (Widgets), 6.2 or newer.
cmake_minimum_required(VERSION 3.19)
project(u8edit LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.2 REQUIRED COMPONENTS Widgets)

add_executable(u8edit WIN32 MACOSX_BUNDLE
	src/main.cpp
	src/data/flex.cpp
	src/data/flex.h
	src/data/shapes.cpp
	src/data/shapes.h
	src/data/usecode.cpp
	src/data/usecode.h
	src/data/world.cpp
	src/data/world.h
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
	src/ui/minimap.h
)

target_include_directories(u8edit PRIVATE src)
target_link_libraries(u8edit PRIVATE Qt6::Widgets)

if(MSVC)
	target_compile_options(u8edit PRIVATE /W4 /permissive-)
else()
	target_compile_options(u8edit PRIVATE -Wall -Wextra)
endif()
