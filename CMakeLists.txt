cmake_minimum_required(VERSION 3.16)
project(Snake LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(DEBUG "DEBUG " ON)

if (MSVC)
	add_compile_options(/W4 /permissive-)
else ()
	add_compile_options(-Wall -Wextra)
endif()

file(GLOB SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/*.c" "${CMAKE_SOURCE_DIR}/src/*/*.c")

add_compile_definitions(STB_EASY_FONT_IMPLEMENTATION)

add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/Include/stb")

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

add_subdirectory(libs/glfw)

target_link_libraries(${PROJECT_NAME} PRIVATE glfw)

find_package(OpenGL REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE 
	OpenGL::GL
	m
)

# set(CMAKE_RUNTIME_OUTPUT_DIRECTORY, "${CMAKE_SOURCE_DIR}/bin")

set_target_properties(${PROJECT_NAME} PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
	RUNTIME_OUTPUT_DIRECTORY_BUILD "${CMAKE_SOURCE_DIR}/bin"
	RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/bin"
)

if (DEBUG)
	if (WIN32)
		target_link_libraries(${PROJECT_NAME} dbghelp)
	else()
		target_compile_definitions(${PROJECT_NAME} PRIVATE _GNU_SOURCE)
		target_link_options(${PROJECT_NAME} PRIVATE -rdynamic)
		# target_link_libraries(${PROJECT_NAME} dl)
	endif()
endif()

set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)

file(MAKE_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")	
