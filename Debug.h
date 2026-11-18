/*
 * SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Tobias Richter
 * GitHub: https://github.com/DerTobiasRichter
 *
 * Created on: 2026-11-18
 * Last Modified: 2026-11-18
 *
 *
 */

//
// Created by tobi on 11/18/26.
//
#pragma once

#include <iostream>
#include <utility>

namespace Debug {
    inline bool enabled = false;

    template<typename... Args>
    void log(Args&&... args) {
        if (!enabled) {
            return;
        }

        (std::cout << ... << std::forward<Args>(args))
            << std::endl;
    }
}