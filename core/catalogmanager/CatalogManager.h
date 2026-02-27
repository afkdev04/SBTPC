#ifndef SBTPC_CATALOGMANAGER_H
#define SBTPC_CATALOGMANAGER_H


#pragma once
#include <filesystem>
#include <mutex>
#include <string>
#include <vector>
#include "../../utils/Structs.h"

class CatalogManager {
public:
    struct BrandInfo {
        std::string name;
        std::string name_lower;
        int index;
    };

    CatalogManager() = default;

    // --- PUBLIC API (Handles Mutex Locking) ---
    void LoadCatalog(const std::string& filename = "catalog.json");
    void SaveCatalog(const std::string& filename = "catalog.json");
    void SortCatalog();
    void CreateBackup();
    void RebuildBrandList();
    void NormalizeIndices();
    void AddBrand(const std::string& brandName);
    void AddProduct(const std::string& name, const std::string& brand, double price);
    void UpdatePrice(int productIdx, const std::string& offerKey, double newPrice);
    void UpdateBarcode(const std::string& brand, const std::string& item, const std::string& barcode, const std::string& promoKey = "standard");
    void MassUpdatePrice(const std::string& brandFilter, const std::string& promoKey, double newPrice);
    // New UI Helpers
    std::vector<BrandInfo> GetUniqueBrandsSorted();
    std::string GetBrandNameFromIndex(int index);
    void RenameBrand(const std::string& oldName, const std::string& newName);
    void RenameProduct(int realIdx, const std::string& newName);
    void MoveProduct(const std::string& productName, int targetIdx);
    void DeleteBrand(const std::string& brand_name);
    void ReorderAndNormalize(const std::string& targetBrand, int newIdx);

    void DeleteProductByName(const std::string& productName);

    const std::vector<Product>& GetCatalog() const { return master_catalog; }
    std::mutex& GetMutex() const { return catalog_mutex; }
    const std::vector<std::string>& GetBrandNames() const { return brand_names; }
    const std::vector<std::string>& GetMasterBrands() const { return master_brands; }
    int GetSelectedBrandIdx() const { return selected_brand_idx; }
    void SetSelectedBrandIdx(int selectedBrandIdx) { selected_brand_idx = selectedBrandIdx; }
    struct BarcodeMapping {
        std::string brand;
        std::string item;
    };

    // Getter for the UI or Server
    const std::map<std::string, BarcodeMapping>& GetBarcodeMap() const { return master_barcodes; }

private:
    std::map<std::string, BarcodeMapping> master_barcodes;

    // --- INTERNAL WORKERS (The "No-Lock" versions - Call these when Mutex is already held) ---
    void LoadInternal(const std::string& filename);
    void SaveInternal(const std::string& filename);
    void SortInternal();
    void RebuildInternal();
    void InternalNormalize();

    int selected_brand_idx = 0;
    std::vector<Product> master_catalog;
    std::vector<std::string> master_brands;
    std::vector<std::string> brand_names;
    mutable std::mutex catalog_mutex;
};

#endif //SBTPC_CATALOGMANAGER_H