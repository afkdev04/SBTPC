//
// Created by archdev on 2/20/26.
//

#ifndef SBTPC_INVOICEMANAGER_H
#define SBTPC_INVOICEMANAGER_H

#pragma once
#include <string>
#include <vector>
#include <fstream>
#include <nlohmann/json.hpp>
#include "../catalogmanager/CatalogManager.h"
#include "../../utils/Structs.h"

using json = nlohmann::json;

class InvoiceManager {
public:
    std::vector<InvoiceLine> invoice_items;
    long long grand_total = 0;
    bool order_complete_flag = false;

    // --- ADD THESE THREE LINES ---
    void RecalculateTotal();
    void ApplyManualCredit(long long cents);
    void ApplyItemCredit(const std::string& name, long long unitPriceCents);

    // Existing functions
    void BuildInvoice(const std::string& filePath);
    void SaveInvoiceToCSV(const std::string& originalPath);
};

#endif //SBTPC_INVOICEMANAGER_H