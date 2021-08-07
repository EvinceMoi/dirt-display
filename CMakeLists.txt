cmake_minimum_required(VERSION 3.20)

project(dirt-display)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_STATIC_RUNTIME ON)
#set(Boost_NO_BOOST_CMAKE ON)
find_package(Boost 1.76 REQUIRED COMPONENTS program_options context coroutine filesystem date_time)

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
	add_definitions(
		-D_WIN32_WINNT=0x0601
		-DWIN32_LEAN_AND_MEAN
		-DNOMINMAX
		-DUNICODE
		-D_UNICODE
		-D_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
	)
endif()


add_executable(dirt-display src/main.cpp
	deps/imgui/imgui.cpp
	deps/imgui/imgui_widgets.cpp
	deps/imgui/imgui_tables.cpp
	deps/imgui/imgui_draw.cpp
	deps/imgui/imgui_demo.cpp
	deps/imgui/backends/imgui_impl_win32.cpp
	deps/imgui/backends/imgui_impl_dx11.cpp
)
target_include_directories(dirt-display PUBLIC src ${Boost_INCLUDE_DIRS} deps/imgui)
target_link_libraries(dirt-display 
	Boost::program_options
	Boost::context
	Boost::coroutine
	Boost::filesystem
	Boost::date_time
)