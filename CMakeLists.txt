cmake_minimum_required(VERSION 3.15)
project(ChunkMapTool VERSION 1.0.0 LANGUAGES CXX)

# 禁用 vcpkg 的自动 DLL 复制（避免 PowerShell 脚本错误）
set(VCPKG_APPLOCAL_DEPS OFF)
set(X_VCPKG_APPLOCAL_DEPS_INSTALL OFF)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 查找依赖库
find_package(ZLIB REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(OpenGL REQUIRED)

# 包含目录
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${ZLIB_INCLUDE_DIRS}
    ${OPENGL_INCLUDE_DIR}
)

# 源文件
set(CORE_SOURCES
    src/Core/McaFileLoader.cpp
    src/Core/NbtParser.cpp
    src/Core/ChunkDecoder.cpp
    src/Utils/Compression.cpp
    src/Utils/TextureLoader.cpp
)

# GUI 源文件
set(GUI_SOURCES
    src/GUI/McaViewerGUI.cpp
    src/GUI/imgui.cpp
    src/GUI/imgui_draw.cpp
    src/GUI/imgui_tables.cpp
    src/GUI/imgui_widgets.cpp
    src/GUI/imgui_impl_glfw.cpp
    src/GUI/imgui_impl_opengl3.cpp
)

# 头文件
set(CORE_HEADERS
    include/Core/DataStructures.h
    include/Core/McaFileLoader.h
    include/Core/NbtParser.h
    include/Core/ChunkDecoder.h
    include/Utils/Compression.h
    include/Utils/Logger.h
    include/Utils/TextureLoader.h
)

# GUI 头文件
set(GUI_HEADERS
    include/GUI/McaViewerGUI.h
)

# 创建静态库
add_library(mcatool_core STATIC ${CORE_SOURCES} ${CORE_HEADERS})
target_link_libraries(mcatool_core PRIVATE ${ZLIB_LIBRARIES})

# 主程序（GUI 版本）
add_executable(mcatool src/main.cpp ${GUI_SOURCES})
target_link_libraries(mcatool PRIVATE 
    mcatool_core 
    ${ZLIB_LIBRARIES}
    glfw
    imgui::imgui
    ${OPENGL_LIBRARIES}
)

# 编译选项
if(MSVC)
    target_compile_options(mcatool_core PRIVATE /W4)
    target_compile_options(mcatool PRIVATE /W4)
else()
    target_compile_options(mcatool_core PRIVATE -Wall -Wextra -pedantic)
    target_compile_options(mcatool PRIVATE -Wall -Wextra -pedantic)
endif()

# 安装规则
install(TARGETS mcatool DESTINATION bin)
install(TARGETS mcatool_core DESTINATION lib)
install(DIRECTORY include/ DESTINATION include)
