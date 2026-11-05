cmake_minimum_required(VERSION 4.4)
project(Matrix_Wallpaper LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK4 REQUIRED IMPORTED_TARGET gtk4)

add_executable(Matrix_Wallpaper main.cpp
        MatrixStream.cpp
        MatrixStream.h
        MatrixRenderer.cpp
        MatrixRenderer.h
        CrtAtmosphere.cpp
        CrtAtmosphere.h
        PhosphorTrail.cpp
        PhosphorTrail.h
        PhosphorGlyph.cpp
        PhosphorGlyph.h)

target_link_libraries(Matrix_Wallpaper PRIVATE PkgConfig::GTK4)