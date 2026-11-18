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
#include <systemd/sd-journal.h>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <system_error>
#include <unistd.h>
#include <sys/eventfd.h>
#include <random>

#include "JournalSource.h"


namespace {

using JournalHandle = std::unique_ptr<sd_journal, decltype(&sd_journal_close)>;

int createWakeFd() {
    const int fd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (fd < 0) {
        throw std::system_error(errno, std::generic_category(), "eventfd");
    }
    return fd;
}

// sd_journal_get_data() returns FIELD=value, not merely the value.
// Copy the result immediately: its borrowed memory is invalidated by later reads.
std::string readField(sd_journal* journal, std::string_view field) {
    const void* rawData = nullptr;
    std::size_t rawSize = 0;
    const std::string name(field);

    if (sd_journal_get_data(journal, name.c_str(), &rawData, &rawSize) < 0 ||
        rawData == nullptr || rawSize <= field.size()) {
        return {};
    }

    const std::string_view data(static_cast<const char*>(rawData), rawSize);
    if (!data.starts_with(field) || data[field.size()] != '=') {
        return {};
    }

    return std::string(data.substr(field.size() + 1));
}

Severity toSeverity(std::string_view priority) {
    if (priority.size() != 1 || priority[0] < '0' || priority[0] > '7') {
        return Severity::Info;
    }

    // Syslog priorities 0-3: emergency/alert/critical/error.
    // 4: warning; 5-7: notice/info/debug.
    const int value = priority[0] - '0';
    if (value <= 3) {
        return Severity::Error;
    }
    return value == 4 ? Severity::Warning : Severity::Info;
}

// MatrixStream currently renders one byte per glyph. Until UTF-8 rendering is
// implemented, convert to safe printable ASCII and cap cached texture height.
std::string toStreamText(std::string_view input) {
    constexpr std::size_t maxCharacters = 120;
    std::string result;
    result.reserve(std::min(input.size(), maxCharacters));

    bool previousWasSpace = false;
    for (std::size_t i = 0; i < input.size() && result.size() < maxCharacters; ++i) {
        const unsigned char ch = static_cast<unsigned char>(input[i]);

        if (ch >= 0x80) {
            // Replace a UTF-8 sequence with a single placeholder.
            if (!result.empty() && result.back() != '?') {
                result.push_back('?');
            } else if (result.empty()) {
                result.push_back('?');
            }
            while (i + 1 < input.size() &&
                   (static_cast<unsigned char>(input[i + 1]) & 0xC0) == 0x80) {
                ++i;
            }
            previousWasSpace = false;
            continue;
        }

        if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r') {
            if (!previousWasSpace && !result.empty()) {
                result.push_back(' ');
                previousWasSpace = true;
            }
        } else if (ch >= 32 && ch <= 126) {
            result.push_back(static_cast<char>(ch));
            previousWasSpace = false;
        }
    }

    if (!result.empty() && result.back() == ' ') {
        result.pop_back();
    }
    return result;
}
std::string randomizeText(std::size_t length) {
    thread_local std::mt19937 generator(std::random_device{}());

    constexpr std::string_view alphabet =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789";

    std::uniform_int_distribution<std::size_t> distribution(
        0, alphabet.size() - 1
    );

    std::string result;
    result.reserve(length);

    for (std::size_t i = 0; i < length; ++i) {
        result.push_back(alphabet[distribution(generator)]);
    }

    return result;
}

}  // namespace

JournalSource::JournalSource()
    : wakeFd_(createWakeFd()) {
    try {
        worker_ = std::jthread([this](std::stop_token stopToken) {
            run(stopToken);
        });
    } catch (...) {
        ::close(wakeFd_);
        wakeFd_ = -1;
        throw;
    }
}

JournalSource::~JournalSource() {
    worker_.request_stop();
    // sd_journal_wait() is bounded to 250 ms, so shutdown cannot hang forever
    // waiting for another journal entry.
    if (worker_.joinable()) {
        worker_.join();
    }
    if (wakeFd_ >= 0) {
        ::close(wakeFd_);
    }
}

int JournalSource::wakeFd() const noexcept {
    return wakeFd_;
}

bool JournalSource::hasPending() const {
    std::lock_guard lock(queueMutex_);
    return !queue_.empty();
}

