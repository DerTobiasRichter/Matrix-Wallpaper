/*
 * SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Tobias Richter
 * GitHub: https://github.com/DerTobiasRichter
 *
 * Created on: 2026-11-03
 * Last Modified: 2026-11-06
 *
 *
 */
#pragma once

#include "MatrixStream.h"

#include <cairo.h>
#include <cstdint>
#include <unordered_map>

class PhosphorGlyph {
public:
    PhosphorGlyph() = default;
    ~PhosphorGlyph();

    PhosphorGlyph(const PhosphorGlyph&) = delete;
    PhosphorGlyph& operator=(const PhosphorGlyph&) = delete;

    void draw(
        cairo_t* cr,
        char glyph,
        double x,
        double baselineY,
        double fontSize,
        const MatrixColor& color,
        double opacity
    );

private:
    struct GlyphMask {
        cairo_surface_t* core = nullptr;
        cairo_surface_t* glow = nullptr;
        double ascent = 0.0;
    };

    static constexpr int padding = 12;

    std::unordered_map<std::uint32_t, GlyphMask> cache_;

    GlyphMask& getMask(
        cairo_t* cr,
        unsigned char glyph,
        int fontSize
    );

    static void blur(
        cairo_surface_t* source,
        cairo_surface_t* destination
    );
};