#include <iostream>
#include <gtk/gtk.h>


struct MatrixState {
    GtkWidget* drawingArea = nullptr;
    double y = 0.0;
    double speed = 120.0;
};

static void draw(
    GtkDrawingArea* area,
    cairo_t* cr,
    int width,
    int height,
    gpointer user_data
) {
    auto* state = static_cast<MatrixState*>(user_data);

    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_paint(cr);

    cairo_select_font_face(
        cr,
        "monospace",
        CAIRO_FONT_SLANT_NORMAL,
        CAIRO_FONT_WEIGHT_NORMAL
    );

    cairo_set_font_size(cr, 28.0);
    cairo_set_source_rgb(cr, 0.0, 1.0, 0.0);

    cairo_move_to(cr, 40.0, state->y);
    cairo_show_text(cr, "Wake up, Neo...");
}

static gboolean update(gpointer user_data) {
    auto* state = static_cast<MatrixState*>(user_data);

    state->y += state->speed / 30.0;

    const int height = gtk_widget_get_height(state->drawingArea);

    if (state->y > height + 30.0) {
        state->y = 0.0;
    }

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
        .y = 0.0,
        .speed = 120.0
    };

    gtk_drawing_area_set_draw_func(
        GTK_DRAWING_AREA(drawingArea),
        draw,
        state,
        nullptr
    );

    gtk_window_set_child(GTK_WINDOW(window), drawingArea);

    // Timer and State are part of the canvas.
    const guint timerId = g_timeout_add(
        33,
        update,
        state
    );

    g_signal_connect(
        window,
        "destroy",
        G_CALLBACK(+[](GtkWidget*, gpointer data) {
            auto* state = static_cast<MatrixState*>(data);
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

    g_signal_connect(app, "activate", G_CALLBACK(activate), nullptr);

    const int result = g_application_run(
        G_APPLICATION(app),
        argc,
        argv
    );

    g_object_unref(app);
    return result;
}