#include "log/Log.hpp"

#include <cmath>
#include <ctime>
#include <iostream>
#include <sstream>

namespace {
    const std::string black = "\033[30m";
}

Log::Level Log::_level = Log::INFO;

int Log::color_idx = 0;
const std::string Log::red = "\033[31m";

const std::string Log::red_bg = "\033[41m";

const std::string Log::bold = "\033[1m";
const std::string Log::underline = "\033[4m";

const std::string Log::reset = "\033[0m";
const std::string Log::nl = "\033[0m\n";

// get a color %n
const std::string Log::color(int n) {
    return gradient(n % 32);
}

// get a background color %n
const std::string Log::background(int n) {
    return gradient(n % 32, true);
}

// Looping rainbow color for step x: three sine waves, 120 degrees apart (R, G, B).
const std::string Log::gradient(int x, bool is_bg) {
    static const double PI = 3.14159265358979323846;
    const int PERIOD = 32;  // steps for one full rainbow loop

    int step = ((x % PERIOD) + PERIOD) % PERIOD;  // keep it positive
    double t = step * 2.0 * PI / PERIOD;

    int r = static_cast<int>((std::sin(t + 0.0 * PI / 3.0) * 0.5 + 0.5) * 255);
    int g = static_cast<int>((std::sin(t + 2.0 * PI / 3.0) * 0.5 + 0.5) * 255);
    int b = static_cast<int>((std::sin(t + 4.0 * PI / 3.0) * 0.5 + 0.5) * 255);

    return rgb_to_ansi(r, g, b, is_bg);
}

// auto downscale from 0-255 -> 0-5
std::string Log::rgb_to_ansi(int r, int g, int b, bool is_bg) {
    // map 0-255 -> 0-5 (each step is 51 wide)
    int ri = r * 6 / 256;
    int gi = g * 6 / 256;
    int bi = b * 6 / 256;
    int index = 16 + 36 * ri + 6 * gi + bi;

    std::ostringstream oss;
    if (is_bg)
        oss << "\033[48;5;" << index << "m";
    else
        oss << "\033[38;5;" << index << "m";
    return oss.str();
}

// current local time as "HH:MM:SS"
std::string Log::timestamp() {
    std::time_t now = std::time(0);
    std::tm* lt = std::localtime(&now);

    char buf[16];
    std::strftime(buf, sizeof(buf), "%H:%M:%S", lt);
    return std::string(buf);
}

void Log::set_level(Level lvl) { _level = lvl; }

void Log::debug(const std::string& s) {
    if (_level > Log::DEBUG) return;
    std::cout << rgb_to_ansi(255, 240, 240) << s << nl;
}

void Log::info(const std::string& s) {
    if (_level > Log::INFO) return;
    std::cout << color(color_idx) << s << nl;
}

void Log::event(const std::string s) {
    if (_level > Log::EVENT) return;
    std::cout << background(color_idx) << black << "[" << timestamp() << "] " << s << nl;
}

void Log::warning(const std::string& s) {
    if (_level > Log::WARNING) return;
    std::cout << background(color_idx) << black << "[" << timestamp() << "] " << s << nl;
}

void Log::error(const std::string& s) {
    if (_level > Log::ERROR) return;
    std::cerr << red << "[" << timestamp() << "] " << s << nl;
}
