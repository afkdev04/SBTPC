#ifndef SBTPC_ROUTEMANAGER_H
#define SBTPC_ROUTEMANAGER_H

#pragma once
#include <vector>
#include <string>
#include <filesystem>
#include <map>
#include <mutex>
#include "../../utils/Structs.h"

namespace fs = std::filesystem;

class RouteManager {
public:
    std::vector<std::string> routes = { "All Routes" };
    int selected_idx = 0;

    std::map<std::string, RouteData> route_storage;
    std::mutex route_mutex;

    RouteManager() = default;

    void AddRoute(const std::string& name);
    void RemoveRoute(int index);
    std::string GetCurrentRouteName();
    void SyncFolders() const;
    void RefreshAllLists();

    // Explicit Load/Save
    void LoadRoutes();
    void SaveRoutes() const;

    RouteData GetSelectedData();
    static bool MoveOrder(const std::string& oldPath, bool toComplete);
};

#endif //SBTPC_ROUTEMANAGER_H