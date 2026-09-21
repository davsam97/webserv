#ifndef LOG_HPP
#define LOG_HPP

#include <string>

class Log {
    public:
        enum Level { DEBUG = 0, INFO = 1, EVENT = 2, WARNING = 3, ERROR = 4 };

    private:
        static Level _level;

        Log();
        Log(const Log& src);
        Log& operator=(const Log& src);
        ~Log();

    public:
        static int color_idx;

        static const std::string red;

        static const std::string red_bg;

        static const std::string bold;
        static const std::string underline;

        static const std::string reset;
        static const std::string nl;

        static const std::string color(int n);
        static const std::string background(int n);
        static const std::string gradient(int x, bool is_bg = 0);

        static std::string rgb_to_ansi(int r, int g, int b, bool is_bg = 0);

        static std::string timestamp();

        static void set_level(Level lvl);
        static void debug(const std::string& s);
        static void info(const std::string& s);
        static void event(const std::string s);
        static void warning(const std::string& s);
        static void error(const std::string& s);
};

#endif
