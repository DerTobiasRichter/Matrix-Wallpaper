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

#include <cstddef>
#include <string>

enum class Severity {
    Info,
    Warning,
    Error
};

struct MatrixColor {
    double red;
    double green;
    double blue;
};

struct MatrixMessage {
    std::string text;
    Severity severity = Severity::Info;
};

class MatrixStream {
public:
    MatrixStream(MatrixMessage message, std::size_t column, double speed, std::size_t minLength = 28);

    void update(double deltaTime);

    [[nodiscard]]
    std::size_t column() const;
    [[nodiscard]]
    double positionY() const;
    [[nodiscard]]
    double speed() const;
    [[nodiscard]]
    const std::string& text() const;
    [[nodiscard]]
    Severity severity() const;
    [[nodiscard]]
    double severityFadeLength() const;

    [[nodiscard]]
    bool isFinished(double viewportHeight, double characterHeight) const;

private:
    std::size_t column_;
    double positionY_;
    double speed_;
    double severityFadeLength_;

    [[nodiscard]]
    static MatrixColor getGlyphColor(Severity severity, std::size_t index, std::size_t length);


    std::string text_;
    Severity severity_;
};