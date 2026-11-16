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

#pragma once

#pragma once

#include <unordered_map>
#include <gdk/gdk.h>
#include <gio/gio.h>

class MonitorManager {
public:
    explicit MonitorManager(GdkDisplay* display);
    ~MonitorManager();

    void initialize();


private:
    GdkDisplay* display_ = nullptr;
    GListModel* monitors_ = nullptr;
    gulong signalHandler_ = 0;
    std::unordered_map<GdkMonitor*, gulong> observedMonitors_;

    static bool isMonitorReady(GdkMonitor* monitor);
    void synchronizeMonitors();
    static void onMonitorsChanged(
        GListModel* model,
        guint position,
        guint removed,
        guint added,
        gpointer userData
    );
    static void onMonitorGeometryChanged(
        GObject* object,
        GParamSpec* pspec,
        gpointer userData
    );
    void observeMonitor(GdkMonitor* monitor);
    void unobserveMonitor(GdkMonitor* monitor);

};
