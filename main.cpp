#include <iostream>
#include <algorithm>
#include <gtk/gtk.h>

#include "MatrixRenderer.h"

struct MatrixState {
    GtkWidget* drawingArea = nullptr;
    guint timerId = 0;
    gint64 lastUpdate = 0;

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
    state->renderer.draw(cr, width, height);
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

    state->renderer.update(deltaTime, width, height);

    gtk_widget_queue_draw(state->drawingArea);

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

    state->timerId = g_timeout_add(33, update, state);

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