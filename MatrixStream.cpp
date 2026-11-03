//
// Created by tobi on 11/3/26.
//
#include "MatrixStream.h"

#include <algorithm>
#include <random>
#include <utility>

MatrixStream::MatrixStream(
    MatrixMessage message,
    std::size_t column,
    double speed,
    std::size_t minLength
)
    : column_(column),
      positionY_(0.0),
      speed_(speed),
      text_(std::move(message.text)),
      severity_(message.severity)
{
    // Pad short messages without modifying the original input object.
    static constexpr char glyphs[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

    static thread_local std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<std::size_t> distribution(
        0, sizeof(glyphs) - 2
    );

    severityFadeLength_ = 10.0 + std::uniform_int_distribution<int>(0, 9)(rng);

    while (text_.size() < minLength) {
        text_ += glyphs[distribution(rng)];
    }
}

void MatrixStream::update(double deltaTime) {
    positionY_ += speed_ * std::max(0.0, deltaTime);
}

std::size_t MatrixStream::column() const {
    return column_;
}

double MatrixStream::positionY() const {
    return positionY_;
}

double MatrixStream::speed() const {
    return speed_;
}

const std::string& MatrixStream::text() const {
    return text_;
}

Severity MatrixStream::severity() const {
    return severity_;
}

double MatrixStream::severityFadeLength() const {
    return severityFadeLength_;
}



bool MatrixStream::isFinished(
    double viewportHeight,
    double characterHeight
) const {
    return positionY_ - text_.size() * characterHeight
           > viewportHeight;
}