#ifndef PROGRESS_H
#define PROGRESS_H

#include "utils.h"

class ProgressBar {
public:
    ProgressBar(int total, int barWidth = 70, char symbol = '=') : total_(total), barWidth_(barWidth), symbol_(symbol) {
        startTime_ = std::chrono::high_resolution_clock::now();
    }

    void update(int progress, std::string customData = "", std::string unit = "B") {
        double percentage = (double) progress / total_ * 100;
        auto currentTime = std::chrono::high_resolution_clock::now();
        double elapsedTime =
                std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime_).count() / 1000.0;
        double speed = progress / elapsedTime;
        std::stringstream ss;
        ss << std::setprecision(1) << std::fixed << percentage << "%|";
        int pos = static_cast<int>(barWidth_ * percentage / 100);
        for (int i = 0; i < barWidth_; ++i) {
            if (i < pos) ss << symbol_;
            else if (i == pos) ss << ">";
            else ss << " ";
        }
        ss << "| speed:" << formatSize(speed) << "/s";
        if (!customData.empty()) {
            ss << " " << customData;
        }
        std::cout << "\r" << ss.str() << std::flush;
    }

    void end() {
        std::cout << std::endl;
    }

    static std::string formatSize(double size) {
        std::string units[] = {"B", "KB", "MB", "GB", "TB"};
        int unitIndex = 0;
        while (size >= 1024 && unitIndex < 4) {
            size /= 1024;
            unitIndex++;
        }
        std::stringstream ss;
        ss << std::setprecision(1) << std::fixed << size << units[unitIndex];
        return ss.str();
    }

    static std::string formatTime(double seconds) {
        int hours = static_cast<int>(seconds / 3600);
        int minutes = static_cast<int>(seconds / 60) % 60;
        int secs = static_cast<int>(seconds) % 60;
        std::stringstream ss;
        if (hours > 0) {
            ss << std::setfill('0') << std::setw(2) << hours << ":";
        }
        ss << std::setfill('0') << std::setw(2) << minutes << ":" << std::setfill('0') << std::setw(2) << secs;
        return ss.str();
    }

private:
    int total_;
    int barWidth_;
    char symbol_;
    std::chrono::time_point<std::chrono::high_resolution_clock> startTime_;


};

#endif