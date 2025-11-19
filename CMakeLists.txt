cmake_minimum_required(VERSION 3.16)
project(Snake LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if (MSVC)
	add_compile_options(/W4 /permissive-)
else ()
	add_compile_options(-Wall -Wextra)
endif()

file(GLOB SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/*.c")

add_compile_definitions(STB_EASY_FONT_IMPLEMENTATION)

add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/Include/stb")

find_package(glfw3 3.4 REQUIRED)
find_package(OpenGL REQUIRED)

target_link_libraries(${PROJECT_NAME} PRIVATE 
	OpenGL::GL
	$<$<TARGET_EXISTS:glfw>:glfw>
	m
)

# set(CMAKE_RUNTIME_OUTPUT_DIRECTORY, "${CMAKE_SOURCE_DIR}/bin")

set_target_properties(${PROJECT_NAME} PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
	RUNTIME_OUTPUT_DIRECTORY_BUILD "${CMAKE_SOURCE_DIR}/bin"
	RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/bin"
)

set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)

file(MAKE_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")	
