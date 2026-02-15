cmake_minimum_required(VERSION 3.16)

project(LightroomImportClone LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui)
find_package(Threads REQUIRED)

# Try to find LibRaw
find_package(PkgConfig)
pkg_check_modules(LibRaw libraw)

if (NOT LibRaw_FOUND)
    message(STATUS "LibRaw not found via PkgConfig, trying to fetch it...")
    include(FetchContent)
    FetchContent_Declare(
        LibRaw
        GIT_REPOSITORY https://github.com/LibRaw/LibRaw.git
        GIT_TAG 0.21.1
    )
    # LibRaw build options to minimize deps if fetched
    set(LIBRAW_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(LibRaw)
    set(LibRaw_LIBRARIES libraw)
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
    target_include_directories(LightroomImportClone PRIVATE ${LibRaw_INCLUDE_DIRS})
    target_link_libraries(LightroomImportClone PRIVATE ${LibRaw_LIBRARIES})
else()
    # Fallback if fetched
    target_include_directories(LightroomImportClone PRIVATE ${libraw_SOURCE_DIR})
    target_link_libraries(LightroomImportClone PRIVATE libraw)
endif()

if(APPLE)
    set_target_properties(LightroomImportClone PROPERTIES MACOSX_BUNDLE ON)
endif()

target_include_directories(LightroomImportClone PRIVATE src)

install(TARGETS LightroomImportClone
    BUNDLE DESTINATION .
    RUNTIME DESTINATION bin
)
