cmake_minimum_required(VERSION 3.21)  # HIP 6.x требует минимум 3.21
project(Snake LANGUAGES C CXX)

# Поиск HIP
if(NOT DEFINED HIP_PATH)
    if(NOT DEFINED ENV{HIP_PATH})
        set(HIP_PATH "C:/Program Files/AMD/ROCm/6.4" CACHE PATH "Path to HIP installation")
    else()
        set(HIP_PATH $ENV{HIP_PATH} CACHE PATH "Path to HIP installation")
    endif()
endif()

list(APPEND CMAKE_PREFIX_PATH ${HIP_PATH})

# Попытка найти HIP
find_package(hip QUIET)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_CXX_EXTENSIONS OFF)

set(debug_name "Snake_debug")
set(bench "Snake_bench")

option(DEBUG "DEBUG" ON)

if (MSVC)
	add_compile_options(/W4 /permissive-)
else ()
	add_compile_options(-Wall -Wextra)
endif()

file(GLOB SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/*.c" "${CMAKE_SOURCE_DIR}/src/errors/*.c" "${CMAKE_SOURCE_DIR}/src/snake/*.c")
file(GLOB SOURCES_BENCH CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/bench/*.c" "${CMAKE_SOURCE_DIR}/src/errors/*.c" "${CMAKE_SOURCE_DIR}/src/snake/*.c")

add_compile_definitions(STB_EASY_FONT_IMPLEMENTATION)

# Минимальный HIP-тест (отдельный exe), чтобы проверить, может ли toolchain собрать простой HIP-раннер
if(hip_FOUND)
    enable_language(HIP)
    add_executable(hip_minimal "tests/hip_minimal/hip_minimal.hip")
    set_source_files_properties("tests/hip_minimal/hip_minimal.hip" PROPERTIES LANGUAGE HIP)
    target_link_libraries(hip_minimal PRIVATE hip::host hip::device)
    set_target_properties(hip_minimal PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/bin"
    )
endif()

# Проверяем наличие GPU директории и HIP
set(GPU_ENABLED FALSE)
# GPU временно отключено из-за проблем с HIP 6.x на Windows
# Для включения: замените FALSE на EXISTS "${CMAKE_SOURCE_DIR}/src/gpu" AND hip_FOUND
if(FALSE AND EXISTS "${CMAKE_SOURCE_DIR}/src/gpu" AND hip_FOUND)
    file(GLOB HIP_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/gpu/*.hip")
    file(GLOB GPU_CPP_SOURCES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/src/gpu/*.cpp")
    
    if(HIP_SOURCES)
        message(STATUS "Building with GPU support (HIP ${hip_VERSION})")
        
        # Включаем HIP язык (работает с Ninja, не работает с VS генератором)
        enable_language(HIP)
        
        # Архитектура для Vega 7 (Ryzen 5 5500U) - используем gfx906 по умолчанию
        set(CMAKE_HIP_ARCHITECTURES "gfx906" CACHE STRING "HIP architectures")
        
        # Создаём GPU библиотеку как SHARED (DLL) для Windows
        add_library(snake_gpu_kernels SHARED ${HIP_SOURCES} ${GPU_CPP_SOURCES})
        set_source_files_properties(${HIP_SOURCES} PROPERTIES LANGUAGE HIP)
        target_include_directories(snake_gpu_kernels PRIVATE "${CMAKE_SOURCE_DIR}/src")
        target_link_libraries(snake_gpu_kernels PUBLIC hip::host hip::device)
        
        # Добавляем флаги для relocatable device code
        target_compile_options(snake_gpu_kernels PRIVATE 
            $<$<COMPILE_LANGUAGE:HIP>:--offload-arch=gfx906>
            $<$<COMPILE_LANGUAGE:HIP>:-fgpu-rdc>
        )
        target_link_options(snake_gpu_kernels PRIVATE
            $<$<LINK_LANGUAGE:HIP>:-fgpu-rdc>
        )
        
        set(GPU_ENABLED TRUE)
    endif()
else()
    if(NOT hip_FOUND)
        message(WARNING "HIP not found - building without GPU support")
    endif()
    if(NOT EXISTS "${CMAKE_SOURCE_DIR}/src/gpu")
        message(STATUS "GPU directory not found - building without GPU support")
    endif()
endif()

add_executable(${PROJECT_NAME} ${SOURCES})
add_executable(${debug_name} ${SOURCES})
add_executable(${bench} ${SOURCES_BENCH})

target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/Include/stb")
target_include_directories(${debug_name} PRIVATE "${CMAKE_SOURCE_DIR}/Include/stb")
target_include_directories(${bench} PRIVATE "${CMAKE_SOURCE_DIR}/Include/stb")

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

add_subdirectory(libs/glfw)

target_link_libraries(${PROJECT_NAME} PRIVATE glfw)
target_link_libraries(${debug_name} PRIVATE glfw)
target_link_libraries(${bench} PRIVATE glfw)

find_package(OpenGL REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE OpenGL::GL)
target_link_libraries(${debug_name} PRIVATE OpenGL::GL)
target_link_libraries(${bench} PRIVATE OpenGL::GL)

# Windows-specific libraries
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE dbghelp)
    target_link_libraries(${debug_name} PRIVATE dbghelp)
    target_link_libraries(${bench} PRIVATE dbghelp)
endif()

# Линковка GPU библиотек если доступны
if(GPU_ENABLED)
	target_link_libraries(${PROJECT_NAME} PUBLIC snake_gpu_kernels)
	target_link_libraries(${debug_name} PUBLIC snake_gpu_kernels)
	target_link_libraries(${bench} PUBLIC snake_gpu_kernels)
	
	target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/src/gpu")
	target_include_directories(${debug_name} PRIVATE "${CMAKE_SOURCE_DIR}/src/gpu")
	target_include_directories(${bench} PRIVATE "${CMAKE_SOURCE_DIR}/src/gpu")
	
	target_compile_definitions(${PROJECT_NAME} PRIVATE USE_GPU=1)
	target_compile_definitions(${debug_name} PRIVATE USE_GPU=1)
	target_compile_definitions(${bench} PRIVATE USE_GPU=1)
	
	# GPU Test executable
	add_executable(Snake_gpu_test "${CMAKE_SOURCE_DIR}/src/gpu/hip_test.cpp")
	target_link_libraries(Snake_gpu_test PRIVATE snake_gpu_kernels)
	target_include_directories(Snake_gpu_test PRIVATE "${CMAKE_SOURCE_DIR}/src/gpu")
	
	message(STATUS "GPU support ENABLED - Bot will use GPU acceleration")
else()
	message(STATUS "GPU support DISABLED - Bot will use CPU only")
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
	RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/bin"
)

set_target_properties(${debug_name} PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
	RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/bin"
)

set_target_properties(${bench} PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
	RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/bin"
)

if(GPU_ENABLED)
	set_target_properties(Snake_gpu_test PROPERTIES
		RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
		RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/bin"
	)
endif()

if (DEBUG)
	if (NOT WIN32)
		target_compile_definitions(${debug_name} PRIVATE _GNU_SOURCE)
		target_link_options(${debug_name} PRIVATE -rdynamic)
	endif()
endif()

file(MAKE_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")	