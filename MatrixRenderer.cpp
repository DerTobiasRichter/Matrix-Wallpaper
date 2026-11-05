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
    if (width <= 0) {
        return;
    }

    const std::size_t newColumnCount = std::max(
        std::size_t{1},
        static_cast<std::size_t>(width / characterWidth)
    );

    if (newColumnCount == columnCount_) {
        return;
    }

    const std::size_t oldColumnCount = columnCount_;
    columnCount_ = newColumnCount;

    // Remove streams outside the resized viewport.
    if (newColumnCount < oldColumnCount) {
        std::erase_if(streams_, [newColumnCount](const ActiveStream& entry) {
            return entry.stream.column() >= newColumnCount;
        });
        return;
    }

    // Populate only newly available columns.
    std::bernoulli_distribution spawnDistribution(
        initialFillProbability
    );

    for (std::size_t column = oldColumnCount;
         column < newColumnCount;
         ++column) {

        if (!spawnDistribution(rng_)) {
            continue;
        }

        spawnStream({
            "systemd: Starting Network Manager service",
            Severity::Info
        }, column);
         }
}

void MatrixRenderer::spawnStream(MatrixMessage message, std::size_t column) {
    double minSpeed = 13.0;
    double maxSpeed = 33.4;

    switch (message.severity) {
        case Severity::Info:
            break;

        case Severity::Warning:
            minSpeed = 20.0;
            maxSpeed = 40.0;
            break;

        case Severity::Error:
            minSpeed = 38.0;
            maxSpeed = 43.0;
            break;
    }

    std::uniform_real_distribution<double> speedDistribution(
        minSpeed,
        maxSpeed
    );

    // Generate visual properties once per stream.
    const SeverityVisual visual =
        getSeverityVisual(message.severity);

    streams_.push_back({
        MatrixStream(
            std::move(message),
            column,
            speedDistribution(rng_)
        ),
        PhosphorTrail::randomTrailLength(rng_),
        visual
    });
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

    // Update
    for (auto& entry : streams_) {
        entry.stream.update(deltaTime);
    }

    std::erase_if(streams_, [height](const ActiveStream& entry) {
        const double lineHeight =
            entry.visual.fontSize + verticalPadding;

        return entry.stream.isFinished(height, lineHeight);
    });

    // Generate synthetic messages for the initial renderer test.
    // Limit the number of simultaneously active streams.
    const std::size_t maxStreams = std::max(
        std::size_t{1},
        static_cast<std::size_t>(columnCount_ * maxStreamDensity)
    );

    // Generate synthetic messages at a controlled rate.
    spawnAccumulator_ += deltaTime;

    constexpr int maxSpawnsPerUpdate = 4;
    int spawnCount = 0;

    while (spawnAccumulator_ >= spawnInterval &&
           spawnCount < maxSpawnsPerUpdate &&
           streams_.size() + pendingMessages_.size() < maxStreams) {

        spawnAccumulator_ -= spawnInterval;
        spawnTestMessage();
        ++spawnCount;
           }

    // Prevent excessive catch-up after long pauses.
    spawnAccumulator_ = std::min(
        spawnAccumulator_,
        spawnInterval * maxSpawnsPerUpdate
    );

    // Track occupied columns to prevent overlapping streams.
    std::vector<bool> occupied(columnCount_, false);

    for (const auto& entry : streams_) {
        occupied[entry.stream.column()] = true;
    }

    std::uniform_int_distribution<std::size_t> columnDistribution(
        0, columnCount_ - 1
    );

    for (auto& message : pendingMessages_) {
        if (streams_.size() >= maxStreams) {
            break;
        }

        const std::size_t startColumn = columnDistribution(rng_);

        for (std::size_t offset = 0; offset < columnCount_; ++offset) {
            const std::size_t column =
                (startColumn + offset) % columnCount_;

            if (occupied[column]) {
                continue;
            }

            spawnStream(std::move(message), column);
            occupied[column] = true;
            break;
        }
    }

    // Discard unspawned synthetic test messages.
    pendingMessages_.clear();
}

