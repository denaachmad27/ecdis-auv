#pragma once

#include <QObject>
#include <QMap>
#include <QString>
#include <QPluginLoader>
#include <QDebug>
#include <type_traits>
#include <QCoreApplication>

class PluginManager {
public:
    static PluginManager& instance();

    bool loadPlugin(const QString& pluginPath, const QString& key);

    template<typename T>
    T* getPlugin(const QString& key) {
        QObject* obj = m_plugins.value(key, nullptr);
        if (!obj) return nullptr;

        T* plugin = qobject_cast<T*>(obj);
        if (!plugin) {
            qWarning() << "[PluginManager] Plugin for key" << key << "does not match expected type.";
        }
        return plugin;
    }

    QString getPluginPath(QString path) const;

private:
    PluginManager() = default;
    ~PluginManager(); // CRITICAL: Need proper destructor to unload plugins

    PluginManager(const PluginManager&) = delete;
    PluginManager& operator=(const PluginManager&) = delete;

    QMap<QString, QObject*> m_plugins;
    QMap<QString, QPluginLoader*> m_loaders; // CRITICAL: Keep loaders to properly unload plugins
};
