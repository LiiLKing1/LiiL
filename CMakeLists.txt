cmake_minimum_required(VERSION 3.15)
project(LiiL VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Warning levels
if(MSVC)
    add_compile_options(/W4 /WX-)
    add_compile_definitions(UNICODE _UNICODE)
else()
    add_compile_options(-Wall -Wextra)
    add_compile_definitions(UNICODE _UNICODE)
endif()

# Source files
set(SOURCES
    src/main.cpp
    src/ui/MainWindow.cpp
    src/core/Logger.cpp
    src/config/ConfigManager.cpp
    src/command/SystemCommands.cpp
    src/command/CommandEngine.cpp
    src/command/SystemActions.cpp
    src/command/AutomationEngine.cpp
)

# Header files (for IDE grouping)
set(HEADERS
    src/ui/MainWindow.h
    src/core/Logger.h
    src/config/ConfigManager.h
    src/command/SystemCommands.h
    src/command/CommandEngine.h
    src/command/SystemActions.h
    src/command/AutomationEngine.h
)

# Add executable
# WIN32 parameter ensures it's a Windows GUI application (no separate console window by default)
add_executable(${PROJECT_NAME} WIN32 ${SOURCES} ${HEADERS})

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Link Windows libraries
target_link_libraries(${PROJECT_NAME} PRIVATE User32 Gdi32 Comctl32 Dxva2)
