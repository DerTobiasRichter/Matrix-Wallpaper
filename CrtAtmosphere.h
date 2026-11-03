//
// Created by tobi on 11/3/26.
//
#pragma once

#include <cairo.h>
#include <cstdint>
#include <random>
#include <vector>

class CrtAtmosphere {
public:
    CrtAtmosphere();

    ~CrtAtmosphere();

    void update(double deltaTime);
    void draw(cairo_t* cr, int width, int height);

    CrtAtmosphere(const CrtAtmosphere&) = delete;
    CrtAtmosphere& operator=(const CrtAtmosphere&) = delete;

private:
    static constexpr int textureWidth = 24;
    static constexpr int textureHeight = 24;

    static constexpr double driftSpeedX = 0.35;
    static constexpr double driftSpeedY = -0.18;

    static constexpr double intensity = 0.065;

    std::mt19937 rng_;

    std::vector<double> noise_;
    std::vector<std::uint32_t> pixels_;

    cairo_surface_t* surface_ = nullptr;

    double offsetX_ = 0.0;
    double offsetY_ = 0.0;

    void generateNoise();
    void updateTexture();

    [[nodiscard]] double sampleNoise(double x, double y) const;
};