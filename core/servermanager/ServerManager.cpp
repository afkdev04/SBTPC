//
// Created by archdev on 2/20/26.
//
//use an older version of httpblib if you want this to be compatible with windows 7
#include "ServerManager.h"
#include "../Core.h"
#include "../catalogmanager/CatalogManager.h"    // Gives the definition of ProductManager and Product
#include "../routemanager/RouteManager.h"
#include "../../utils/Utils.h"
#include "../../utils/logger/Logger.h"
#include <nlohmann/json.hpp>   // Gives the definition of nlohmann::json
#include <fstream>             // Required for file operations like ifstream/ofstream
#include <filesystem>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <zint.h>
#include <GLFW/glfw3.h>
#include <nlohmann/json_fwd.hpp>
#include <chrono>
#include <iomanip>
#include <sstream>
static bool is_running = false;
std::string ServerManager::GetLocalIP() {
    struct ifaddrs *interfaces = NULL;
    struct ifaddrs *temp_addr = NULL;
    std::string ipAddress = "127.0.0.1";

    if (getifaddrs(&interfaces) == 0) {
        temp_addr = interfaces;
        while (temp_addr != NULL) {
            // THE FIX: Check if ifa_addr is NOT null before accessing sa_family
            if (temp_addr->ifa_addr != NULL && temp_addr->ifa_addr->sa_family == AF_INET) {
                // Ignore loopback
                if (std::string(temp_addr->ifa_name) != "lo") {
                    ipAddress = inet_ntoa(((struct sockaddr_in*)temp_addr->ifa_addr)->sin_addr);
                    // Optional: break; if you want the first valid IP found
                }
            }
            temp_addr = temp_addr->ifa_next;
        }
    }

    if (interfaces) {
        freeifaddrs(interfaces);
    }
    return ipAddress;
}
// Constructor
ServerManager::ServerManager() {}

