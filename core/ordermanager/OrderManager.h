#ifndef SBTPC_ORDERMANAGER_H
#define SBTPC_ORDERMANAGER_H
#pragma once
#include <vector>
#include <string>
#include <algorithm>
#include "../catalogmanager/CatalogManager.h"
#include "../../utils/Structs.h"

class RouteManager;

class OrderManager {
public:
    void LoadOrderFromFile(const std::string& filepath);
    void AddItem(const std::string& barcode, const std::string& name, long long price, int bIdx, int iIdx);
    void RemoveItem(int index);
    void ClearOrder();
    long long GetTotal() const;
    void RenderOrderUI();
    void SaveOrderToPending();
    bool IsEmpty() const { return current_items.empty(); }
    std::vector<OrderItem> current_items;
    char order_name[256] = "New Order";
};
#endif