void MatrixRenderer::draw( cairo_t* cr, int width, int height)
{
    // Clear background
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_paint(cr);

    // Render CRT atmosphere
    atmosphere_.draw(cr, width, height);

    cairo_select_font_face(
        cr,
        "monospace",
        CAIRO_FONT_SLANT_NORMAL,
        CAIRO_FONT_WEIGHT_NORMAL
    );

    cairo_font_options_t* options = cairo_font_options_create();

    cairo_font_options_set_antialias(
        options,
        CAIRO_ANTIALIAS_GRAY
    );

    cairo_font_options_set_hint_style(
        options,
        CAIRO_HINT_STYLE_NONE
    );

    cairo_font_options_set_hint_metrics(
        options,
        CAIRO_HINT_METRICS_OFF
    );

    cairo_set_font_size(cr, fontSize);
    cairo_set_font_options(cr, options);
    cairo_font_options_destroy(options);


    for (const auto& entry : streams_) {
        const MatrixStream& stream = entry.stream;
        const std::string& text = stream.text();
        const SeverityVisual& visual = entry.visual;

        const double scale = visual.fontSize / fontSize;
        const double currentGlyphWidth = glyphWidth * scale;
        const double currentCharacterHeight = visual.fontSize + verticalPadding;

        const double x = stream.column() * characterWidth + (characterWidth - currentGlyphWidth) * 0.5;
        const double headY = stream.positionY();
        const double tailOpacityRatio = 0.59;

        cairo_set_font_size(cr, visual.fontSize);

        // Render one phosphor trail per stream
        phosphorTrail_.draw(
            cr,
            x,
            headY,
            currentGlyphWidth,
            currentCharacterHeight,
            entry.trailLength,
            visual.trailOpacity*tailOpacityRatio
        );

        for (std::size_t i = 0; i < text.size(); ++i) {
            const double y = headY - i * currentCharacterHeight;

            if (y < 0.0 || y > height) {
                continue;
            }

            const MatrixColor color = getGlyphColor(
                stream.severity(),
                i,
                stream.severityFadeLength()
            );

            // Stable pseudo-random highlight for each glyph
            std::uint32_t hash =
                static_cast<std::uint32_t>(stream.column()) * 0x9E3779B9u ^
                static_cast<std::uint32_t>(stream.speed() * 1000.0) ^
                static_cast<std::uint32_t>(i) * 0x85EBCA6Bu;

            hash ^= hash >> 16;
            hash *= 0x7FEB352Du;
            hash ^= hash >> 15;
            hash *= 0x846CA68Bu;
            hash ^= hash >> 16;

            constexpr int highlightProbability = 12;

            const bool highlighted = (hash % 100) < highlightProbability;
            const double glyphBrightness = highlighted ? 1.25 : 0.70;
            const double glyphIntensity = std::clamp(visual.brightness * glyphBrightness,0.0, 1.0);

            // Additional phosphor glow for highlighted glyphs
            if (highlighted) {
                const double radius = visual.fontSize * 1.23;

                const double centerX = x + currentGlyphWidth * 0.5;
                const double centerY = y - visual.fontSize * 0.4;

                // Match the decay already applied by getGlyphColor().
                constexpr double trailDecay = 0.015;
                const double trailFade = std::exp(
                    -trailDecay * static_cast<double>(i)
                );

                // Preserve the glyph's fading intensity when mixing towards white.
                constexpr double whiteMix = 0.55;

                const double hotRed = std::lerp(color.red, trailFade, whiteMix);
                const double hotGreen = std::lerp(color.green, trailFade, whiteMix);
                const double hotBlue = std::lerp(color.blue, trailFade, whiteMix);

                cairo_pattern_t* glow = cairo_pattern_create_radial(
                    centerX, centerY, 0.2,
                    centerX, centerY, radius
                );

                // Bright phosphor hotspot
                cairo_pattern_add_color_stop_rgba(
                    glow, 0.1,
                    hotRed, hotGreen, hotBlue,
                    glyphIntensity * 0.45
                );

                // Colored, diffuse halo
                cairo_pattern_add_color_stop_rgba(
                    glow, 0.2,
                    color.red, color.green, color.blue,
                    glyphIntensity * 0.35
                );

                cairo_pattern_add_color_stop_rgba(
                    glow, 1.0,
                    color.red, color.green, color.blue,
                    0.0
                );

                cairo_save(cr);

                // Add emitted light to the existing image.
                cairo_set_operator(cr, CAIRO_OPERATOR_ADD);

                cairo_set_source(cr, glow);
                cairo_rectangle(
                    cr,
                    centerX - radius,
                    centerY - radius,
                    radius * 2.0,
                    radius * 2.0
                );
                cairo_fill(cr);

                cairo_restore(cr);
                cairo_pattern_destroy(glow);
            }

            const char glyph[2] = {text[i], '\0'};

            cairo_new_path(cr);
            cairo_move_to(cr, x, y);
            cairo_text_path(cr, glyph);

            // Diffuse phosphor fringe around the glyph
            cairo_set_line_width(cr, 1.6);
            cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);

            cairo_set_source_rgba(
                cr,
                color.red,
                color.green,
                color.blue,
                std::clamp(
                    visual.brightness * glyphBrightness * 0.28,0.0, 1.0)
            );

            cairo_stroke_preserve(cr);

            // Softer glyph core
            cairo_set_source_rgba(
                cr,
                color.red,
                color.green,
                color.blue,
                std::clamp(
                    visual.brightness * glyphBrightness * 0.65, 0.0, 1.0)
            );

            cairo_fill(cr);
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

MatrixRenderer::SeverityVisual MatrixRenderer::getSeverityVisual(Severity severity) {
    SeverityVisual visual{};

    switch (severity) {
        case Severity::Info:
            visual = {18.0, 0.35, 0.30};
            break;

        case Severity::Warning:
            visual = {22.0, 0.45, 0.30};
            break;

        case Severity::Error:
            visual = {26.0, 0.56, 0.40};
            break;
    }

    // Independent variation of +/- 30% per property.
    std::uniform_real_distribution<double> variation(0.7, 1.3);

    visual.fontSize *= variation(rng_);

    visual.brightness = std::clamp(
        visual.brightness * variation(rng_),
        0.0, 1.0
    );

    visual.trailOpacity = std::clamp(
        visual.trailOpacity * variation(rng_),
        0.0, 1.0
    );

    return visual;
}