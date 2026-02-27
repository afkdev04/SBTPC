#include "CatalogManager.h"
#include "../../utils/Utils.h"
#include "../../utils/logger/Logger.h"
#include "../Core.h"
#include <fstream>
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <set>
#include <nlohmann/json.hpp>
using json = nlohmann::json;

void CatalogManager::MoveProduct(const std::string& productName, int targetIdx) {
    std::lock_guard<std::mutex> lock(catalog_mutex);

    // 1. Find the item
    const auto it = std::ranges::find_if(master_catalog,
                                   [&](const Product& p) { return p.name == productName; });
    if (it == master_catalog.end()) return;

    // 2. MOVE it out safely (Use std::move to avoid expensive copies)
    Product movingProduct = std::move(*it);
    master_catalog.erase(it);

    // 3. Clamp target and insert
    targetIdx = std::clamp(targetIdx, 0, static_cast<int>(master_catalog.size()));

    master_catalog.insert(master_catalog.begin() + targetIdx, std::move(movingProduct));

    // 4. Re-index and Save
    for (int i = 0; i < static_cast<int>(master_catalog.size()); i++) {
        master_catalog[i].item_index = i;
    }

    SaveInternal("catalog.json");
    Core::needs_ui_refresh = true;
    Utils::logger().Log(LogLevel::SUCCESS, "Moved product: " + productName);
}
void CatalogManager::DeleteProductByName(const std::string& productName) {
    if (productName.empty()) return;

    std::lock_guard<std::mutex> lock(catalog_mutex);

    auto it = std::find_if(master_catalog.begin(), master_catalog.end(),
        [&](const Product& p) { return p.name == productName; });

    if (it != master_catalog.end()) {
        std::string fullDisplayName = "[" + it->brand + "] " + it->name;

        master_catalog.erase(it);

        // Re-index remaining items
        for (int i = 0; i < static_cast<int>(master_catalog.size()); i++) {
            master_catalog[i].item_index = i;
        }

        SaveInternal("catalog.json");
        Core::needs_ui_refresh = true;

        Utils::logger().Log(LogLevel::INFO, "Deleted Product: " + fullDisplayName);
    }
}
void CatalogManager::DeleteBrand(const std::string& brand_name) {
    if (brand_name.empty()) return;

    std::lock_guard<std::mutex> lock(catalog_mutex);

    size_t initial_size = master_catalog.size();
    master_catalog.erase(std::remove_if(master_catalog.begin(), master_catalog.end(),
        [&](const Product& p) { return p.brand == brand_name; }), master_catalog.end());

    if (master_catalog.size() < initial_size) {
        InternalNormalize();
        SortInternal();
        RebuildInternal();
        SaveInternal("catalog.json");

        Core::needs_ui_refresh = true;
    }
}
void CatalogManager::RebuildInternal() {
    brand_names.clear();
    brand_names.push_back("ALL BRANDS");

    struct BrandSortItem { std::string name; int index; };
    std::vector<BrandSortItem> temp_list;
    std::set<std::string> seen;

    for (const auto& prod : master_catalog) {
        if (seen.find(prod.brand) == seen.end()) {
            temp_list.push_back({ prod.brand, prod.brand_index });
            seen.insert(prod.brand);
        }
    }

    std::sort(temp_list.begin(), temp_list.end(), [](const BrandSortItem& a, const BrandSortItem& b) {
        return a.index < b.index;
    });

    for (const auto& b : temp_list) brand_names.push_back(b.name);

    master_brands.clear();
    for (size_t i = 1; i < brand_names.size(); i++) {
        master_brands.push_back(brand_names[i]);
    }
}

