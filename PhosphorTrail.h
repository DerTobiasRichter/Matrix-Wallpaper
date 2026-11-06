/*
 * SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Tobias Richter
 * GitHub: https://github.com/DerTobiasRichter
 *
 * Created on: 2026-11-05
 * Last Modified: 2026-11-06
 *
 *
 */
#pragma once

#include <cairo.h>
#include <random>

class PhosphorTrail {
public:
    PhosphorTrail();
    ~PhosphorTrail();

    PhosphorTrail(const PhosphorTrail&) = delete;
    PhosphorTrail& operator=(const PhosphorTrail&) = delete;

    static constexpr int minTrailLength = 39;
    static constexpr int maxTrailLength = 69;

    [[nodiscard]] static int randomTrailLength(std::mt19937& rng) {
        return std::uniform_int_distribution<int>(
            minTrailLength,
            maxTrailLength
        )(rng);
    }
    void draw(cairo_t* cr, double x, double headY, double glyphWidth, double characterHeight,  int trailLength, double opacity) const;

private:
    cairo_surface_t* surface_ = nullptr;
};