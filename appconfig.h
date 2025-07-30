// AppConfig.h
#pragma once

class AppConfig
{
public:
    enum class Mode {
        Development,
        Production
    };

    static void setMode(Mode mode);
    static Mode mode();
    static bool isDevelopment();
    static bool isProduction();

private:
    static Mode _mode;
};
