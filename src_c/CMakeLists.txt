cmake_minimum_required(VERSION 3.16)

project(LightroomImportClone_C LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Sources
set(SOURCES
    main.c
    scanner.c
    import.c
    hash.c
)

set(HEADERS
    common.h
    scanner.h
    import.h
    hash.h
)

if(WIN32)
    # Windows-specific build
    add_executable(LightroomImportClone_C WIN32 ${SOURCES} ${HEADERS})
    
    # Link Windows libraries
    target_link_libraries(LightroomImportClone_C 
        comctl32
        shlwapi
        shell32
        ole32
    )
    
    # Set subsystem to Windows
    if(MSVC)
        set_target_properties(LightroomImportClone_C PROPERTIES
            LINK_FLAGS "/SUBSYSTEM:WINDOWS"
        )
    endif()
else()
    message(FATAL_ERROR "This C implementation is Windows-only. Use the Qt C++ version for cross-platform support.")
endif()

# Install target
install(TARGETS LightroomImportClone_C
    RUNTIME DESTINATION bin
)
