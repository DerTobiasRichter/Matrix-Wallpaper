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
#include <csignal>

#include <memory>
#include <vector>
#include <string>

#include "Debug.h"
#include "JournalSource.h"
#include "MatrixRenderer.h"
#include "MonitorManager.h"

namespace {
    constexpr guint frameIntervalMs = 66; // Approximately 15 FPS while active.
    constexpr double journalImportInterval = 0.49; // At most ~2 new streams / second.

    // RUSAGE_SELF covers the whole application, including the journal worker.
    double processCpuSeconds() {
        rusage usage{};
        if (::getrusage(RUSAGE_SELF, &usage) != 0) {
            return 0.0;
        }
        const auto asSeconds = [](const timeval &value) {
            return static_cast<double>(value.tv_sec) + value.tv_usec / 1'000'000.0;
        };
        return asSeconds(usage.ru_utime) + asSeconds(usage.ru_stime);
    }
} // namespace

struct MonitorState {
    GdkMonitor* monitor = nullptr;

    GtkWidget* window = nullptr;
    GtkWidget* drawingArea = nullptr;

    MatrixRenderer renderer;

    // Cairo callback metrics
    gint64 reportStart = 0;
    gint64 totalDrawTime = 0;
    gint64 maxDrawTime = 0;
    std::uint64_t frameCount = 0;
};

struct ApplicationState {
    // Debug.h Log State
    bool debug = false;

    // Shared event source
    JournalSource journalSource;
    // Empty = all monitors
    std::string selectedMonitor;

    // Application owns all monitor states
    std::vector<std::unique_ptr<MonitorState>> monitors;

    // Shared event loop
    guint timerId = 0;
    guint journalWatchId = 0;

    gint64 lastUpdate = 0;
    double journalAccumulator = journalImportInterval;

    // Global performance metrics
    gint64 updateReportStart = 0;
    std::uint64_t updateCount = 0;
    double cpuReportStart = 0.0;

    //Monitor state change manager
    std::unique_ptr<MonitorManager> monitorManager;

    //Paranoia Mode Controls
    guint paranoiaSignalId = 0;
    bool paranoiaMode = false;
};

static void draw(
    GtkDrawingArea * /*area*/,
    cairo_t *cr,
    int width,
    int height,
    gpointer userData
) {
    auto *state = static_cast<MonitorState *>(userData);
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

        Debug::log(std::fixed, std::setprecision(2),
            "[Render] ", width, "x", height,
            " | FPS: ", state->frameCount / elapsed,
            " | Avg: ", averageMs, " ms",
            " | Max: ", maximumMs," ms");

        state->reportStart = end;
        state->totalDrawTime = 0;
        state->maxDrawTime = 0;
        state->frameCount = 0;
    }
}

static gboolean update(gpointer userData) {
    auto *state = static_cast<ApplicationState *>(userData);
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

    if (state->journalAccumulator >= journalImportInterval && state->journalSource.hasPending()) {

        auto messages = state->journalSource.drain(1);

        if (!messages.empty()) {
            for (const auto& message : messages) {
                for (auto& monitor : state->monitors) {
                    monitor->renderer.append(message);
                }
            }

            state->journalAccumulator -= journalImportInterval;
        }
    }

    bool anyActive = false;
    bool anyPending = false;
    std::size_t totalStreams = 0;

    // Update each monitor independently.
    for (auto& monitor : state->monitors) {
        const int width = gtk_widget_get_width(monitor->drawingArea);
        const int height = gtk_widget_get_height(monitor->drawingArea);

        const bool wasActive = monitor->renderer.hasActiveStreams();
        monitor->renderer.update(deltaTime, width, height);
        const bool isActive = monitor->renderer.hasActiveStreams();

        if (wasActive || isActive) {
            gtk_widget_queue_draw(monitor->drawingArea);
        }

        anyActive |= isActive;
        anyPending |= monitor->renderer.hasPendingMessages();
        totalStreams += monitor->renderer.activeStreamCount();
    }

    ++state->updateCount;
    const double elapsed = (now - state->updateReportStart) / 1'000'000.0;
    if (elapsed >= 5.0) {
        const double currentCpuSeconds = processCpuSeconds();
        const double cpuPercent =
            (currentCpuSeconds - state->cpuReportStart) / elapsed * 100.0;

        Debug::log(
            std::fixed, std::setprecision(2),
            "[Update] ", state->updateCount / elapsed, " Hz",
            " | Streams: ", totalStreams,
            " | CPU: ", cpuPercent, "% of one core"
        );

        state->updateCount = 0;
        state->updateReportStart = now;
        state->cpuReportStart = currentCpuSeconds;
    }

    // All monitors idle and no queued work?
    if (!anyActive && !anyPending && !state->journalSource.hasPending()) {
        state->timerId = 0;
        Debug::log("[Animation] idle");
        return G_SOURCE_REMOVE;
    }

    return G_SOURCE_CONTINUE;
}

