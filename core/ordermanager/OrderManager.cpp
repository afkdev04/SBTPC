#include "OrderManager.h"
#include "../Core.h"
#include "imgui.h"
#include <fstream>
#include <filesystem>
#include <iostream>
#include <nlohmann/json.hpp>
#include <chrono>
#include "../routemanager/RouteManager.h"
#include "../../utils/Utils.h"
#include "../../utils/logger/Logger.h"
using json = nlohmann::json;
namespace fs = std::filesystem;
static std::string current_order_filepath = "";
// 1. AddItem - check that the string types and 'const&' match the header
void OrderManager::AddItem(const std::string& barcode, const std::string& name, long long price, int bIdx, int iIdx) {
    OrderItem newItem;
    newItem.barcode = barcode;
    newItem.name = name;
    newItem.price = price;
    newItem.brand_index = bIdx;
    newItem.item_index = iIdx;
    newItem.quantity = 1;

    // Explicitly initialize progress as "New"
    newItem.got = 0;
    newItem.complete = false;
    newItem.disabled = false;

    current_items.push_back(newItem);
    Utils::logger().Log(LogLevel::SUCCESS, "Added to Order: " + name);
}
void OrderManager::RemoveItem(int index) {
    if (index >= 0 && index < (int)current_items.size()) {
        std::string removedName = current_items[index].name;
        current_items.erase(current_items.begin() + index);

        Utils::logger().Log(LogLevel::INFO, "Removed from Order: " + removedName);
    }
}

void OrderManager::ClearOrder() {
    size_t count = current_items.size();
    current_items.clear();
    current_order_filepath = "";
    memset(order_name, 0, sizeof(order_name));

    if (count > 0) {
        Utils::logger().Log(LogLevel::WARNING, "Order Cleared: " + std::to_string(count) + " items removed.");
    }
}
void OrderManager::RenderOrderUI() {
    ImGuiIO& io = ImGui::GetIO();

}
long long OrderManager::GetTotal() const {
    long long total = 0;
    for (const auto& item : current_items) {
        total += (item.price * item.quantity);
    }
    return total;
}
void OrderManager::LoadOrderFromFile(const std::string& filepath) {
    if (!fs::exists(filepath)) return;

    try {
        std::ifstream file(filepath);
        json j;
        file >> j;

        ClearOrder();
        current_order_filepath = filepath;

        // Parse filename for UI buffer
        std::string filename = fs::path(filepath).stem().string();
        size_t first_underscore = filename.find('_');
        if (first_underscore != std::string::npos) {
            std::string name_part = filename.substr(first_underscore + 1);
            std::replace(name_part.begin(), name_part.end(), '_', ' ');
            strncpy(order_name, name_part.c_str(), sizeof(order_name) - 1);
        }

        if (j.is_array()) {
            for (const auto& item_json : j) {
                OrderItem item;
                // STRICT SNAKE CASE KEYS
                item.barcode     = item_json.value("barcode", "");
                item.name        = item_json.value("name", "Unknown Item");
                item.quantity    = item_json.value("requested_qty", 1);
                item.price       = item_json.value("price", 0LL);
                item.brand_index = item_json.value("brand_idx", -1);
                item.item_index  = item_json.value("item_idx", -1);

                // Checklist state
                item.got         = item_json.value("got_qty", 0);
                item.complete    = item_json.value("is_complete", false);
                item.disabled    = item_json.value("is_disabled", false);

                current_items.push_back(item);
            }
            Utils::logger().Log(LogLevel::INFO, "Order Loaded: " + filename);
        }
    } catch (const std::exception& e) {
        Utils::logger().Log(LogLevel::ERR, "JSON Parse Error: " + std::string(e.what()));
    }
}
void OrderManager::SaveOrderToPending() {
    if (current_items.empty()) return;

    // 1. Snapshot the disk state to protect Handheld progress
    nlohmann::json disk_json;
    if (!current_order_filepath.empty() && fs::exists(current_order_filepath)) {
        std::ifstream f_in(current_order_filepath);
        if (f_in.is_open()) f_in >> disk_json;
    }

    nlohmann::json final_array = nlohmann::json::array();

    for (const auto& item : current_items) {
        nlohmann::json j;
        j["barcode"]       = item.barcode;
        j["name"]          = item.name;
        j["price"]         = item.price;
        j["requested_qty"] = item.quantity;
        j["brand_idx"]     = item.brand_index;
        j["item_idx"]      = item.item_index;

        // MERGE LOGIC: Default to 0, but take disk values if barcode matches
        int  p_got  = 0;
        bool p_comp = false;
        bool p_dis  = false;

        for (const auto& d_item : disk_json) {
            if (d_item.value("barcode", "") == item.barcode) {
                p_got  = d_item.value("got_qty", 0);
                p_comp = d_item.value("is_complete", false);
                p_dis  = d_item.value("is_disabled", false);
                break;
            }
        }

        j["got_qty"]     = p_got;
        j["is_complete"] = p_comp;
        j["is_disabled"] = p_dis;

        final_array.push_back(j);
    }
    fs::path finalPath;
    // Check if we are overwriting an existing file
    if (!current_order_filepath.empty()) {
        finalPath = current_order_filepath;
    } else {
        // Create new filename with timestamp
        std::string safe_name = (strlen(order_name) > 0) ? order_name : "Unnamed_Order";
        std::replace(safe_name.begin(), safe_name.end(), ' ', '_');

        fs::path dirPath = fs::path("outgoing") / Core::route_manager().GetCurrentRouteName();
        auto now = std::chrono::system_clock::now();
        auto ts = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
        finalPath = dirPath / (std::to_string(ts) + "_" + safe_name + ".json");
    }
    std::ofstream f_out(finalPath);
    if (f_out.is_open()) {
        f_out << final_array.dump(4);
        f_out.close();

        // Final cleanup
        current_order_filepath = "";
        memset(order_name, 0, sizeof(order_name));
        ClearOrder();
        Core::route_manager().RefreshAllLists();
    }
}