// --- 1. THE PUBLIC GATEWAY (The one at line 112) ---
void CatalogManager::LoadCatalog(const std::string& filename) {
    // 1. Grab the lock. It stays locked until this function returns.
    std::lock_guard<std::mutex> lock(catalog_mutex);

    // 2. Call the worker to do the actual disk I/O and JSON parsing
    LoadInternal(filename);
    master_barcodes.clear();
    for (const auto& prod : master_catalog) {
        for (const auto& [promo_name, offer] : prod.offers) {
            if (!offer.barcode.empty()) {
                // Map the barcode to the Brand + Item name
                master_barcodes[offer.barcode] = { prod.brand, prod.name };
            }
        }
    }
    // 3. Log the success (since we are now thread-safe)
    Utils::logger().Log(LogLevel::SUCCESS, "Catalog Loaded: " + std::to_string(master_catalog.size()) + " items.");
}
// --- 2. THE INTERNAL WORKER (The actual logic) ---
void CatalogManager::LoadInternal(const std::string& filename) {
    if (!std::filesystem::exists(filename)) {
        Utils::logger().Log(LogLevel::WARNING, "Catalog file not found. Starting fresh.");
        return;
    }

    try {
        std::ifstream file(filename);
        nlohmann::json j;
        file >> j;

        // Clear existing data before loading new data
        master_catalog.clear();

        // Parse the JSON array into our Product structs
        for (auto& item : j) {
            Product p;
            p.brand = item.value("brand", "Unknown");
            p.name  = item.value("item", "Unnamed Item");
            p.brand_index = item.value("brandIndex", 0); // Load the saved brand order
            p.item_index  = item.value("itemIndex", 0);  // Load the saved product order
            // Pre-calculate lowercase versions for the optimized search logic
            p.brand_lower = p.brand;
            std::transform(p.brand_lower.begin(), p.brand_lower.end(), p.brand_lower.begin(), ::tolower);
            p.name_lower = p.name;
            std::transform(p.name_lower.begin(), p.name_lower.end(), p.name_lower.begin(), ::tolower);

            // Load the offers (standard, promo, etc.)
            for (auto& [key, val] : item["offers"].items()) {
                p.offers[key] = {
                    val.value("barcode", ""),
                    val.value("price", 0LL)
                };
            }
            master_catalog.push_back(p);
        }

        // 4. Rebuild the helper lists so the UI dropdowns work
        InternalNormalize();
        RebuildInternal();
        SortInternal();

    } catch (const std::exception& e) {
        Utils::logger().Log(LogLevel::ERR, "Failed to parse catalog: " + std::string(e.what()));
    }
}
void CatalogManager::RebuildBrandList() {
    std::lock_guard<std::mutex> lock(catalog_mutex);
    RebuildInternal();
}

// The "Button" version
void CatalogManager::NormalizeIndices() {
    std::lock_guard<std::mutex> lock(catalog_mutex);
    InternalNormalize();
    SortCatalog();
    SaveCatalog();
}

void CatalogManager::InternalNormalize() {
    // We use a map to store current unique brand names sorted by their EXISTING index
    // This preserves the user's intended order while stripping out gaps (e.g., 0, 5, 12 -> 0, 1, 2)
    std::map<int, std::string> orderMap;
    std::set<std::string> seen;

    for (const auto& p : master_catalog) {
        if (seen.find(p.brand) == seen.end()) {
            int idx = p.brand_index;
            // Handle collisions: if two brands have the same index, push the new one up
            while (orderMap.count(idx)) idx++;
            orderMap[idx] = p.brand;
            seen.insert(p.brand);
        }
    }

    // Now re-assign clean, sequential indices (0, 1, 2, 3...)
    int cleanIdx = 0;
    for (auto const& [currentIdx, name] : orderMap) {
        for (auto& p : master_catalog) {
            if (p.brand == name) {
                p.brand_index = cleanIdx;
            }
        }
        cleanIdx++;
    }
}
void CatalogManager::ReorderAndNormalize(const std::string& targetBrand, int newIdx) {
    if (targetBrand.empty()) return;

    std::lock_guard<std::mutex> lock(catalog_mutex);

    // 1. Find the current index of the brand we are moving
    int oldIdx = -1;
    for (const auto& p : master_catalog) {
        if (p.brand == targetBrand) {
            oldIdx = p.brand_index;
            break;
        }
    }

    if (oldIdx == -1 || oldIdx == newIdx) return;

    // 2. Shift all other brands
    for (auto& p : master_catalog) {
        if (p.brand == targetBrand) {
            p.brand_index = newIdx;
        } else {
            if (newIdx < oldIdx) { // Moving UP
                if (p.brand_index >= newIdx && p.brand_index < oldIdx) p.brand_index++;
            } else { // Moving DOWN
                if (p.brand_index > oldIdx && p.brand_index <= newIdx) p.brand_index--;
            }
        }
    }

    SortInternal();
    InternalNormalize(); // Ensure no gaps
    SaveInternal("catalog.json");
    Core::needs_ui_refresh = true;
}
void CatalogManager::SortCatalog() {
    std::lock_guard<std::mutex> lock(catalog_mutex);
    SortInternal();
}

void CatalogManager::SaveCatalog(const std::string& filename) {
    std::lock_guard<std::mutex> lock(catalog_mutex);
    SaveInternal(filename);
}

