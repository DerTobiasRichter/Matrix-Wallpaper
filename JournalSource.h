/*
 * SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Tobias Richter
 * GitHub: https://github.com/DerTobiasRichter
 *
 * Created on: 2026-11-06
 * Last Modified: 2026-11-06
 *
 *
 */
#pragma once

#include <cstddef>
#include <deque>
#include <mutex>
#include <stop_token>
#include <thread>
#include <vector>
#include <atomic>

#include "MatrixStream.h"

// Reads new systemd journal records without touching GTK or MatrixRenderer.
// All journal I/O stays on the worker thread; the GTK thread drains a bounded queue.
class JournalSource final {
public:
    JournalSource();
    ~JournalSource();

    JournalSource(const JournalSource&) = delete;
    JournalSource& operator=(const JournalSource&) = delete;

    // Call only from the GTK main thread. Never blocks on journal I/O.
    [[nodiscard]]
    std::vector<MatrixMessage> drain(std::size_t maxMessages = 1);

    void setParanoiaMode(bool enabled) noexcept {
        paranoiaMode_.store(enabled);
    }

    [[nodiscard]] bool isParanoiaMode() const noexcept {
        return paranoiaMode_.load();
    }

    // Linux eventfd: readable when the first queued event arrives.
    // GTK watches this FD without polling; never close it from GTK.
    [[nodiscard]] int wakeFd() const noexcept;
    [[nodiscard]] bool hasPending() const;

private:
    void run(const std::stop_token& stopToken);
    void enqueue(MatrixMessage message);
    std::atomic_bool paranoiaMode_{false};

    static constexpr std::size_t maxQueuedMessages = 64;

    mutable std::mutex queueMutex_;
    std::deque<MatrixMessage> queue_;

    // Created before worker_, closed after its thread has exited.
    int wakeFd_ = -1;

    // Must be declared last so it is stopped before queue/mutex/fd are destroyed.
    std::jthread worker_;
};