// Destructor - ensures the server stops if the object is destroyed
ServerManager::~ServerManager() {
    Stop();
}
void ServerManager::Start(int port) {
    if (is_running) return; // Prevent multiple server instances
    is_running = true;
    // 1. PING ROUTE - For handheld connection testing
    svr.Get("/ping", [](const httplib::Request&, httplib::Response& res) {
        res.set_content("pong", "text/plain");
    });

    // 2. INVENTORY DOWNLOAD - Sends the whole catalog to the device
    svr.Get("/inventory", [this](const httplib::Request& req, httplib::Response& res) {
        if (Core::catalog_manager().GetCatalog().empty()) {
            res.status = 500;
            res.set_content("Catalog not loaded", "text/plain");
            return;
        }

        std::lock_guard<std::mutex> lock(Core::catalog_manager().GetMutex());
        nlohmann::json root = nlohmann::json::array();

        for (const auto& prod : Core::catalog_manager().GetCatalog()) {
            nlohmann::json item;
            item["brand"] = prod.brand;
            item["brandIndex"] = prod.brand_index;
            item["item"] = prod.name;
            item["itemIndex"] = prod.item_index;

            for (const auto& [name, offer] : prod.offers) {
                item["offers"][name]["barcode"] = offer.barcode;
                item["offers"][name]["price"] = offer.price;
            }
            root.push_back(item);
        }

        Utils::logger().Log(LogLevel::INFO, "Sync: Inventory downloaded by " + req.remote_addr);
        res.set_content(root.dump(), "application/json");
    });

    // 3. GET ORDER ROUTE - Device fetches a specific JSON order to fulfill
    svr.Get("/get_order", [&](const httplib::Request& req, httplib::Response& res) {
        if (!req.has_param("id")) {
            res.status = 400;
            return;
        }

        std::string orderId = req.get_param_value("id");
        std::string foundPath = "";

        {
            std::lock_guard<std::mutex> lock(Core::route_manager().route_mutex);
            for (const auto& [name, data] : Core::route_manager().route_storage) {
                for (const auto& p : data.outgoing) {
                    if (p.stem().string() == orderId) {
                        foundPath = p.string();
                        break;
                    }
                }
                if (!foundPath.empty()) break;
            }
        }

        if (!foundPath.empty()) {
            std::ifstream f(foundPath);
            if (f.is_open()) {
                std::stringstream buffer;
                buffer << f.rdbuf();

                Utils::logger().Log(LogLevel::INFO, "Order " + orderId + " sent to " + req.remote_addr);
                res.set_content(buffer.str(), "application/json");
                return;
            }
        }
        res.status = 404;
    });

    // 4. UPLOAD ROUTE - Handheld sends back the fulfilled order
    svr.Post("/upload_order", [&](const httplib::Request& req, httplib::Response& res) {
        if (!req.form.has_file("order_file")) {
            res.status = 400;
            return;
        }

        const auto& file = req.form.get_file("order_file");
        std::string safe_ip = req.remote_addr;
        std::replace(safe_ip.begin(), safe_ip.end(), '.', '_');

        // Note: Ensure the 'incoming' folder is organized by IP to prevent overwrites
        std::string filename = "incoming/" + safe_ip + "_" + file.filename;
        std::ofstream ofs(filename, std::ios::binary);

        if (ofs.is_open()) {
            ofs << file.content;
            ofs.close();

            Core::route_manager().RefreshAllLists();
            Utils::logger().Log(LogLevel::SUCCESS, "Order Uploaded: " + file.filename + " from " + req.remote_addr);

            glfwPostEmptyEvent(); // Refresh UI to show the new file
            res.set_content("OK", "text/plain");
            return;
        }
        res.status = 500;
    });

    // 5. UPDATE BARCODE - Allows the device to update the PC catalog on the fly
    svr.Post("/update_inventory", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            auto j = nlohmann::json::parse(req.body);
            std::string itemLabel = j.value("brand", "") + " " + j.value("item", "");

            Core::catalog_manager().UpdateBarcode(
                j.value("brand", ""),
                j.value("item", ""),
                j.value("barcode", ""),
                j.value("promo", "standard")
            );

            Utils::logger().Log(LogLevel::INFO, "Barcode Update: " + itemLabel + " updated by " + req.remote_addr);

            glfwPostEmptyEvent();
            res.set_content("OK", "text/plain");
        } catch (const std::exception& e) {
            res.status = 400;
            res.set_content(e.what(), "text/plain");
        }
    });
    // 6. THREAD START
    server_thread = std::thread([this, port]() {
        // This runs in the background
        if (!svr.listen("0.0.0.0", port)) {
            Utils::logger().Log(LogLevel::ERR, "Server failed to bind to port " + std::to_string(port));
            is_running = false;
        }
    });
}
void ServerManager::Stop() {
    if (is_running) {
        Utils::logger().Log(LogLevel::INFO, "Stopping server...");
        svr.stop(); // This breaks the svr.listen() loop

        if (server_thread.joinable()) {
            server_thread.join(); // Wait for the thread to actually exit
        }
        is_running = false;
        Utils::logger().Log(LogLevel::SUCCESS, "Server stopped.");
    }
}

void ServerManager::GenerateIPBarcode(std::string ip) {
    struct zint_symbol *my_symbol;
    my_symbol = ZBarcode_Create();

    // 1. Change Symbology to Code 128 (Standard 1D Barcode)
    my_symbol->symbology = BARCODE_CODE128;

    // 2. Set the data with your prefix
    std::string data = "IP:" + ip;

    // 3. Optional: Set scale so it's not tiny
    my_symbol->scale = 2.0f;
    my_symbol->height = 50; // Increases the vertical height of the bars
    my_symbol->whitespace_width = 10; // Adds some "quiet zone" on the sides
    strcpy(my_symbol->outfile, "ip_barcode.png");

    // Generate the barcode
    ZBarcode_Encode_and_Print(my_symbol, (unsigned char*)data.c_str(), 0, 0);
    ZBarcode_Delete(my_symbol);
}