#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <utility>

#include <gtk/gtk.h>
#include <gtk4-layer-shell.h>
#include <glib-unix.h>
#include <sys/resource.h>
#include <unistd.h>

#include "JournalSource.h"
#include "MatrixRenderer.h"

namespace {
constexpr guint frameIntervalMs = 66;         // Approximately 15 FPS while active.
constexpr double journalImportInterval = 0.49; // At most ~2 new streams / second.

// RUSAGE_SELF covers the whole application, including the journal worker.
double processCpuSeconds() {
    rusage usage{};
    if (::getrusage(RUSAGE_SELF, &usage) != 0) {
        return 0.0;
    }
    const auto asSeconds = [](const timeval& value) {
        return static_cast<double>(value.tv_sec) + value.tv_usec / 1'000'000.0;
    };
    return asSeconds(usage.ru_utime) + asSeconds(usage.ru_stime);
}
} // namespace

struct MatrixState {
    JournalSource journalSource;
    MatrixRenderer renderer;

    GtkWidget* drawingArea = nullptr;
    guint timerId = 0;
    guint journalWatchId = 0;
    gint64 lastUpdate = 0;
    double journalAccumulator = journalImportInterval;

    // Cairo callback metrics (not total GTK/GSK render duration).
    gint64 reportStart = 0;
    gint64 totalDrawTime = 0;
    gint64 maxDrawTime = 0;
    std::uint64_t frameCount = 0;

    gint64 updateReportStart = 0;
    std::uint64_t updateCount = 0;
    double cpuReportStart = 0.0;
};

static void draw(
    GtkDrawingArea* /*area*/,
    cairo_t* cr,
    int width,
    int height,
    gpointer userData
) {
    auto* state = static_cast<MatrixState*>(userData);
    const gint64 start = g_get_monotonic_time();

    state->renderer.draw(cr, width, height);

    const gint64 end = g_get_monotonic_time();
    const gint64 duration = end - start;

    if (state->reportStart == 0) {
        state->reportStart = start;
    }

    state->totalDrawTime += duration;
    state->maxDrawTime = std::max(state->maxDrawTime, duration);
    ++state->frameCount;

    const double elapsed = (end - state->reportStart) / 1'000'000.0;
    if (elapsed >= 5.0) {
        const double averageMs =
            static_cast<double>(state->totalDrawTime) / state->frameCount / 1000.0;
        const double maximumMs = state->maxDrawTime / 1000.0;

        std::cout << std::fixed << std::setprecision(2)
                  << "[Render] " << width << 'x' << height
                  << " | FPS: " << state->frameCount / elapsed
                  << " | Avg: " << averageMs << " ms"
                  << " | Max: " << maximumMs << " ms\n";

        state->reportStart = end;
        state->totalDrawTime = 0;
        state->maxDrawTime = 0;
        state->frameCount = 0;
    }
}

static gboolean update(gpointer userData) {
    auto* state = static_cast<MatrixState*>(userData);
    const gint64 now = g_get_monotonic_time();

    const double deltaTime = std::clamp(
        static_cast<double>(now - state->lastUpdate) / 1'000'000.0,
        0.0, 0.1
    );
    state->lastUpdate = now;

    // A token bucket with capacity 1: at most one journal record per 490 ms.
    // No active/pending records? Keep the token for immediate use on next event.
    state->journalAccumulator = std::min(
        journalImportInterval,
        state->journalAccumulator + deltaTime
    );

    if (state->journalAccumulator >= journalImportInterval &&
        state->journalSource.hasPending()) {
        auto messages = state->journalSource.drain(1);
        if (!messages.empty()) {
            for (auto& message : messages) {
                state->renderer.append(std::move(message));
            }
            state->journalAccumulator -= journalImportInterval;
        }
    }

    const int width = gtk_widget_get_width(state->drawingArea);
    const int height = gtk_widget_get_height(state->drawingArea);

    const bool wasActive = state->renderer.hasActiveStreams();
    state->renderer.update(deltaTime, width, height);
    const bool isActive = state->renderer.hasActiveStreams();

    // Repaint while raining; repaint ONCE more to erase the last expired stream.
    if (wasActive || isActive) {
        gtk_widget_queue_draw(state->drawingArea);
    }

    ++state->updateCount;
    const double elapsed = (now - state->updateReportStart) / 1'000'000.0;
    if (elapsed >= 5.0) {
        const double currentCpuSeconds = processCpuSeconds();
        const double cpuPercent =
            (currentCpuSeconds - state->cpuReportStart) / elapsed * 100.0;

        std::cout << std::fixed << std::setprecision(2)
                  << "[Update] " << state->updateCount / elapsed << " Hz"
                  << " | Streams: " << state->renderer.activeStreamCount()
                  << " | CPU: " << cpuPercent << "% of one core\n";

        state->updateCount = 0;
        state->updateReportStart = now;
        state->cpuReportStart = currentCpuSeconds;
    }

    // No work remains: remove the 66ms timer completely, do not merely skip frames.
    if (!isActive && !state->renderer.hasPendingMessages() &&
        !state->journalSource.hasPending()) {
        state->timerId = 0;
        std::cout << "[Animation] idle\n";
        return G_SOURCE_REMOVE;
    }

    return G_SOURCE_CONTINUE;
}