static void startAnimation(ApplicationState *state) {
    if (state->timerId != 0 || state->monitors.empty()) {
        return;
    }

    const gint64 now = g_get_monotonic_time();
    state->lastUpdate = now;
    state->journalAccumulator = journalImportInterval;
    state->updateReportStart = now;
    state->updateCount = 0;
    state->cpuReportStart = processCpuSeconds();

    for (auto& monitor : state->monitors) {
        monitor->reportStart = 0;
        monitor->totalDrawTime = 0;
        monitor->maxDrawTime = 0;
        monitor->frameCount = 0;
    }

    state->timerId = g_timeout_add_full(
        G_PRIORITY_DEFAULT,
        frameIntervalMs,
        update,
        state,
        nullptr
    );

    Debug::log("[Animation] active (15 FPS cap)");
}

static gboolean onJournalReady(
    gint fd,
    GIOCondition condition,
    gpointer userData
) {
    auto *state = static_cast<ApplicationState *>(userData);
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

static void createWallpaperWindow(GtkApplication* app, GdkMonitor* monitor, ApplicationState* application)
{
    GtkWidget* window = gtk_application_window_new(app);

    gtk_window_set_title(GTK_WINDOW(window), "Matrix Wallpaper");
    gtk_window_set_decorated(GTK_WINDOW(window), FALSE);

    // Initialize Layer Shell
    gtk_layer_init_for_window(GTK_WINDOW(window));
    gtk_layer_set_monitor(GTK_WINDOW(window), monitor);

    gtk_layer_set_namespace(
        GTK_WINDOW(window), "matrix-wallpaper"
    );

    gtk_layer_set_layer(
        GTK_WINDOW(window),
        GTK_LAYER_SHELL_LAYER_BACKGROUND
    );

    // Fill the selected monitor
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

    gtk_layer_set_exclusive_zone(GTK_WINDOW(window), -1);

    gtk_layer_set_keyboard_mode(
        GTK_WINDOW(window),
        GTK_LAYER_SHELL_KEYBOARD_MODE_NONE
    );

    // Per-window state
    GtkWidget* drawingArea = gtk_drawing_area_new();

    auto monitorState = std::make_unique<MonitorState>();

    monitorState->window = window;
    monitorState->drawingArea = drawingArea;
    monitorState->monitor = monitor;

    MonitorState* monitorPtr = monitorState.get();

    application->monitors.push_back(std::move(monitorState));

    gtk_drawing_area_set_draw_func(
        GTK_DRAWING_AREA(drawingArea),
        draw,
        monitorPtr,
        nullptr
    );

    gtk_window_set_child(GTK_WINDOW(window), drawingArea);

    g_signal_connect(
        window,
        "destroy",
        G_CALLBACK(+[](GtkWidget* window, gpointer userData) {
            auto* appState = static_cast<ApplicationState*>(userData);

            std::erase_if(
                appState->monitors,
                [window](const auto& monitor) {
                    return monitor->window == window;
                }
            );
        }),
        application
    );

    gtk_window_present(GTK_WINDOW(window));
}

static gint handleLocalOptions(
    GApplication* /*app*/,
    GVariantDict* options,
    gpointer userData
) {
    auto* state = static_cast<ApplicationState*>(userData);

    state->debug = g_variant_dict_contains(options, "debug");
    Debug::enabled = state->debug;

    const char* connector = nullptr;

    if (g_variant_dict_lookup(options, "monitor", "&s", &connector)) {
        state->selectedMonitor = connector;
    }

    return -1; // Continue normal application startup
}

static gboolean onParanoiaSignal(gpointer userData) {
    auto* state = static_cast<ApplicationState*>(userData);

    state->paranoiaMode = !state->paranoiaMode;
    state->journalSource.setParanoiaMode(state->paranoiaMode);

    Debug::log("[Paranoia] ",
               state->paranoiaMode ? "enabled" : "disabled");

    return G_SOURCE_CONTINUE;
}

static void activate(GtkApplication* app, gpointer userData) {
    auto* application = static_cast<ApplicationState*>(userData);

    static bool applicationHeld = false;

    if (!applicationHeld) {
        g_application_hold(G_APPLICATION(app));
        applicationHeld = true;
    }

    if (application->paranoiaSignalId == 0) {
        application->paranoiaSignalId = g_unix_signal_add(
            SIGUSR1,
            onParanoiaSignal,
            application
        );
    }

    if (!application->monitors.empty()) {
        return;
    }

    GdkDisplay* display = gdk_display_get_default();
    if (!application->monitorManager) {
        application->monitorManager = std::make_unique<MonitorManager>(display);

        application->monitorManager->setMonitorRemovedCallback([application](GdkMonitor* removedMonitor)
        {
            auto it = std::find_if(
                application->monitors.begin(),
                application->monitors.end(),
                [removedMonitor](const auto& state) {
                    return state->monitor == removedMonitor;
                }
            );

            if (it == application->monitors.end()) {
                return;
            }

            GtkWidget* window = (*it)->window;

            Debug::log("[Monitor] Removing wallpaper");

            gtk_window_destroy(GTK_WINDOW(window));
        });

        application->monitorManager->setMonitorReadyCallback([app, application](GdkMonitor* monitor) {

        const char* connector = gdk_monitor_get_connector(monitor);

        if (!application->selectedMonitor.empty() &&
            (!connector || application->selectedMonitor != connector)) {
            return;
        }

        const bool alreadyExists = std::any_of(
            application->monitors.begin(),
            application->monitors.end(),
            [monitor](const auto& state) {
                return state->monitor == monitor;
            }
        );

        if (alreadyExists) {
            return;
        }

        Debug::log("[Monitor] Creating wallpaper on ", (connector ? connector : "unknown"));

        createWallpaperWindow(app, monitor, application);
    });

        application->monitorManager->initialize();
    }

    // One eventfd watch per application.
    application->journalWatchId = g_unix_fd_add_full(
        G_PRIORITY_DEFAULT,
        application->journalSource.wakeFd(),
        static_cast<GIOCondition>(G_IO_IN | G_IO_ERR | G_IO_HUP),
        onJournalReady,
        application,
        nullptr
    );

    if (application->journalSource.hasPending()) {
        startAnimation(application);
    }
}


int main(int argc, char **argv)
{
    GtkApplication *app = gtk_application_new(
        "dev.matrix.wallpaper", G_APPLICATION_DEFAULT_FLAGS
    );

    ApplicationState applicationState;

    g_application_add_main_option(
        G_APPLICATION(app),
        "monitor",
        '\0',
        G_OPTION_FLAG_NONE,
        G_OPTION_ARG_STRING,
        "Display connector (default: all monitors)",
        "CONNECTOR"
    );
    g_application_add_main_option(
        G_APPLICATION(app),
        "debug",
        'd',
        G_OPTION_FLAG_NONE,
        G_OPTION_ARG_NONE,
        "Enable diagnostic output",
        nullptr
    );

    g_signal_connect(
        app,
        "handle-local-options",
        G_CALLBACK(handleLocalOptions),
        &applicationState
    );

    g_signal_connect(app, "activate", G_CALLBACK(activate), &applicationState);

    const int result = g_application_run(G_APPLICATION(app), argc, argv);

    if (applicationState.paranoiaSignalId != 0) {
        g_source_remove(applicationState.paranoiaSignalId);
    }

    if (applicationState.timerId != 0) {
        g_source_remove(applicationState.timerId);
    }

    if (applicationState.journalWatchId != 0) {
        g_source_remove(applicationState.journalWatchId);
    }

    applicationState.monitorManager.reset();
    g_object_unref(app);
    return result;
}