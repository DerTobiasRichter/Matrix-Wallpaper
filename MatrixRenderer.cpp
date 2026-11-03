//
// Created by tobi on 11/3/26.
//

#include "MatrixRenderer.h"

#include <algorithm>
#include <string>
#include <utility>
#include <cmath>

MatrixRenderer::MatrixRenderer()
    : rng_(std::random_device{}())
{
}

void MatrixRenderer::initializeColumns(int width) {
    if (columnCount_ != 0 || width <= 0) {
        return;
    }

    columnCount_ = std::max(
        std::size_t{1},
        static_cast<std::size_t>(width / characterWidth)
    );

    // Fill every column once before using random placement.
    for (std::size_t column = 0; column < columnCount_; ++column) {
        spawnStream({
            "systemd: Starting Network Manager service",
            Severity::Info
        }, column);
    }

    nextInitialColumn_ = columnCount_;
}

void MatrixRenderer::spawnStream(
    MatrixMessage message,
    std::size_t column
) {
    double minSpeed = 60.0;
    double maxSpeed = 99.0;

    switch (message.severity) {
        case Severity::Info:
            break;

        case Severity::Warning:
            minSpeed = 99.8;
            maxSpeed = 133.0;
            break;

        case Severity::Error:
            minSpeed = 133.5;
            maxSpeed = 159.3;
            break;
    }

    std::uniform_real_distribution<double> speedDistribution(
        minSpeed,
        maxSpeed
    );

    streams_.emplace_back(
        std::move(message),
        column,
        speedDistribution(rng_)
    );
}

void MatrixRenderer::append(MatrixMessage message) {
    pendingMessages_.push_back(std::move(message));
}

void MatrixRenderer::spawnTestMessage() {
    static const std::vector<MatrixMessage> messages = {
        {"systemd: Starting Network Manager", Severity::Info},
        {"WARNING: Network connection temporarily unavailable", Severity::Warning},
        {"ERROR: Failed to initialize network device", Severity::Error},
        {"kernel: USB device connected", Severity::Info},
        {"NetworkManager: Connection activated", Severity::Info}
    };

    std::uniform_int_distribution<std::size_t> messageDistribution(
        0, messages.size() - 1
    );

    append(messages[messageDistribution(rng_)]);
}

void MatrixRenderer::update(
    double deltaTime,
    int width,
    int height
) {

    atmosphere_.update(deltaTime);

    elapsedTime_ += deltaTime;
    initializeColumns(width);

    for (auto& stream : streams_) {
        stream.update(deltaTime);
    }

    std::erase_if(streams_, [height](const MatrixStream& stream) {
        return stream.isFinished(height, characterHeight);
    });

    // Generate synthetic messages for the initial renderer test.
    spawnAccumulator_ += deltaTime;

    if (spawnAccumulator_ >= 0.15) {
        spawnAccumulator_ = 0.0;
        spawnTestMessage();
    }

    if (columnCount_ == 0 || pendingMessages_.empty()) {
        return;
    }

    std::uniform_int_distribution<std::size_t> columnDistribution(
        0, columnCount_ - 1
    );

    for (auto& message : pendingMessages_) {
        spawnStream(std::move(message), columnDistribution(rng_));
    }

    pendingMessages_.clear();
}

void MatrixRenderer::draw(
    cairo_t* cr,
    int width,
    int height
) {
    // Clear the background
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_paint(cr);

    // Render the CRT atmosphere behind the glyphs
    atmosphere_.draw(cr, width, height);

    // Set Font Face
    cairo_select_font_face(
        cr,
        "monospace",
        CAIRO_FONT_SLANT_NORMAL,
        CAIRO_FONT_WEIGHT_NORMAL
    );

    cairo_set_font_size(cr, fontSize);

    for (const auto& stream : streams_) {
        const std::string& text = stream.text();

        // Soft phosphor trail behind the falling stream
        const double x = stream.column() * characterWidth;
        const double headY = stream.positionY();

        // Radial phosphor glow around the leading glyph

        constexpr double glowRadius = 18.0;

        cairo_pattern_t* glowPattern = cairo_pattern_create_radial(
            x + glyphWidth * 0.5,
            headY - glyphHeight * 0.5,
            0.0,
            x + glyphWidth * 0.5,
            headY - glyphHeight * 0.5,
            glowRadius
        );

        cairo_pattern_add_color_stop_rgba(
            glowPattern,
            0.0,
            0.2, 1.0, 0.2, 0.35
        );

        cairo_pattern_add_color_stop_rgba(
            glowPattern,
            1.0,
            0.0, 0.8, 0.0, 0.0
        );

        cairo_save(cr);

        cairo_set_source(cr, glowPattern);

        cairo_rectangle(
            cr,
            x - glowRadius,
            headY - glowRadius - glyphHeight,
            glowRadius * 2.0 + glyphWidth,
            glowRadius * 2.0 + glyphHeight
        );

        cairo_fill(cr);

        cairo_restore(cr);
        cairo_pattern_destroy(glowPattern);

        for (std::size_t i = 0; i < text.size(); ++i) {
            const double y =
                stream.positionY() - i * characterHeight;

            if (y < 0.0 || y > height) {
                continue;
            }

            const MatrixColor color = getGlyphColor(
                stream.severity(),
                i,
                stream.severityFadeLength()
            );

            cairo_set_source_rgb(
                cr,
                color.red,
                color.green,
                color.blue
            );

            char glyph[2] = {text[i], '\0'};

            cairo_move_to(cr, x, y);
            cairo_show_text(cr, glyph);
        }
    }
}


MatrixColor MatrixRenderer::getGlyphColor( Severity severity, std::size_t index,  double severityFadeLength)
{
    const MatrixColor green{0.0, 1.0, 0.0};

    MatrixColor startColor = green;

    switch (severity) {
        case Severity::Info:
            break;

        case Severity::Warning:
            startColor = {1.0, 0.5, 0.0};
            break;

        case Severity::Error:
            startColor = {1.0, 0.0, 0.0};
            break;
    }

    // Transition from severity color to green within n glyphs.
    const double factor = std::clamp(
        static_cast<double>(index) / severityFadeLength,
        0.0,
        1.0
    );

    // Reduce brightness gradually along the trail.
    constexpr double trailDecay = 0.015;

    const double brightness = std::exp(
        -trailDecay * static_cast<double>(index)
    );

    return {
        (startColor.red * (1.0 - factor) + green.red * factor) * brightness,
        (startColor.green * (1.0 - factor) + green.green * factor) * brightness,
        (startColor.blue * (1.0 - factor) + green.blue * factor) * brightness
    };
}
