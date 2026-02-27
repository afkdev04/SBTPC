//
// Created by archdev on 2/20/26.
//

#ifndef SBTPC_SERVER_H
#define SBTPC_SERVER_H

#include "../../utils/httplib.h"
#include <thread>
#include <mutex>
#include <vector>
#include <string>

class RouteManager;
class ProductManager;

class ServerManager {
public:
    httplib::Server svr;
    ServerManager();
    ~ServerManager();

    std::string GetLocalIP();
    void GenerateIPBarcode(std::string ip);
    void Start(int port);
    void Stop();
    std::vector<std::string> GetScanLog() {
        std::lock_guard<std::mutex> lock(data_mutex);
        return orders;
    }
    bool isRunning() {
        return svr.is_running();
    }
private:
    std::thread server_thread;
    std::mutex data_mutex;
    std::vector<std::string> orders;
};


#endif //SBTPC_SERVER_H