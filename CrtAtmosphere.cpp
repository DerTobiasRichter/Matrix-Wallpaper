//
// Created by tobi on 11/3/26.
//

#include "CrtAtmosphere.h"

#include <algorithm>
#include <cmath>

CrtAtmosphere::CrtAtmosphere()
    : rng_(std::random_device{}())
{
    generateNoise();
}

void CrtAtmosphere::generateNoise() {
    noise_.resize(textureWidth * textureHeight);

    std::uniform_real_distribution<double> distribution(0.0, 1.0);

    for (double& value : noise_) {
        value = distribution(rng_);
    }
}

double CrtAtmosphere::sampleNoise(double x, double y) const {
    const auto wrap = [](int value, int size) {
        return (value % size + size) % size;
    };

    const int x0 = static_cast<int>(std::floor(x));
    const int y0 = static_cast<int>(std::floor(y));

    const double fx = x - std::floor(x);
    const double fy = y - std::floor(y);

    // Smoothstep interpolation
    const double sx = fx * fx * (3.0 - 2.0 * fx);
    const double sy = fy * fy * (3.0 - 2.0 * fy);

    const auto sample = [&](int px, int py) {
        return noise_[
            wrap(py, textureHeight) * textureWidth +
            wrap(px, textureWidth)
        ];
    };

    const double a = std::lerp(sample(x0, y0), sample(x0 + 1, y0), sx);
    const double b = std::lerp(sample(x0, y0 + 1), sample(x0 + 1, y0 + 1), sx);

    return std::lerp(a, b, sy);
}

void CrtAtmosphere::update(double deltaTime) {
    offsetX_ += driftSpeedX * deltaTime;
    offsetY_ += driftSpeedY * deltaTime;
}

void CrtAtmosphere::updateTexture() {
    pixels_.resize(textureWidth * textureHeight);

    for (int y = 0; y < textureHeight; ++y) {
        for (int x = 0; x < textureWidth; ++x) {
            // Sample the noise at a lower frequency to create broad clouds.
            const double value = sampleNoise(
                x * 0.12 + offsetX_,
                y * 0.12 + offsetY_
            );

            const auto green = static_cast<std::uint8_t>(
                std::clamp(value * intensity * 255.0, 0.0, 255.0)
            );

            // Cairo ARGB32 uses premultiplied alpha.
            pixels_[y * textureWidth + x] =
                0xFF000000u |
                (static_cast<std::uint32_t>(green) << 8);
        }
    }

    // Release the previous Cairo surface before replacing its pixel data.
    if (surface_ != nullptr) {
        cairo_surface_destroy(surface_);
        surface_ = nullptr;
    }

    // Create a new surface for the current frame.
    surface_ = cairo_image_surface_create_for_data(
        reinterpret_cast<unsigned char*>(pixels_.data()),
        CAIRO_FORMAT_ARGB32,
        textureWidth,
        textureHeight,
        textureWidth * sizeof(std::uint32_t)
    );
}

void CrtAtmosphere::draw(cairo_t* cr, int width, int height) {
    if (width <= 0 || height <= 0) {
        return;
    }

    updateTexture();

    cairo_save(cr);

    cairo_scale(
        cr,
        static_cast<double>(width) / textureWidth,
        static_cast<double>(height) / textureHeight
    );

    cairo_set_source_surface(cr, surface_, 0.0, 0.0);

    cairo_pattern_set_filter(
        cairo_get_source(cr),
        CAIRO_FILTER_BILINEAR
    );

    cairo_paint(cr);

    cairo_restore(cr);
}

CrtAtmosphere::~CrtAtmosphere() {
    if (surface_ != nullptr) {
        cairo_surface_destroy(surface_);
    }
}

