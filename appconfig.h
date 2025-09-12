// AppConfig.h
#pragma once

class SettingsManager;

class AppConfig
{
public:
    enum class Mode {
        Development,
        Production
    };

    enum class AppTheme {
        Light,
        Dim,
        Dark
    };

    static void setMode(Mode mode);
    static Mode mode();
    static bool isDevelopment();
    static bool isProduction();

    static void setTheme(AppTheme theme);
    static AppTheme theme();
    static bool isLight();
    static bool isDim();
    static bool isDark();

private:
    static Mode _mode;
    static AppTheme _theme;
};
