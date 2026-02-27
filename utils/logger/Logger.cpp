//
// Created by archdev on 2/25/26.
//

#include "Logger.h"
#include <chrono>
#include <iomanip>

Logger::Logger() {
    log_file.open(filename, std::ios::app);
}

Logger::~Logger() {
    if (log_file.is_open()) log_file.close();
}
void Logger::Log(LogLevel level, const std::string& message) {
    std::lock_guard<std::mutex> lock(log_mutex);

    std::string timestamp = GetTimestamp();
    std::string levelStr = LevelToString(level);
    std::string formatted = "[" + timestamp + "] [" + levelStr + "] " + message;

    // 1. Output to Console (Stdout) - SKIP IF DEBUG
    if (level != LogLevel::DEBUG) {
        std::cout << formatted << std::endl;
    }

    // 2. Output to File - ALWAYS log everything to the file for post-mortem
    if (log_file.is_open()) {
        log_file << formatted << std::endl;
        log_file.flush();
    }

    // 3. Store for UI
    log_entries.push_back(formatted);
    if (log_entries.size() > 100) {
        log_entries.erase(log_entries.begin());
    }
}

std::vector<std::string> Logger::GetLogs() {
    std::lock_guard<std::mutex> lock(log_mutex);
    return log_entries;
}

void Logger::Clear() {
    std::lock_guard<std::mutex> lock(log_mutex);
    log_entries.clear();
}

std::string Logger::GetTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&in_time_t), "%H:%M:%S");
    return ss.str();
}

std::string Logger::LevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::INFO:    return "INFO";
        case LogLevel::WARNING: return "WARN";
        case LogLevel::ERR:     return "ERROR";
        case LogLevel::SUCCESS: return "OK";
        default:      return "LOG";
    }
}