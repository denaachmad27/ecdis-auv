// AppConfig.cpp
#include "AppConfig.h"

AppConfig::Mode AppConfig::_mode = AppConfig::Mode::Beta;

void AppConfig::setMode(Mode mode) {
    _mode = mode;
}

AppConfig::Mode AppConfig::mode() {
    return _mode;
}

bool AppConfig::isDevelopment() {
    return _mode == Mode::Development;
}

bool AppConfig::isProduction() {
    return _mode == Mode::Production;
}

bool AppConfig::isBeta() {
    return _mode == Mode::Beta;
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
