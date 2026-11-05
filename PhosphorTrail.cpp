//
// Created by tobi on 11/5/26.
//
#include "PhosphorTrail.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace {
    constexpr int textureWidth = 48;
    constexpr int textureHeight = 256;
    constexpr int headRow = 224;

    constexpr double intensity = 0.55;
    constexpr double trailWidthFactor = 2.5;
}

PhosphorTrail::PhosphorTrail() {
    surface_ = cairo_image_surface_create(
        CAIRO_FORMAT_ARGB32,
        textureWidth,
        textureHeight
    );

    if (cairo_surface_status(surface_) != CAIRO_STATUS_SUCCESS) {
        return;
    }

    cairo_surface_flush(surface_);

    auto* data = cairo_image_surface_get_data(surface_);
    const int stride = cairo_image_surface_get_stride(surface_);

    for (int y = 0; y < textureHeight; ++y) {
        auto* row = reinterpret_cast<std::uint32_t*>(
            data + y * stride
        );

        for (int x = 0; x < textureWidth; ++x) {

            // Horizontal soft falloff
            const double dx =
                (x + 0.5 - textureWidth * 0.5) /
                (textureWidth * 0.5);

            const double horizontal = std::pow(
                std::max(0.0, 1.0 - dx * dx),
                2.0
            ) * std::exp(-1.5 * dx * dx);

            // Asymmetric vertical falloff
            double vertical = 0.0;

            if (y <= headRow) {
                const double t =
                    static_cast<double>(headRow - y) / headRow;

                vertical =
                    std::pow(1.0 - t, 1.25) *
                    std::exp(-0.55 * t);
            } else {
                const double t =
                    static_cast<double>(y - headRow) /
                    (textureHeight - 1 - headRow);

                vertical = std::exp(-6.0 * t * t);
            }

            const double alpha =
                std::clamp(intensity * horizontal * vertical, 0.0, 1.0);

            // Premultiplied ARGB32 pixel
            const auto a = static_cast<std::uint32_t>(
                std::lround(alpha * 255.0)
            );

            const auto r = static_cast<std::uint32_t>(
                std::lround(alpha * 0.08 * 255.0)
            );

            const auto g = a;

            const auto b = static_cast<std::uint32_t>(
                std::lround(alpha * 0.13 * 255.0)
            );

            row[x] = (a << 24) | (r << 16) | (g << 8) | b;
        }
    }

    // The texture is immutable after this point.
    cairo_surface_mark_dirty(surface_);
}

PhosphorTrail::~PhosphorTrail() {
    if (surface_ != nullptr) {
        cairo_surface_destroy(surface_);
    }
}

void PhosphorTrail::draw(
    cairo_t* cr,
    double x,
    double headY,
    double glyphWidth,
    double characterHeight,
    int trailLength,
    double opacity
) const {
    if (surface_ == nullptr ||
        cairo_surface_status(surface_) != CAIRO_STATUS_SUCCESS) {
        return;
    }

    const double drawWidth = glyphWidth * trailWidthFactor;
    const double drawHeight = characterHeight * (trailLength + 1.0);

    const double left = x + glyphWidth * 0.6 - drawWidth * 0.6;

    // Align the brightest part with the leading glyph.
    const double top =
        headY - characterHeight * 0.5 -
        drawHeight * static_cast<double>(headRow) / textureHeight;

    cairo_save(cr);

    cairo_translate(cr, left, top);
    cairo_scale(
        cr,
        drawWidth / textureWidth,
        drawHeight / textureHeight
    );

    cairo_set_source_surface(cr, surface_, 0.0, 0.0);

    cairo_pattern_set_filter(
        cairo_get_source(cr),
        CAIRO_FILTER_BILINEAR
    );

    cairo_rectangle(cr, 0.0, 0.0, textureWidth, textureHeight);
    cairo_clip(cr);
    cairo_paint_with_alpha(cr, opacity);
    cairo_restore(cr);
}