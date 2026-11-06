#include <iostream>
#include <algorithm>
#include <gtk/gtk.h>
#include <cstdint>
#include <iomanip>
#include <utility>

#include "MatrixRenderer.h"
#include "JournalSource.h"

struct MatrixState {
    JournalSource journalSource;
    double journalAccumulator = 0.0;

    GtkWidget* drawingArea = nullptr;
    guint timerId = 0;
    gint64 lastUpdate = 0;

    // Rendering performance metrics
    gint64 reportStart = 0;
    gint64 totalDrawTime = 0;
    gint64 maxDrawTime = 0;
    std::uint64_t frameCount = 0;

    gint64 updateReportStart = 0;
    std::uint64_t updateCount = 0;

    MatrixRenderer renderer;
};

static void draw(
    GtkDrawingArea* area,
    cairo_t* cr,
    int width,
    int height,
    gpointer user_data
) {
    auto* state = static_cast<MatrixState*>(user_data);

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

    const double elapsed =
        static_cast<double>(end - state->reportStart) / 1'000'000.0;

    // Report performance every five seconds.
    if (elapsed >= 5.0) {
        const double averageMs =
            static_cast<double>(state->totalDrawTime) /
            state->frameCount / 1000.0;

        const double maximumMs =
            static_cast<double>(state->maxDrawTime) / 1000.0;

        const double fps = state->frameCount / elapsed;

        std::cout
            << std::fixed << std::setprecision(2)
            << "[Render] "
            << width << "x" << height
            << " | FPS: " << fps
            << " | Avg: " << averageMs << " ms"
            << " | Max: " << maximumMs << " ms"
            << '\n';

        state->reportStart = end;
        state->totalDrawTime = 0;
        state->maxDrawTime = 0;
        state->frameCount = 0;
    }
}

static gboolean update(gpointer user_data) {
    auto* state = static_cast<MatrixState*>(user_data);

    const gint64 now = g_get_monotonic_time();

    const double deltaTime = std::clamp(
        static_cast<double>(now - state->lastUpdate) / 1'000'000.0,
        0.0,
        0.1
    );

    state->lastUpdate = now;

    const int width = gtk_widget_get_width(state->drawingArea);
    const int height = gtk_widget_get_height(state->drawingArea);

    state->journalAccumulator += deltaTime;

    constexpr double importInterval = 0.49;

    if (state->journalAccumulator >= importInterval) {
        state->journalAccumulator -= importInterval;

        for (auto& message : state->journalSource.drain(1)) {
            state->renderer.append(std::move(message));
        }
    }

    state->renderer.update(deltaTime, width, height);

    gtk_widget_queue_draw(state->drawingArea);

    ++state->updateCount;

    if (state->updateReportStart == 0) {
        state->updateReportStart = now;
    }

    const double elapsed =
        (now - state->updateReportStart) / 1'000'000.0;

    if (elapsed >= 5.0) {
        std::cout
            << "[Update] "
            << state->updateCount / elapsed
            << " Hz\n";

        state->updateCount = 0;
        state->updateReportStart = now;
    }

    return G_SOURCE_CONTINUE;
}

static void activate(GtkApplication* app, gpointer user_data) {
    GtkWidget* window = gtk_application_window_new(app);

    gtk_window_set_title(GTK_WINDOW(window), "Matrix Wallpaper");
    gtk_window_set_default_size(GTK_WINDOW(window), 1200, 600);

    GtkWidget* drawingArea = gtk_drawing_area_new();

    auto* state = new MatrixState{
        .drawingArea = drawingArea,
        .lastUpdate = g_get_monotonic_time()
    };

    gtk_drawing_area_set_draw_func(
        GTK_DRAWING_AREA(drawingArea),
        draw,
        state,
        nullptr
    );

    gtk_window_set_child(GTK_WINDOW(window), drawingArea);

    state->timerId = g_timeout_add(10, update, state);

    g_signal_connect(
        window,
        "destroy",
        G_CALLBACK(+[](GtkWidget*, gpointer data) {
            auto* state = static_cast<MatrixState*>(data);

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
        "dev.matrix.wallpaper",
        G_APPLICATION_DEFAULT_FLAGS
    );

    g_signal_connect(
        app,
        "activate",
        G_CALLBACK(activate),
        nullptr
    );

    const int result = g_application_run(
        G_APPLICATION(app),
        argc,
        argv
    );

    g_object_unref(app);
    return result;
}