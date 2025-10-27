#include "PluginManager.h"
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QStandardPaths>

PluginManager& PluginManager::instance() {
    static PluginManager inst;
    return inst;
}

bool PluginManager::loadPlugin(const QString& pluginPath, const QString& key) {
    if (m_plugins.contains(key)) {
        qCritical() << "[PLUGIN MANAGER] Plugin with key" << key << "already loaded.";
        return false;
    }

    // Build robust full path for plugin
    QString fullPath;
    QFileInfo reqInfo(pluginPath);

    // If caller passed a path with a leading slash but not a valid absolute path on Windows,
    // try to treat it as just a file name
    QString candidateName = reqInfo.fileName().isEmpty() ? pluginPath : reqInfo.fileName();

    if (reqInfo.isAbsolute() && QFile::exists(pluginPath)) {
        fullPath = QDir::cleanPath(pluginPath);
    } else {
        // Resolve APPDATA base, with fallback
        QString baseAppData = QString::fromLocal8Bit(qgetenv("APPDATA"));
        if (baseAppData.isEmpty()) {
            baseAppData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
            if (baseAppData.isEmpty()) baseAppData = QCoreApplication::applicationDirPath();
        }
        QDir baseDir(baseAppData);
        QString pluginDir = baseDir.filePath("SevenCs/EC2007/DENC/PLUGIN");
        fullPath = QDir(pluginDir).filePath(candidateName);
        fullPath = QDir::cleanPath(fullPath);

        // Fallback: also try next to executable if not found
        if (!QFile::exists(fullPath)) {
            QString alt = QDir(QCoreApplication::applicationDirPath()).filePath(candidateName);
            if (QFile::exists(alt)) fullPath = alt;
        }
    }

    if (!QFile::exists(fullPath)) {
        qCritical() << "[PLUGIN MANAGER] Plugin file not found:" << fullPath << "(requested:" << pluginPath << ")";
        return false;
    }

    QPluginLoader loader(fullPath);
    QObject* rawPlugin = loader.instance();
    if (!rawPlugin) {
        qCritical() << "[PLUGIN MANAGER] Failed to load plugin at" << fullPath << ":" << loader.errorString();
        return false;
    }

    m_plugins[key] = rawPlugin;
    qCritical() << "[PLUGIN MANAGER] Loaded plugin:" << fullPath << "as key:" << key;
    return true;
}