// Ensure this one is also present as it was missing in the linker logs
void CatalogManager::SortInternal() {
    std::sort(master_catalog.begin(), master_catalog.end(), [](const Product& a, const Product& b) {
        if (a.brand_index != b.brand_index)
            return a.brand_index < b.brand_index;
        return a.item_index < b.item_index;
    });
}
void CatalogManager::SaveInternal(const std::string& filename) {
    nlohmann::json root = nlohmann::json::array();

    for (const auto& prod : master_catalog) {
        nlohmann::json item;
        item["brand"] = prod.brand;
        item["item"] = prod.name;
        item["brandIndex"] = prod.brand_index;
        item["itemIndex"] = prod.item_index;

        // Save the nested offers map (Standard, Promo1, Promo2)
        for (const auto& [key, offer] : prod.offers) {
            item["offers"][key]["barcode"] = offer.barcode;
            item["offers"][key]["price"] = offer.price;
        }
        root.push_back(item);
    }

    std::ofstream file(filename);
    if (file.is_open()) {
        // Use dump(4) for pretty-printing, making the JSON human-readable
        file << root.dump(4);
    }
}
void CatalogManager::RenameBrand(const std::string& oldName, const std::string& newName) {
    std::lock_guard<std::mutex> lock(catalog_mutex);

    std::string newLower = newName;
    std::transform(newLower.begin(), newLower.end(), newLower.begin(), ::tolower);

    for (auto& p : master_catalog) {
        if (p.brand == oldName) {
            p.brand = newName;
            p.brand_lower = newLower;
        }
    }

    // Trigger UI refresh to update the combo boxes and main table
   Core::needs_ui_refresh = true;
}
void CatalogManager::RenameProduct(int realIdx, const std::string& newName) {
    if (newName.empty()) return;

    std::lock_guard<std::mutex> lock(catalog_mutex);

    if (realIdx >= 0 && realIdx < (int)master_catalog.size()) {
        master_catalog[realIdx].name = newName;

        // Update lowercase search string
        master_catalog[realIdx].name_lower = newName;
        std::transform(master_catalog[realIdx].name_lower.begin(),
                       master_catalog[realIdx].name_lower.end(),
                       master_catalog[realIdx].name_lower.begin(), ::tolower);

        SaveInternal("catalog.json"); // No-lock save
        Core::needs_ui_refresh.store(true);
    }
}
std::vector<CatalogManager::BrandInfo> CatalogManager::GetUniqueBrandsSorted() {
    std::lock_guard<std::mutex> lock(catalog_mutex);

    std::vector<BrandInfo> result;
    std::set<std::string> seen;

    for (const auto& p : master_catalog) {
        // Only process each brand once
        if (seen.find(p.brand) == seen.end()) {
            BrandInfo info;
            info.name = p.brand;
            info.name_lower = p.brand_lower; // Use the cached lower-case version
            info.index = p.brand_index;

            result.push_back(info);
            seen.insert(p.brand);
        }
    }

    // Sort by index so the UI list matches your desired order
    std::sort(result.begin(), result.end(), [](const BrandInfo& a, const BrandInfo& b) {
        return a.index < b.index;
    });

    return result;
}
std::string CatalogManager::GetBrandNameFromIndex(int index) {
    std::lock_guard<std::mutex> lock(catalog_mutex);
    for (const auto& p : master_catalog) {
        if (p.brand_index == index) {
            return p.brand;
        }
    }
    return "";
}
void CatalogManager::AddProduct(const std::string& name, const std::string& brand, double price) {
    Product p;
    p.brand = brand;
    p.name = name;

    {
        std::lock_guard<std::mutex> lock(catalog_mutex);

        // Find existing index or calculate new one
        int target_brand_idx = -1;
        int max_brand_idx = -1;

        for(const auto& item : master_catalog) {
            if(item.brand == brand) target_brand_idx = item.brand_index;
            if(item.brand_index > max_brand_idx) max_brand_idx = item.brand_index;
        }

        p.brand_index = (target_brand_idx != -1) ? target_brand_idx : (max_brand_idx + 1);

        // Convert to cents
        long long cents = static_cast<long long>(std::round(price * 100));
        p.offers["standard"] = { "", cents };

        // Pre-calculate lower case for Core::split_fuzzy_match
        p.brand_lower = p.brand;
        std::transform(p.brand_lower.begin(), p.brand_lower.end(), p.brand_lower.begin(), ::tolower);
        p.name_lower = p.name;
        std::transform(p.name_lower.begin(), p.name_lower.end(), p.name_lower.begin(), ::tolower);

        master_catalog.push_back(p);

        // 3. Keep internal structures in sync immediately
        SortInternal();
        InternalNormalize();
        RebuildInternal();
        SaveInternal("catalog.json");
    }

    Core::needs_ui_refresh = true;
    Utils::logger().Log(LogLevel::SUCCESS, "Added Product: [" + brand + "] " + name);
}

