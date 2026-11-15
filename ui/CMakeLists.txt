cmake_minimum_required(VERSION 3.20)
project(ZTX VERSION 0.1.0 LANGUAGES CXX RC)

if(NOT WIN32)
    message(FATAL_ERROR "ZTX uses the native Windows API. Build with MSYS2 UCRT64 on Windows.")
endif()

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build configuration" FORCE)
endif()

add_executable(ZTX WIN32
    src/main.cpp
    src/preferences.cpp
    resources/ztx.rc
)
target_compile_features(ZTX PRIVATE cxx_std_17)
target_compile_definitions(ZTX PRIVATE
    UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN
    WINVER=0x0A00 _WIN32_WINNT=0x0A00
)
target_include_directories(ZTX PRIVATE resources)
set_property(SOURCE resources/ztx.rc PROPERTY OBJECT_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/resource.h;${CMAKE_CURRENT_SOURCE_DIR}/resources/ztx.manifest;${CMAKE_CURRENT_SOURCE_DIR}/resources/ztx.ico;${CMAKE_CURRENT_SOURCE_DIR}/resources/logo.png"
)
target_link_libraries(ZTX PRIVATE
    gdiplus dwmapi comctl32 ole32 shell32 uuid gdi32 user32 shlwapi
)
set_target_properties(ZTX PROPERTIES CXX_EXTENSIONS OFF)

if(MINGW)
    target_compile_options(ZTX PRIVATE
        "$<$<COMPILE_LANGUAGE:CXX>:-Wall;-Wextra;-Wpedantic>"
        "$<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CONFIG:Release>>:-Os;-flto;-ffunction-sections;-fdata-sections>"
    )
    target_link_options(ZTX PRIVATE
        -municode -static -static-libgcc -static-libstdc++
        "$<$<CONFIG:Release>:-flto;-Wl,--gc-sections;-s>"
    )
elseif(MSVC)
    target_compile_options(ZTX PRIVATE /W4 /utf-8 "$<$<CONFIG:Release>:/O1;/GL>")
    target_link_options(ZTX PRIVATE "$<$<CONFIG:Release>:/LTCG;/OPT:REF;/OPT:ICF>")
    set_property(TARGET ZTX PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()
