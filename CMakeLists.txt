cmake_minimum_required(VERSION 3.20)

project(dirt-display)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_STATIC_RUNTIME ON)
find_package(Boost 1.76 REQUIRED COMPONENTS program_options context coroutine filesystem date_time)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
add_compile_definitions(
	_WIN32_WINNT=0x0601
	WIN32_LEAN_AND_MEAN
	NOMINMAX
	UNICODE
	_UNICODE
	_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
)

if(ENABLE_MULTICAST)
	add_compile_definitions(ENABLE_MULTICAST)
endif()

file(GLOB DD_HEADERS
	src/*.hpp
)
file(GLOB IMGUI_HEADERS
	deps/imgui/*.h
	deps/imgui/backends/imgui_impl_win32.h
	deps/imgui/backends/imgui_impl_dx11.h
)
file(GLOB DD_SOURCES
	src/*.cpp
)
file(GLOB IMGUI_SOURCES
	deps/imgui/*.cpp
	deps/imgui/backends/imgui_impl_win32.cpp
	deps/imgui/backends/imgui_impl_dx11.cpp
)

source_group("Header Files" FILES ${DD_HEADERS})
source_group("Header Files/imgui" FILES ${IMGUI_HEADERS})
source_group("Source Files/imgui" FILES ${IMGUI_SOURCES})

add_executable(dirt-display 
	${DD_SOURCES} 
	${IMGUI_SOURCES}
	${DD_HEADERS}
	${IMGUI_HEADERS}
)
target_include_directories(dirt-display PUBLIC src ${Boost_INCLUDE_DIRS} deps/imgui)
target_link_libraries(dirt-display 
	Boost::program_options
	Boost::context
	Boost::coroutine
	Boost::filesystem
	Boost::date_time
)