void CatalogManager::AddBrand(const std::string& brandName) {
    if (brandName.empty()) return;

    std::lock_guard<std::mutex> lock(catalog_mutex);

    // Check if it already exists in the master list to avoid duplicates
    auto it = std::find(master_brands.begin(), master_brands.end(), brandName);
    if (it == master_brands.end()) {
        master_brands.push_back(brandName);

        // Since we added a brand, we should refresh the lists
        // and save so the UI dropdowns see it immediately.
        RebuildInternal();
        SaveInternal("catalog.json");
    }
}
void CatalogManager::UpdateBarcode(const std::string& brand, const std::string& item,
                                   const std::string& barcode, const std::string& promoKey) {
    std::lock_guard<std::mutex> lock(catalog_mutex);

    bool found = false;
    for (auto& prod : master_catalog) {
        if (prod.brand == brand && prod.name == item) {
            auto it = prod.offers.find(promoKey);
            if (it != prod.offers.end()) {
                it->second.barcode = barcode;
                found = true;

                // Create the combined name: [Brand] ItemName
                std::string fullDisplayName = "[" + prod.brand + "] " + prod.name;

                SaveInternal("catalog.json");
                Core::needs_ui_refresh.store(true);

                std::string action = barcode.empty() ? "CLEARED" : barcode;
                Utils::logger().Log(barcode.empty() ? LogLevel::INFO : LogLevel::SUCCESS,
                    "Barcode Update: " + fullDisplayName + " -> " + action);

                return;
            }
        }
    }

    if (!found) {
        Utils::logger().Log(LogLevel::WARNING, "Barcode Update Failed: " + brand + " " + item + " not found.");
    }
}
void CatalogManager::CreateBackup() {
    try {
        std::string backupFolder = "backups";
        if (!std::filesystem::exists(backupFolder)) {
            std::filesystem::create_directories(backupFolder);
        }

        // Generate a timestamp: YYYY-MM-DD_HH-MM-SS
        auto now = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d_%H-%M-%S");

        std::string backupName = "catalog_" + ss.str() + ".json";
        std::filesystem::path backupPath = std::filesystem::path(backupFolder) / backupName;

        // Copy current catalog to the backup path
        if (std::filesystem::exists("catalog.json")) {
            copy_file("catalog.json", backupPath, std::filesystem::copy_options::overwrite_existing);
            Utils::logger().Log(LogLevel::INFO, "System Snapshot Created: " + backupName);
        }

        // Optional: Keep only the last 20 backups to save space
        //CleanupOldBackups(20);

    } catch (const std::exception& e) {
        Utils::logger().Log(LogLevel::ERR, "Backup Failed: " + std::string(e.what()));
    }
}
void CatalogManager::UpdatePrice(int productIdx, const std::string& offerKey, double newPrice) {
    std::lock_guard<std::mutex> lock(catalog_mutex);
    if (productIdx < 0 || productIdx >= master_catalog.size()) return;

    long long cents = static_cast<long long>(std::round(newPrice * 100));
    master_catalog[productIdx].offers[offerKey].price = cents;

    SaveInternal("catalog.json"); // No-lock version
}
void CatalogManager::MassUpdatePrice(const std::string& brandFilter, const std::string& promoKey, double newPrice) {
    std::lock_guard<std::mutex> lock(catalog_mutex);

    long long cents = static_cast<long long>(std::round(newPrice * 100));
    int updateCount = 0;

    for (auto& prod : master_catalog) {
        if (brandFilter.empty() || prod.brand == brandFilter) {
            auto it = prod.offers.find(promoKey);
            if (it != prod.offers.end()) {
                it->second.price = cents;
                updateCount++;
            }
        }
    }

    if (updateCount > 0) {
        SaveInternal("catalog.json");
        Core::needs_ui_refresh.store(true);

        // Log showing the scope of the change
        std::string scope = brandFilter.empty() ? "ALL BRANDS" : "[" + brandFilter + "]";
        Utils::logger().Log(LogLevel::SUCCESS,
            "Mass Price Edit: " + std::to_string(updateCount) + " items updated in " + scope);
    }
}

