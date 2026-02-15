cmake_minimum_required(VERSION 3.16)

project(LightroomImportClone LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui)
find_package(Threads REQUIRED)

# Try to find LibRaw: prefer a CMake config package (e.g. from vcpkg), then PkgConfig, then FetchContent.
find_package(LibRaw CONFIG QUIET)

if (NOT LibRaw_FOUND)
    find_package(PkgConfig QUIET)
    if (PkgConfig_FOUND)
        pkg_check_modules(LibRaw libraw)
    endif()
endif()

if (NOT LibRaw_FOUND)
    message(STATUS "LibRaw not found via CMake config or PkgConfig, trying to fetch it...")
    include(FetchContent)
    FetchContent_Declare(
        LibRaw
        GIT_REPOSITORY https://github.com/LibRaw/LibRaw.git
        GIT_TAG 0.21.1
    )
    # LibRaw build options to minimize deps if fetched
    set(LIBRAW_INSTALL OFF CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE) # Build static libraw
    FetchContent_MakeAvailable(LibRaw)
    
    # Check if libraw target exists (it might be named differently depending on how it's built)
    if(TARGET libraw)
        set(LibRaw_LIBRARIES libraw)
    elseif(TARGET raw)
        set(LibRaw_LIBRARIES raw)
    else()
        message(WARNING "LibRaw target not found after fetch!")
    endif()
    
    set(LibRaw_INCLUDE_DIRS ${libraw_SOURCE_DIR})
endif()

set(SOURCES
    src/main.cpp
    src/MainWindow.cpp
    src/ThumbnailWorker.cpp
    src/ScannerWorker.cpp
    src/ImportWorker.cpp
    src/CustomWidgets.cpp
    src/Utils.cpp
)

set(HEADERS
    src/MainWindow.h
    src/ThumbnailWorker.h
    src/ScannerWorker.h
    src/ImportWorker.h
    src/CustomWidgets.h
    src/Utils.h
)

qt_add_executable(LightroomImportClone
    MANUAL_FINALIZATION
    ${SOURCES}
    ${HEADERS}
)

target_link_libraries(LightroomImportClone PRIVATE
    Qt6::Widgets
    Qt6::Core
    Qt6::Gui
    Threads::Threads
)

if (LibRaw_FOUND)
    if (TARGET libraw::raw_r)
        target_link_libraries(LightroomImportClone PRIVATE libraw::raw_r)
    elseif (TARGET libraw::raw)
        target_link_libraries(LightroomImportClone PRIVATE libraw::raw)
    elseif (TARGET LibRaw::LibRaw)
        target_link_libraries(LightroomImportClone PRIVATE LibRaw::LibRaw)
    else()
        target_include_directories(LightroomImportClone PRIVATE ${LibRaw_INCLUDE_DIRS})
        target_link_directories(LightroomImportClone PRIVATE ${LibRaw_LIBRARY_DIRS})
        target_link_libraries(LightroomImportClone PRIVATE ${LibRaw_LIBRARIES})
    endif()
else()
    # Fallback if fetched
    target_include_directories(LightroomImportClone PRIVATE ${libraw_SOURCE_DIR})
    if(TARGET libraw)
        target_link_libraries(LightroomImportClone PRIVATE libraw)
    elseif(TARGET raw)
        target_link_libraries(LightroomImportClone PRIVATE raw)
    else()
        if (WIN32)
            message(FATAL_ERROR "LibRaw not found. On Windows, install it with vcpkg (vcpkg install libraw) and configure CMake with the vcpkg toolchain file.")
        else()
            message(FATAL_ERROR "LibRaw not found. Please install the LibRaw development package (e.g. libraw-dev on Linux, brew install libraw on macOS).")
        endif()
    endif()
endif()

if(APPLE)
    set_target_properties(LightroomImportClone PROPERTIES MACOSX_BUNDLE ON)
endif()

target_include_directories(LightroomImportClone PRIVATE src)

install(TARGETS LightroomImportClone
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)
