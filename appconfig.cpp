// AppConfig.cpp
#include "AppConfig.h"

AppConfig::Mode AppConfig::_mode = AppConfig::Mode::Kssr; // Default is KSSR

void AppConfig::setMode(Mode mode) {
    _mode = mode;
}

AppConfig::Mode AppConfig::mode() {
    return _mode;
}

bool AppConfig::isKssr() {
    return _mode == Mode::Kssr;
}

bool AppConfig::isNextDev() {
    return _mode == Mode::NextDev;
}

bool AppConfig::isDev() {
    return _mode == Mode::Dev;
}

// Legacy wrappers to prevent compilation errors in existing code
bool AppConfig::isDevelopment() {
    return isDev();
}

bool AppConfig::isProduction() {
    return isKssr();
}

bool AppConfig::isBeta() {
    return isNextDev();
}

AppConfig::AppTheme AppConfig::_theme = AppTheme::Dark;

void AppConfig::setTheme(AppTheme theme) {
    _theme = theme;
}

AppConfig::AppTheme AppConfig::theme() {
    return _theme;
}

bool AppConfig::isLight() {
    return _theme == AppTheme::Light;
}

bool AppConfig::isDim() {
    return _theme == AppTheme::Dim;
}

bool AppConfig::isDark() {
    return _theme == AppTheme::Dark;
}
