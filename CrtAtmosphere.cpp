//
// Created by tobi on 11/3/26.
//

#include "CrtAtmosphere.h"

#include <algorithm>
#include <cmath>


CrtAtmosphere::CrtAtmosphere()
    : rng_(std::random_device{}())
{
    generateField(fieldA_);
    generateField(fieldB_);
}

CrtAtmosphere::~CrtAtmosphere() {
    if (surface_ != nullptr) {
        cairo_surface_destroy(surface_);
    }
}

void CrtAtmosphere::generateField(std::vector<double>& field) {
    field.resize(textureWidth * textureHeight);

    std::uniform_real_distribution<double> distribution(0.0, 1.0);

    for (double& value : field) {
        value = distribution(rng_);
    }
}

void CrtAtmosphere::update(double deltaTime) {
    blend_ += deltaTime * blendSpeed_;

    if (blend_ >= 1.0) {
        blend_ = 0.0;
        fieldA_ = fieldB_;
        generateField(fieldB_);
    }
}

void CrtAtmosphere::updateTexture() {
    pixels_.resize(textureWidth * textureHeight);

    for (int y = 0; y < textureHeight; ++y) {
        for (int x = 0; x < textureWidth; ++x) {
            const std::size_t index = y * textureWidth + x;

            double value = std::lerp(fieldA_[index], fieldB_[index], blend_);
            const double pulse = 0.92 + 0.18 * std::sin(blend_ * 6.28318);
            value *= pulse;

            // Slight smoothing / shaping
            value = std::clamp(value, 0.0, 1.0);
            value = value * value * (3.0 - 2.0 * value); // smoothstep-like

            // Optional quantization for CRT-like blotches
            value = std::round(value * 5.0) / 5.0;

            const auto green = static_cast<std::uint8_t>(
                std::clamp(value * intensity * 255.0, 0.0, 255.0)
            );

            pixels_[index] =
                0xFF000000u |
                (static_cast<std::uint32_t>(green) << 8);
        }
    }

    if (surface_ != nullptr) {
        cairo_surface_destroy(surface_);
        surface_ = nullptr;
    }

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
        CAIRO_FILTER_NEAREST
    );

    cairo_paint(cr);

    cairo_restore(cr);
}