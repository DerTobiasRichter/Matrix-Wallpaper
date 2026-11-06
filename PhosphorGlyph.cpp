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
#include "PhosphorGlyph.h"

#include <algorithm>
#include <cmath>
#include <vector>

PhosphorGlyph::~PhosphorGlyph() {
    for (auto& [key, mask] : cache_) {
        if (mask.core) cairo_surface_destroy(mask.core);
        if (mask.glow) cairo_surface_destroy(mask.glow);
    }
}

void PhosphorGlyph::blur(
    cairo_surface_t* source,
    cairo_surface_t* destination
) {
    constexpr int weights[5] = {1, 4, 6, 4, 1};

    const int width = cairo_image_surface_get_width(source);
    const int height = cairo_image_surface_get_height(source);

    cairo_surface_flush(source);
    cairo_surface_flush(destination);

    const auto* src = cairo_image_surface_get_data(source);
    auto* dst = cairo_image_surface_get_data(destination);

    const int srcStride = cairo_image_surface_get_stride(source);
    const int dstStride = cairo_image_surface_get_stride(destination);

    std::vector<unsigned char> temporary(width * height);

    // Horizontal Gaussian approximation
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int sum = 0;

            for (int k = -2; k <= 2; ++k) {
                const int sx = std::clamp(x + k, 0, width - 1);
                sum += src[y * srcStride + sx] * weights[k + 2];
            }

            temporary[y * width + x] = (sum + 8) / 16;
        }
    }

    // Vertical Gaussian approximation
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int sum = 0;

            for (int k = -2; k <= 2; ++k) {
                const int sy = std::clamp(y + k, 0, height - 1);
                sum += temporary[sy * width + x] * weights[k + 2];
            }

            dst[y * dstStride + x] = (sum + 8) / 16;
        }
    }

    cairo_surface_mark_dirty(destination);
}

PhosphorGlyph::GlyphMask& PhosphorGlyph::getMask(
    cairo_t* cr,
    unsigned char glyph,
    int fontSize
) {
    const std::uint32_t key =
        (static_cast<std::uint32_t>(fontSize) << 8) | glyph;

    auto [it, inserted] = cache_.try_emplace(key);
    GlyphMask& mask = it->second;

    if (!inserted) {
        return mask;
    }

    cairo_save(cr);
    cairo_select_font_face(
        cr, "monospace",
        CAIRO_FONT_SLANT_NORMAL,
        CAIRO_FONT_WEIGHT_NORMAL
    );
    cairo_set_font_size(cr, fontSize);

    cairo_font_extents_t extents{};
    cairo_font_extents(cr, &extents);
    cairo_restore(cr);

    mask.ascent = extents.ascent;

    const int width = std::max(
        1,
        static_cast<int>(std::ceil(extents.max_x_advance)) + 2 * padding
    );
    const int height = std::max(
        1,
        static_cast<int>(std::ceil(extents.ascent + extents.descent)) + 2 * padding
    );

    mask.core = cairo_image_surface_create(CAIRO_FORMAT_A8, width, height);
    mask.glow = cairo_image_surface_create(CAIRO_FORMAT_A8, width, height);

    if (cairo_surface_status(mask.core) != CAIRO_STATUS_SUCCESS ||
        cairo_surface_status(mask.glow) != CAIRO_STATUS_SUCCESS) {
        return mask;
    }

    cairo_t* glyphCr = cairo_create(mask.core);

    cairo_select_font_face(
        glyphCr, "monospace",
        CAIRO_FONT_SLANT_NORMAL,
        CAIRO_FONT_WEIGHT_NORMAL
    );
    cairo_set_font_size(glyphCr, fontSize);

    const char text[2] = {static_cast<char>(glyph), '\0'};

    cairo_set_source_rgb(glyphCr, 1.0, 1.0, 1.0);
    cairo_move_to(glyphCr, padding, padding + mask.ascent);
    cairo_show_text(glyphCr, text);

    cairo_destroy(glyphCr);

    // Blur the mask four times, only when first encountered.
    cairo_surface_t* scratch =
        cairo_image_surface_create(CAIRO_FORMAT_A8, width, height);

    if (cairo_surface_status(scratch) == CAIRO_STATUS_SUCCESS) {
        blur(mask.core, scratch);
        blur(scratch, mask.glow);
        blur(mask.glow, scratch);
        blur(scratch, mask.glow);
    }

    cairo_surface_destroy(scratch);

    return mask;
}

void PhosphorGlyph::draw(
    cairo_t* cr,
    char glyph,
    double x,
    double baselineY,
    double fontSize,
    const MatrixColor& color,
    double opacity
) {
    if (glyph == ' ') {
        return;
    }

    const int roundedSize = std::max(
        1, static_cast<int>(std::lround(fontSize))
    );

    GlyphMask& mask = getMask(
        cr, static_cast<unsigned char>(glyph), roundedSize
    );

    if (!mask.core || !mask.glow ||
        cairo_surface_status(mask.core) != CAIRO_STATUS_SUCCESS ||
        cairo_surface_status(mask.glow) != CAIRO_STATUS_SUCCESS) {
        return;
    }

    const double left = x - padding;
    const double top = baselineY - padding - mask.ascent;
    const double alpha = std::clamp(opacity, 0.0, 1.0);

    // Soft, blurred phosphor impression
    cairo_set_source_rgba(
        cr, color.red, color.green, color.blue, alpha * 0.75
    );
    cairo_mask_surface(cr, mask.glow, left, top);

    // Weak core preserves a little glyph structure
    cairo_set_source_rgba(
        cr, color.red, color.green, color.blue, alpha * 0.23
    );
    cairo_mask_surface(cr, mask.core, left, top);
}