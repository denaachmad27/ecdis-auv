#include "PluginManager.h"

PluginManager& PluginManager::instance() {
    static PluginManager inst;
    return inst;
}

bool PluginManager::loadPlugin(const QString& pluginPath, const QString& key) {
    if (m_plugins.contains(key)) {
        qWarning() << "[PluginManager] Plugin with key" << key << "already loaded.";
        return false;
    }

    QPluginLoader loader(QCoreApplication::applicationDirPath() + pluginPath);
    QObject* rawPlugin = loader.instance();
    if (!rawPlugin) {
        qWarning() << "[PluginManager] Failed to load plugin:" << loader.errorString();
        return false;
    }

    m_plugins[key] = rawPlugin;
    qDebug() << "[PluginManager] Loaded plugin:" << pluginPath << "as key:" << key;
    return true;
}
