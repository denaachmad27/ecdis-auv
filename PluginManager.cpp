#include "PluginManager.h"
#include <QDir>

PluginManager& PluginManager::instance() {
    static PluginManager inst;
    return inst;
}

bool PluginManager::loadPlugin(const QString& pluginPath, const QString& key) {
    if (m_plugins.contains(key)) {
        qCritical() << "[PLUGIN MANAGER] Plugin with key" << key << "already loaded.";
        return false;
    }

    //QPluginLoader loader(QCoreApplication::applicationDirPath() + pluginPath);
    QDir baseDir(QString::fromLocal8Bit(qgetenv("APPDATA")));
    QString path = baseDir.filePath("SevenCs/EC2007/DENC/PLUGIN/");

    QPluginLoader loader(path + pluginPath);
    QObject* rawPlugin = loader.instance();
    if (!rawPlugin) {
        qCritical() << "[PLUGIN MANAGER] Failed to load plugin:" << loader.errorString();
        return false;
    }

    m_plugins[key] = rawPlugin;
    qCritical() << "[PLUGIN MANAGER] Loaded plugin:" << pluginPath << "as key:" << key;
    return true;
}
