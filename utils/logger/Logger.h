//
// Created by archdev on 2/25/26.
//

#ifndef SBTPC_LOGGER_H
#define SBTPC_LOGGER_H
#pragma once
#include "../Structs.h"
#include <string>
#include <vector>
#include <mutex>
#include <fstream>
#include <iostream>

class Logger {
public:
    Logger();  // Move this to public
    ~Logger(); // Move this to public

    // Main logging function
    void Log(LogLevel level, const std::string& message);

    // UI Helpers
    std::vector<std::string> GetLogs();
    void Clear();

private:
    std::string GetTimestamp();
    std::string LevelToString(LogLevel level);

    std::mutex log_mutex;
    std::vector<std::string> log_entries;
    std::ofstream log_file;
    const std::string filename = "server_log.txt";
};

#endif //SBTPC_LOGGER_H