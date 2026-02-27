#include "Core.h"
#include "invoicemanager/InvoiceManager.h"
#include "ordermanager/OrderManager.h"
#include "catalogmanager/CatalogManager.h"
#include "routemanager/RouteManager.h"
#include "servermanager/ServerManager.h"
#include "../utils/Utils.h"
#include "../utils/logger/Logger.h"

// Static member initializations
bool Core::pending_refresh = false;
std::string Core::selected_invoice_path = "";
std::string Core::current_station_ip = "";
std::atomic<bool> Core::needs_ui_refresh{false};
float Core::global_row_height = 0.f;
// --- Manager Singletons ---
InvoiceManager& Core::invoice_manager() { static InvoiceManager instance; return instance; }
OrderManager&   Core::order_manager()   { static OrderManager instance; return instance; }
CatalogManager& Core::catalog_manager() { static CatalogManager instance; return instance; }
RouteManager&   Core::route_manager()   { static RouteManager instance; return instance; }
ServerManager&  Core::server_manager()  { static ServerManager instance; return instance; }
void Core::Initialize() {
    Core::catalog_manager().LoadCatalog("catalog.json");

    Core::route_manager().LoadRoutes();
    Core::route_manager().RefreshAllLists();

    Core::current_station_ip = Core::server_manager().GetLocalIP();
    Core::server_manager().GenerateIPBarcode(Core::current_station_ip);
}
bool Core::split_fuzzy_match(const std::string& query_lower, const std::string& b_lower, const std::string& n_lower) {
    if (query_lower.empty()) return true;

    std::stringstream ss(query_lower);
    std::string token;
    
    // We scan the brand and name combined for each search word
    while (ss >> token) {
        if (b_lower.find(token) == std::string::npos && 
            n_lower.find(token) == std::string::npos) {
            return false; // This token didn't match anything
        }
    }

    return true;
}
/*
bool Core::fuzzy_match(const std::string& query, const std::string& target) {
    if (query.empty()) return true;
    auto it_q = query.begin();
    auto it_t = target.begin();
    while (it_q != query.end() && it_t != target.end()) {
        if (std::tolower(*it_q) == std::tolower(*it_t)) it_q++;
        it_t++;
    }
    return it_q == query.end();
}*/