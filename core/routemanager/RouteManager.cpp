//
// Created by archdev on 2/24/26.
//
#include "RouteManager.h"
#include "../Core.h"
#include "../servermanager/ServerManager.h"
#include "../../utils/Utils.h"
#include "../../utils/logger/Logger.h"
bool RouteManager::MoveOrder(const std::string& oldPath, bool toComplete) {
    const std::string fromDir = toComplete ? "outgoing" : "incoming";
    const std::string toDir   = toComplete ? "incoming" : "outgoing";

    const std::string filename = std::filesystem::path(oldPath).filename().string();
    std::string newPath = oldPath;

    if (const size_t pos = newPath.find(fromDir); pos != std::string::npos) {
        newPath.replace(pos, fromDir.length(), toDir);
        try {
            std::filesystem::path p(newPath);
            std::filesystem::create_directories(p.parent_path());
            std::filesystem::rename(oldPath, newPath);

            // Using the new centralized logger
            Utils::logger().Log(toComplete ? LogLevel::SUCCESS : LogLevel::INFO,
                "Order " + std::string(toComplete ? "Completed: " : "Reopened: ") + filename);

            Core::pending_refresh = true;
            return true;
        } catch (const std::filesystem::filesystem_error& e) {
            Utils::logger().Log(LogLevel::ERR, "FS Move Error: " + std::string(e.what()));
        }
    }
    return false;
}
void RouteManager::AddRoute(const std::string& name) {
    if (name.empty()) return;

    if (std::ranges::find(routes, name) != routes.end()) {
        Utils::logger().Log(LogLevel::WARNING, "Attempted to add duplicate route: " + name);
        return;
    }

    routes.push_back(name);
    SyncFolders();
    SaveRoutes();
    RefreshAllLists();

    Utils::logger().Log(LogLevel::SUCCESS, "New route created: " + name);
}

void RouteManager::RemoveRoute(const int index) {
    // Standard safety: Don't delete the "All Routes" virtual entry at index 0
    if (index <= 0 || index >= routes.size()) return;

    std::string removedName = routes[index];

    try {
        // 1. Path definitions
        fs::path outgoingPath = fs::path("outgoing") / removedName;
        fs::path incomingPath = fs::path("incoming") / removedName;
        fs::path backupPath   = fs::path("backups/orders") / removedName;

        // 2. Perform the cleanup
        // We use remove_all to wipe the directory and everything inside (the orders)
        if (fs::exists(outgoingPath)) fs::remove_all(outgoingPath);
        if (fs::exists(incomingPath)) fs::remove_all(incomingPath);

        // This handles the backup cleanup we discussed earlier
        if (fs::exists(backupPath)) {
            fs::remove_all(backupPath);
            Utils::logger().Log(LogLevel::INFO, "Cleaned up order backups for: " + removedName);
        }

        // 3. Remove from memory and config
        routes.erase(routes.begin() + index);

        // Reset selection if we just deleted the route we were looking at
        if (selected_idx == index) selected_idx = 0;

        SaveRoutes();
        RefreshAllLists();

        Utils::logger().Log(LogLevel::SUCCESS, "Route and associated data removed: " + removedName);
    } catch (const fs::filesystem_error& e) {
        Utils::logger().Log(LogLevel::ERR, "Failed to fully delete route folders: " + std::string(e.what()));
    }
}
std::string RouteManager::GetCurrentRouteName() {
    return routes[selected_idx];
}
void RouteManager::SyncFolders() const {
    fs::create_directories("outgoing");
    fs::create_directories("incoming");
    for (const auto& r : routes) {
        if (r == "All Routes") continue;
        fs::create_directories("outgoing/" + r);
        fs::create_directories("incoming/" + r);
    }
}
void RouteManager::RefreshAllLists() {
    // 1. Build everything "offline" in new_storage
    std::map<std::string, RouteData> new_storage;
    for (const auto& r : routes) {
        new_storage[r] = RouteData();
    }
    // THE FIX: Capture new_storage by reference so the lambda fills the NEW map
    auto Scan = [&](const std::string& base_dir, bool is_outgoing) {
        if (!fs::exists(base_dir)) return;

        for (const auto& entry : fs::recursive_directory_iterator(base_dir)) {
            if (!entry.is_regular_file() || entry.path().extension() != ".json") continue;

            // 1. Add to "All Routes" in the NEW storage
            if (is_outgoing) new_storage["All Routes"].outgoing.push_back(entry.path());
            else             new_storage["All Routes"].incoming.push_back(entry.path());

            // Inside your Scan lambda
            for (const auto& r : routes) {
                if (r == "All Routes") continue;

                // Check if the immediate folder name matches the route name
                if (entry.path().parent_path().filename() == r) {
                    if (is_outgoing) new_storage[r].outgoing.push_back(entry.path());
                    else             new_storage[r].incoming.push_back(entry.path());
                }
            }
        }
    };

    Scan("outgoing", true);
    Scan("incoming", false);

    // 2. LOCK only to perform the swap
    {
        std::lock_guard<std::mutex> lock(route_mutex);
        // This instantly replaces the old data with the new data
        route_storage = std::move(new_storage);
    }
    // UI thread is now safe to read the updated route_storage
}

RouteData RouteManager::GetSelectedData() {
    std::lock_guard<std::mutex> lock(route_mutex);

    // Safety check for index
    int idx = (selected_idx < 0 || selected_idx >= (int)routes.size()) ? 0 : selected_idx;

    // This returns a COPY. It is safe to use in the UI
    // even if the original is refreshed/deleted later.
    return route_storage[routes[idx]];
}
// PRIVATE

void RouteManager::SaveRoutes() const {
    std::ofstream file("routes.cfg");
    for (size_t i = 1; i < routes.size(); ++i) file << routes[i] << "\n";
}
void RouteManager::LoadRoutes() {
    std::ifstream file("routes.cfg");

    // Reset to base state to prevent duplicates on reload
    routes.clear();
    routes.push_back("All Routes");

    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty()) {
            routes.push_back(line);
        }
    }

    // After routes are in memory, ensure folders exist on disk
    SyncFolders();
}