static void startAnimation(MatrixState* state) {
    if (state->timerId != 0) {
        return;
    }

    const gint64 now = g_get_monotonic_time();
    state->lastUpdate = now; // No multi-minute deltaTime after waking from idle.
    state->journalAccumulator = journalImportInterval;
    state->updateReportStart = now;
    state->updateCount = 0;
    state->cpuReportStart = processCpuSeconds();
    state->reportStart = 0;
    state->totalDrawTime = 0;
    state->maxDrawTime = 0;
    state->frameCount = 0;

    state->timerId = g_timeout_add_full(
        G_PRIORITY_DEFAULT,
        frameIntervalMs,
        update,
        state,
        nullptr
    );
    std::cout << "[Animation] active (15 FPS cap)\n";
}

static gboolean onJournalReady(
    gint fd,
    GIOCondition condition,
    gpointer userData
) {
    auto* state = static_cast<MatrixState*>(userData);
    std::uint64_t notifications = 0;
    while (::read(fd, &notifications, sizeof(notifications)) ==
           static_cast<ssize_t>(sizeof(notifications))) {
        // eventfd is nonblocking and coalesces concurrent notifications.
    }

    if ((condition & (G_IO_HUP | G_IO_ERR)) != 0) {
        state->journalWatchId = 0;
        return G_SOURCE_REMOVE;
    }

    // This callback runs on the GTK thread; no GTK APIs are called by JournalSource.
    startAnimation(state);
    return G_SOURCE_CONTINUE;
}

static void activate(GtkApplication* app, gpointer user_data) {
    GtkWidget* window = gtk_application_window_new(app);

    gtk_window_set_title(GTK_WINDOW(window), "Matrix Wallpaper");
    gtk_window_set_decorated(GTK_WINDOW(window), FALSE);

    // Initialize Wayland layer shell before the window is realized.
    gtk_layer_init_for_window(GTK_WINDOW(window));

    gtk_layer_set_namespace(
        GTK_WINDOW(window),
        "matrix-wallpaper"
    );

    // Place the wallpaper behind normal application windows.
    gtk_layer_set_layer(
        GTK_WINDOW(window),
        GTK_LAYER_SHELL_LAYER_BACKGROUND
    );

    // Fill the entire monitor.
    gtk_layer_set_anchor(
        GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_TOP, TRUE
    );
    gtk_layer_set_anchor(
        GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE
    );
    gtk_layer_set_anchor(
        GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_LEFT, TRUE
    );
    gtk_layer_set_anchor(
        GTK_WINDOW(window), GTK_LAYER_SHELL_EDGE_RIGHT, TRUE
    );

    // Do not reserve workspace space.
    gtk_layer_set_exclusive_zone(GTK_WINDOW(window), -1);

    // Wallpaper must never receive keyboard focus.
    gtk_layer_set_keyboard_mode(
        GTK_WINDOW(window),
        GTK_LAYER_SHELL_KEYBOARD_MODE_NONE
    );

    gtk_window_set_title(GTK_WINDOW(window), "Matrix Wallpaper");

    GtkWidget* drawingArea = gtk_drawing_area_new();
    auto* state = new MatrixState{};
    state->drawingArea = drawingArea;

    gtk_drawing_area_set_draw_func(
        GTK_DRAWING_AREA(drawingArea), draw, state, nullptr
    );
    gtk_window_set_child(GTK_WINDOW(window), drawingArea);

    // Subscribe to the worker's eventfd; unlike a timer, this sleeps when idle.
    state->journalWatchId = g_unix_fd_add_full(
        G_PRIORITY_DEFAULT,
        state->journalSource.wakeFd(),
        static_cast<GIOCondition>(G_IO_IN | G_IO_ERR | G_IO_HUP),
        onJournalReady,
        state,
        nullptr
    );

    g_signal_connect(
        window, "destroy",
        G_CALLBACK(+[](GtkWidget*, gpointer data) {
            auto* state = static_cast<MatrixState*>(data);
            if (state->journalWatchId != 0) {
                g_source_remove(state->journalWatchId);
            }
            if (state->timerId != 0) {
                g_source_remove(state->timerId);
            }
            delete state;
        }),
        state
    );

    gtk_window_present(GTK_WINDOW(window));
}

int main(int argc, char** argv) {
    GtkApplication* app = gtk_application_new(
        "dev.matrix.wallpaper", G_APPLICATION_DEFAULT_FLAGS
    );
    g_signal_connect(app, "activate", G_CALLBACK(activate), nullptr);
    const int result = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return result;
}
