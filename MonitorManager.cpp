/*
 * SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Tobias Richter
 * GitHub: https://github.com/DerTobiasRichter
 *
 * Created on: 2026-11-16
 * Last Modified: 2026-11-16
 *
 *
 */

//
// Created by tobi on 11/16/26.
//

#include "MonitorManager.h"

#include <iostream>
#include <vector>

#include "Debug.h"

MonitorManager::MonitorManager(GdkDisplay* display)
    : display_(display) {
}

MonitorManager::~MonitorManager() {

    onMonitorReady_ = nullptr;
    onMonitorRemoved_ = nullptr;

    if (signalHandler_ != 0) {
        g_signal_handler_disconnect(monitors_, signalHandler_);
    }

    while (!observedMonitors_.empty()) {
        unobserveMonitor(observedMonitors_.begin()->first);
    }

    if (monitors_ != nullptr) {
        g_object_unref(monitors_);
    }
}

void MonitorManager::initialize() {
    if (monitors_ != nullptr) {
        return;
    }

    monitors_ = gdk_display_get_monitors(display_);

    //  keep the reference on the list of monitors
    g_object_ref(monitors_);

    signalHandler_ = g_signal_connect(
        monitors_,
        "items-changed",
        G_CALLBACK(MonitorManager::onMonitorsChanged),
        this
    );

    Debug::log("[MonitorManager] Monitoring initialized");

    synchronizeMonitors();
}

void MonitorManager::onMonitorsChanged(
    GListModel* model,
    guint position,
    guint removed,
    guint added,
    gpointer userData
) {

    auto* self = static_cast<MonitorManager*>(userData);
    self->synchronizeMonitors();

    Debug::log(
        "[MonitorManager] Monitors changed",
        " | position: ", position,
        " | removed: ", removed,
        " | added: ", added);
}

void MonitorManager::onMonitorGeometryChanged(GObject *object, GParamSpec *pspec, gpointer userData) {

    auto* monitor = GDK_MONITOR(object);

    GdkRectangle geometry{};
    gdk_monitor_get_geometry(monitor, &geometry);

    const char* connector = gdk_monitor_get_connector(monitor);

    auto* self = static_cast<MonitorManager*>(userData);
    self->updateMonitorState(monitor);

    Debug::log(
        "[MonitorManager] Geometry changed: ", (connector ? connector : "unknown"),
        " | ", geometry.width, "x", geometry.height,
        " @ ", geometry.x, ",", geometry.y,
        " | ready: ", std::boolalpha, isMonitorReady(monitor));
}

void MonitorManager::observeMonitor(GdkMonitor* monitor) {
    Debug::log("[MonitorManager] ObserveMonitor() called");
    Debug::log("[MonitorManager] Observing: ",
    (gdk_monitor_get_connector(monitor)? gdk_monitor_get_connector(monitor): "unknown"));

    if (observedMonitors_.contains(monitor)) {
        return;
    }

    g_object_ref(monitor);

    const gulong handlerId = g_signal_connect(
        monitor,
        "notify::geometry",
        G_CALLBACK(MonitorManager::onMonitorGeometryChanged),
        this
    );

    observedMonitors_.emplace(monitor, handlerId);
    updateMonitorState(monitor);
}

void MonitorManager::unobserveMonitor(GdkMonitor *monitor) {

    Debug::log("[MonitorManager] unobserveMonitor() called");
    Debug::log("[MonitorManager] Unobserving: ",
        (gdk_monitor_get_connector(monitor)? gdk_monitor_get_connector(monitor): "unknown"));

    const auto it = observedMonitors_.find(monitor);

    if (it == observedMonitors_.end()) {
        return;
    }

    g_signal_handler_disconnect(monitor, it->second);
    observedMonitors_.erase(it);

    if (readyMonitors_.erase(monitor) > 0 && onMonitorRemoved_) {
        onMonitorRemoved_(monitor);
    }

    g_object_unref(monitor);
}


void MonitorManager::synchronizeMonitors(){
    std::unordered_set<GdkMonitor*> currentMonitors;

    const guint count = g_list_model_get_n_items(monitors_);

    for (guint i = 0; i < count; ++i) {
        auto* monitor = GDK_MONITOR(
            g_list_model_get_item(monitors_, i)
        );

        currentMonitors.insert(monitor);
        observeMonitor(monitor);


        g_object_unref(monitor);
    }

    // Validate disconnected monitor list
    std::vector<GdkMonitor*> removedMonitors;

    for (const auto& [monitor, handlerId] : observedMonitors_) {
        if (!currentMonitors.contains(monitor)) {
            removedMonitors.push_back(monitor);
        }
    }

    for (auto* monitor : removedMonitors) {
        unobserveMonitor(monitor);
    }
}


bool MonitorManager::isMonitorReady(GdkMonitor* monitor) {
    if (!gdk_monitor_is_valid(monitor)) {
        return false;
    }

    GdkRectangle geometry{};
    gdk_monitor_get_geometry(monitor, &geometry);

    return geometry.width > 0 && geometry.height > 0;
}

void MonitorManager::setMonitorReadyCallback(MonitorCallback callback) {
    onMonitorReady_ = std::move(callback);
}

void MonitorManager::setMonitorRemovedCallback(MonitorCallback callback) {
    onMonitorRemoved_ = std::move(callback);
}

void MonitorManager::updateMonitorState(GdkMonitor* monitor) {

    const bool ready = isMonitorReady(monitor);
    const bool wasReady = readyMonitors_.contains(monitor);

    const char* connector = gdk_monitor_get_connector(monitor);

    Debug::log(
        "[MonitorManager] State: ",(connector ? connector : "unknown"),
        " | ready=", ready, " | wasReady=", wasReady);

    if (ready && !wasReady) {
        Debug::log("[MonitorManager] Inserting because ready=true");

        readyMonitors_.insert(monitor);

        if (onMonitorReady_) {
            onMonitorReady_(monitor);
        }
    }
    else if (!ready && wasReady) {
       Debug::log("[MonitorManager] Removing because ready=false");

        readyMonitors_.erase(monitor);

        if (onMonitorRemoved_) {
            onMonitorRemoved_(monitor);
        }
    }
}