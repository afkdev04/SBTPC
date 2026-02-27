#ifndef SBTPC_CORE_H
#define SBTPC_CORE_H

#include <atomic>
#include <string>
class InvoiceManager;
class OrderManager;
class CatalogManager;
class RouteManager;
class ServerManager;

class Core {
public:
    static void Initialize();

    static InvoiceManager& invoice_manager();
    static OrderManager&   order_manager();
    static CatalogManager& catalog_manager();
    static RouteManager&   route_manager();
    static ServerManager&  server_manager();

    static bool split_fuzzy_match(const std::string& query_lower, const std::string& b_lower, const std::string& n_lower);

    static bool pending_refresh;
    static std::string selected_invoice_path;
    static std::string current_station_ip;
    static std::atomic<bool> needs_ui_refresh;
    static float global_row_height;
};

#endif //SBTPC_CORE_H