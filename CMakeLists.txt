#
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tobias Richter
# GitHub: https://github.com/DerTobiasRichter
#
# Created on: 2026-11-05
# Last Modified: 2026-11-06

cmake_minimum_required(VERSION 4.4)
project(Matrix_Wallpaper LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
pkg_check_modules(GTK4 REQUIRED IMPORTED_TARGET gtk4)
pkg_check_modules(GTK4_LAYER_SHELL REQUIRED IMPORTED_TARGET gtk4-layer-shell-0)
pkg_check_modules(LIBSYSTEMD REQUIRED IMPORTED_TARGET libsystemd)

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
        PhosphorGlyph.h
        JournalSource.cpp
        JournalSource.h
        MonitorManager.cpp
        MonitorManager.h)

target_link_libraries(Matrix_Wallpaper PRIVATE
        PkgConfig::GTK4
        PkgConfig::LIBSYSTEMD
        PkgConfig::GTK4_LAYER_SHELL
        Threads::Threads
)

set_target_properties(Matrix_Wallpaper PROPERTIES OUTPUT_NAME "matrix-wallpaper")

include(GNUInstallDirs)

install(TARGETS Matrix_Wallpaper RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
