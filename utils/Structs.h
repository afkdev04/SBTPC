//
// Created by archdev on 2/25/26.
//

#ifndef SBTPC_STRUCTS_H
#define SBTPC_STRUCTS_H
#include <map>
#include <string>
#include <vector>
#include <filesystem>
//0.6f, 0.2f, 0.2f, 1.0f
struct Colors {
    // RGBA format
    inline static float red[4] =              { 0.6f, 0.2f, 0.2f, 1.0f };
    inline static float green[4] =            { 0.1f, 0.4f, 0.1f, 1.0f };
    inline static float yellow[4] =            { 1.0f, 0.8f, 0.0f, 1.0f };
    inline static float blue[4] =            { 0.4f, 0.7f, 1.0f, 1.0f };
    // same as framebg
    inline static float darkbg[4] =            { 0.05f, 0.05f, 0.06f, 1.00f };
};
// --- Logging Types ---
enum class LogLevel {
    INFO,
    WARNING,
    ERR,
    SUCCESS,
    DEBUG
};

// --- Catalog Types ---
struct Offer {
    std::string barcode;
    long long price;
};

struct Product {
    std::string brand;
    std::string name;
    std::string brand_lower;
    std::string name_lower;
    int brand_index = 0;
    int item_index = 0;
    std::map<std::string, Offer> offers;
};


// --- Invoice Types ---
struct InvoiceLine {
    std::string name;
    int requested = 0;
    int got = 0;
    long long price = 0;
    long long subtotal = 0;
    bool complete = false;
    bool disabled = false;
    bool is_credit = false;
};

struct OrderItem {
    std::string barcode;
    std::string name;
    long long price = 0;
    int quantity = 1;     // This is "requested_qty"

    // Checklist/Progress fields
    int got = 0;          // "got_qty"
    bool complete = false; // "is_complete"
    bool disabled = false; // "is_disabled"

    // Metadata for UI/Catalog mapping
    int brand_index = -1;
    int item_index = -1;
};

struct RouteData {
    std::vector<std::filesystem::path> outgoing;
    std::vector<std::filesystem::path> incoming;
};

#endif //SBTPC_STRUCTS_H