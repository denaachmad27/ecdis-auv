#ifndef SETTINGSMANAGER_H
#define SETTINGSMANAGER_H

#include "SettingsData.h"
#include <QDebug>

class SettingsManager {
    public:
        static SettingsManager& instance();

        void load();                            // Load from .ini
        void save(const SettingsData& data);    // Save to .ini
        const SettingsData& data() const;       // Access cached

    private:
        SettingsManager() = default;
        ~SettingsManager() {
            qDebug() << "[SETTINGS MANAGER] Destructor called";
        }
        SettingsData m_data;
};

#endif // SETTINGSMANAGER_H
