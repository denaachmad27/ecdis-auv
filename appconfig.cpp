// AppConfig.cpp
#include "AppConfig.h"

AppConfig::Mode AppConfig::_mode = AppConfig::Mode::Development;

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
