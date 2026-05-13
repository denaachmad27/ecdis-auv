 // AppConfig.h
#pragma once

class SettingsManager;

class AppConfig
{
public:
    enum class Mode {
        Kssr,       // Mode terbatas fitur tertentu saja (Default)
        NextDev,    // Mode full fitur yang sudah rilis
        Dev         // Mode full fitur bahkan yang masih di-develop
    };

    enum class AppTheme {
        Light,
        Dim,
        Dark
    };

    static void setMode(Mode mode);
    static Mode mode();
    
    // New specific mode checkers
    static bool isKssr();
    static bool isNextDev();
    static bool isDev();

    // Legacy support (to be phased out)
    static bool isDevelopment(); // Maps to isDev()
    static bool isProduction();  // Maps to isKssr()
    static bool isBeta();        // Maps to isNextDev()

    static void setTheme(AppTheme theme);
    static AppTheme theme();
    static bool isLight();
    static bool isDim();
    static bool isDark();

private:
    static Mode _mode;
    static AppTheme _theme;
};
