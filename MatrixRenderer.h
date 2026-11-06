//
// Created by tobi on 11/3/26.
//

#pragma once

#include "MatrixStream.h"
#include "PhosphorTrail.h"
#include "CrtAtmosphere.h"

#include <cstddef>
#include <memory>
#include <random>
#include <vector>
#include <cairo.h>

class MatrixRenderer {
public:
    MatrixRenderer();

    void update(double deltaTime, int width, int height);
    void draw(cairo_t* cr, int width, int height);
    void append(MatrixMessage message);

private:
    struct SeverityVisual {
        double fontSize;
        double brightness;
        double trailOpacity;
    };

    using CairoSurface = std::unique_ptr<
        cairo_surface_t,
        decltype(&cairo_surface_destroy)
    >;

    struct ActiveStream {
        MatrixStream stream;
        int trailLength{};
        SeverityVisual visual{};

        // RAII ownership; released automatically when a stream expires.
        CairoSurface glyphSurface{nullptr, &cairo_surface_destroy};
        double surfaceXOffset = 0.0;
        double surfaceYOffset = 0.0;
    };

    static constexpr double fontSize = 22.0;
    static constexpr double maxFontSize = 34.0;

    // Spacing between glyphs
    static constexpr double horizontalPadding = 3.0;
    static constexpr double verticalPadding = 3.0;

    // Actual glyph dimensions (temporary approximation)
    static constexpr double glyphWidth = 13.2;
    static constexpr double glyphHeight = 22.0;

    // Distance between consecutive glyph positions
    static constexpr double characterWidth = glyphWidth * (maxFontSize / fontSize) + horizontalPadding;
    static constexpr double characterHeight = glyphHeight + verticalPadding;

    // Rain density configuration
    static constexpr double initialFillProbability = 0.08;
    static constexpr double maxStreamDensity = 0.40;
    static constexpr double spawnInterval = 0.49;

    // CTR Nebula
    double elapsedTime_ = 0.0;

    std::vector<ActiveStream> streams_;
    std::vector<MatrixMessage> pendingMessages_;
    PhosphorTrail phosphorTrail_;
    CrtAtmosphere atmosphere_;

    std::mt19937 rng_;

    std::size_t columnCount_ = 0;
    std::size_t nextInitialColumn_ = 0;

    double spawnAccumulator_ = 0.0;

    [[nodiscard]]
    SeverityVisual getSeverityVisual(Severity severity);

    MatrixColor getGlyphColor(Severity severity, std::size_t index, double severityFadeLength);
    void initializeColumns(int width);
    void spawnStream(MatrixMessage message, std::size_t column);
    void spawnTestMessage();
    void buildGlyphSurface(ActiveStream& entry);
    void renderGlyphs(cairo_t* cr, const ActiveStream& entry,
                      double x, double headY);
};