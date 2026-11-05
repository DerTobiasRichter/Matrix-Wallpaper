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
    static constexpr int textureWidth = 4092;
    static constexpr int textureHeight = 2048;
    static constexpr double intensity = 0.08;

    std::mt19937 rng_;

    std::vector<double> fieldA_;
    std::vector<double> fieldB_;
    std::vector<std::uint32_t> pixels_;

    cairo_surface_t* surface_ = nullptr;

    double blend_ = 0.0;
    double blendSpeed_ = 0.08; // very slow

    void generateField(std::vector<double>& field);
    void updateTexture();
};