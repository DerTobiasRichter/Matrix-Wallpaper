//
// Created by tobi on 11/6/26.
//
#pragma once

#include <cstddef>
#include <deque>
#include <mutex>
#include <stop_token>
#include <thread>
#include <vector>

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

    // Linux eventfd: readable when the first queued event arrives.
    // GTK watches this FD without polling; never close it from GTK.
    [[nodiscard]] int wakeFd() const noexcept;
    [[nodiscard]] bool hasPending() const;

private:
    void run(std::stop_token stopToken);
    void enqueue(MatrixMessage message);

    static constexpr std::size_t maxQueuedMessages = 64;

    mutable std::mutex queueMutex_;
    std::deque<MatrixMessage> queue_;

    // Created before worker_, closed after its thread has exited.
    int wakeFd_ = -1;

    // Must be declared last so it is stopped before queue/mutex/fd are destroyed.
    std::jthread worker_;
};