cmake_minimum_required(VERSION 3.20)

if(NOT CMAKE_TOOLCHAIN_FILE AND DEFINED ENV{VCPKG_ROOT})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Vcpkg toolchain file")
endif()

project(vcg-simplifier VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 2. 查找 Eigen3
message(STATUS "Looking for Eigen3...")
find_package(Eigen3 REQUIRED)
message(STATUS "Found Eigen3: ${EIGEN3_INCLUDE_DIR}")

# 3. 配置本地 VCGLib 路径
# 我们直接指向你刚才放好的 'vcglib' 文件夹
set(LOCAL_VCGLIB_PATH "${CMAKE_CURRENT_SOURCE_DIR}/vcglib")

if(EXISTS "${LOCAL_VCGLIB_PATH}/vcg/complex/complex.h")
    message(STATUS "Found local VCGLib at: ${LOCAL_VCGLIB_PATH}")
else()
    message(FATAL_ERROR "Could not find VCGLib in ${LOCAL_VCGLIB_PATH}. Please make sure you extracted the zip file correctly.")
endif()

# --------------------------------------------------------
# 4. 定义可执行文件
# --------------------------------------------------------
# [关键修复] 必须包含 wrap/ply/plylib.cpp，否则链接时会报错 "Undefined symbols ... vcg::ply::..."
add_executable(vcg-simplifier 
    src/main.cpp
    src/obj_loader.cpp
    src/glb_loader.cpp
    "${LOCAL_VCGLIB_PATH}/wrap/ply/plylib.cpp" 
)

# --------------------------------------------------------
# 5. 设置包含路径
# --------------------------------------------------------
# (A) Eigen3
target_link_libraries(vcg-simplifier PRIVATE Eigen3::Eigen)

# (B) 本地 VCGLib
target_include_directories(vcg-simplifier PRIVATE ${LOCAL_VCGLIB_PATH})

# (C) 本地源码
target_include_directories(vcg-simplifier PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# (D) 【新增】TinyGLTF
find_path(TINYGLTF_INCLUDE_DIRS "tiny_gltf.h")
target_include_directories(vcg-simplifier PRIVATE ${TINYGLTF_INCLUDE_DIRS})

# Disable secure warnings
target_compile_definitions(vcg-simplifier PRIVATE _CRT_SECURE_NO_WARNINGS)

message(STATUS "Ready to build")