std::vector<MatrixMessage> JournalSource::drain(std::size_t maxMessages) {
    std::vector<MatrixMessage> messages;
    std::lock_guard lock(queueMutex_);

    const std::size_t count = std::min(maxMessages, queue_.size());
    messages.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        // Present urgent records promptly even when info messages are queued.
        auto next = std::find_if(queue_.begin(), queue_.end(),
            [](const MatrixMessage& item) {
                return item.severity != Severity::Info;
            });
        if (next == queue_.end()) {
            next = queue_.begin();
        }
        messages.push_back(std::move(*next));
        queue_.erase(next);
    }
    return messages;
}

void JournalSource::enqueue(MatrixMessage message) {
    bool wake = false;
    {
        std::lock_guard lock(queueMutex_);

        if (queue_.size() >= maxQueuedMessages) {
            auto evict = std::find_if(queue_.begin(), queue_.end(),
                [](const MatrixMessage& item) {
                    return item.severity == Severity::Info;
                });
            if (evict == queue_.end()) {
                if (message.severity == Severity::Info) {
                    return;
                }
                if (message.severity == Severity::Warning) {
                    evict = std::find_if(queue_.begin(), queue_.end(),
                        [](const MatrixMessage& item) {
                            return item.severity == Severity::Warning;
                        });
                    if (evict == queue_.end()) {
                        return;
                    }
                } else {
                    evict = queue_.begin();
                }
            }
            queue_.erase(evict);
        }

        wake = queue_.empty();
        queue_.push_back(std::move(message));
    }

    // Notify GTK only for the empty -> nonempty transition.
    // eventfd coalesces notifications and is safe to write from this thread.
    if (wake && wakeFd_ >= 0) {
        const std::uint64_t one = 1;
        const auto written = ::write(wakeFd_, &one, sizeof(one));
        (void)written; // EAGAIN means a notification is already pending.
    }
}

void JournalSource::run(const std::stop_token& stopToken) {
    sd_journal* raw = nullptr;
    const int opened = sd_journal_open(&raw, SD_JOURNAL_LOCAL_ONLY);
    if (opened < 0) {
        std::cerr << "[JournalSource] open failed: "
                  << std::strerror(-opened) << '\n';
        return;
    }
    JournalHandle journal(raw, &sd_journal_close);

    // No need to decompress / copy arbitrarily large journal message fields.
    sd_journal_set_data_threshold(journal.get(), 4096);

    // Replay the latest records, then follow subsequent journal entries.
    constexpr std::uint64_t historyCount = 40;
    int result = sd_journal_seek_tail(journal.get());
    if (result < 0) {
        std::cerr << "[JournalSource] seek failed: "
                  << std::strerror(-result) << '\n';
        return;
    }

    result = sd_journal_previous_skip(journal.get(), historyCount);
    if (result < 0) {
        std::cerr << "[JournalSource] history positioning failed: "
                  << std::strerror(-result) << '\n';
        return;
    }
    bool currentEntryReady = result > 0;

    const std::string ownPid = std::to_string(getpid());

    while (!stopToken.stop_requested()) {
        if (currentEntryReady) {
            currentEntryReady = false;
            result = 1;
        } else {
            result = sd_journal_next(journal.get());
        }
        if (result < 0) {
            std::cerr << "[JournalSource] read failed: "
                      << std::strerror(-result) << '\n';
            break;
        }

        if (result == 0) {
            // The worker sleeps until a new entry arrives or shutdown is needed.
            result = sd_journal_wait(journal.get(), 250'000);
            if (result < 0) {
                std::cerr << "[JournalSource] wait failed: "
                          << std::strerror(-result) << '\n';
                break;
            }
            continue;
        }

        // Do not feed our own journal output back into the wallpaper.
        if (readField(journal.get(), "_PID") == ownPid) {
            continue;
        }

        const std::string message = readField(journal.get(), "MESSAGE");
        if (message.empty()) {
            continue;
        }

        std::string identifier = readField(journal.get(), "SYSLOG_IDENTIFIER");
        if (identifier.empty()) {
            identifier = readField(journal.get(), "_COMM");
        }

        std::string combined;
        if (!identifier.empty()) {
            combined = identifier + ": ";
        }
        combined += message;

        MatrixMessage matrixMessage{
            toStreamText(combined),
            toSeverity(readField(journal.get(), "PRIORITY"))
        };

        if (paranoiaMode_.load(std::memory_order_relaxed)) {
            matrixMessage.text = randomizeText(matrixMessage.text.size());
        }

        if (!matrixMessage.text.empty()) {
            enqueue(std::move(matrixMessage));
        }
    }
}
