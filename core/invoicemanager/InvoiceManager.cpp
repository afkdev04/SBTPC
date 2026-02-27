//
// Created by archdev on 2/20/26.
//

#include "InvoiceManager.h"
#include "../Core.h"
#include "../catalogmanager/CatalogManager.h"
#include "../../utils/Utils.h"
#include "../../utils/logger/Logger.h"
#include <fstream>
#include <iomanip>

void InvoiceManager::BuildInvoice(const std::string& filePath) {
    invoice_items.clear();
    grand_total = 0;

    std::ifstream file(filePath);
    if (!file.is_open()) {
        Utils::logger().Log(LogLevel::ERR, "Invoice Builder: Could not open " + filePath);
        return;
    }

    nlohmann::json orderData;
    try {
        file >> orderData;
    } catch (const std::exception& e) {
        Utils::logger().Log(LogLevel::ERR, "Invoice Builder: JSON Parse Error in " + filePath);
        return;
    }

    // Extract just the filename for cleaner logging
    std::string filename = std::filesystem::path(filePath).filename().string();

    std::lock_guard<std::mutex> lock(Core::catalog_manager().GetMutex());
    const auto& catalog = Core::catalog_manager().GetCatalog();

    for (const auto& entry : orderData) {
        int itemIdx   = entry.value("m_iItemIndex", -1);
        int requested = entry.value("m_iRequested", 0);
        int got       = entry.value("m_iGot", 0);
        bool complete = entry.value("m_bComplete", false);
        bool disabled = entry.value("m_bDisabled", false);

        if (itemIdx >= 0 && itemIdx < (int)catalog.size()) {
            const auto& prod = catalog[itemIdx];

            long long priceInCents = 0;
            if (prod.offers.count("standard")) {
                priceInCents = prod.offers.at("standard").price;
            }

            InvoiceLine line;
            line.name = prod.name; // This will show as Brand + Name based on your Catalog logic
            line.requested = requested;
            line.disabled = disabled;
            line.complete = complete;
            line.is_credit = false;

            if (disabled)      line.got = got;
            else if (complete) line.got = (got > 0) ? got : requested;
            else               line.got = got;

            line.price = priceInCents;
            line.subtotal = (long long)line.got * line.price;

            invoice_items.push_back(line);
        }
    }

    RecalculateTotal();
    Utils::logger().Log(LogLevel::INFO, "Invoice Built: " + filename + " (" + std::to_string(invoice_items.size()) + " items)");
}

void InvoiceManager::RecalculateTotal() {
    grand_total = 0;
    for (auto& item : invoice_items) {
        item.subtotal = item.got * item.price;
        grand_total += item.subtotal;
    }
}

void InvoiceManager::ApplyItemCredit(const std::string& name, long long unitPriceCents) {
    InvoiceLine credit;
    credit.name = "[CREDIT] " + name;
    credit.requested = 1;
    credit.got = 1;
    credit.price = -unitPriceCents;
    credit.is_credit = true;
    credit.disabled = false;
    credit.complete = true;

    invoice_items.push_back(credit);
    RecalculateTotal();

    Utils::logger().Log(LogLevel::SUCCESS, "Applied Item Credit: " + name);
}

void InvoiceManager::ApplyManualCredit(long long cents) {
    InvoiceLine credit;
    credit.name = "MANUAL ADJUSTMENT";
    credit.requested = 1;
    credit.got = 1;
    credit.price = -cents;
    credit.is_credit = true;
    credit.disabled = false;
    credit.complete = true;

    invoice_items.push_back(credit);
    RecalculateTotal();

    double dollarVal = static_cast<double>(cents) / 100.0;
    Utils::logger().Log(LogLevel::SUCCESS, "Applied Manual Credit: $" + std::to_string(dollarVal));
}

void InvoiceManager::SaveInvoiceToCSV(const std::string& originalPath) {
    if (invoice_items.empty()) {
        Utils::logger().Log(LogLevel::WARNING, "Export Blocked: Invoice is empty.");
        return;
    }

    std::string csvPath = originalPath;
    size_t lastDot = csvPath.find_last_of(".");
    if (lastDot != std::string::npos) csvPath = csvPath.substr(0, lastDot);
    csvPath += "_invoice.csv";

    std::ofstream csv(csvPath);
    if (!csv.is_open()) {
        // This usually triggers if the file is open in Excel
        Utils::logger().Log(LogLevel::ERR, "CSV Export Failed: File is locked or path invalid.");
        throw std::runtime_error("File Locked"); // Let the UI catch this for the popup
    }

    csv << "Item Name,Status,Qty,Unit Price,Subtotal\n";

    for (const auto& line : invoice_items) {
        std::string status = line.is_credit ? "CREDIT" : (line.disabled ? "STOPPED" : "DONE");

        double dPrice = line.price / 100.0;
        double dSub   = line.subtotal / 100.0;

        csv << "\"" << line.name << "\","
            << status << ","
            << line.got << ","
            << std::fixed << std::setprecision(2) << dPrice << ","
            << dSub << "\n";
    }

    double finalTotal = static_cast<double>(grand_total) / 100.0;
    csv << "\n,,,TOTAL,$" << std::fixed << std::setprecision(2) << finalTotal << "\n";
    csv.close();

    Utils::logger().Log(LogLevel::SUCCESS, "CSV Export Successful: " + csvPath);
}