#pragma once

#include <QString>
#include <QtPlugin>

class IAisDvrPlugin {
public:
    virtual ~IAisDvrPlugin() {}

    virtual void startRecording(const QString& filePath) = 0;
    virtual void stopRecording() = 0;
    virtual bool isRecording() const = 0;
    virtual void recordRawNmea(const QString& nmeaSentence) = 0;
};

#define IAisDvrPlugin_iid "org.ecdis.plugin.IAisDvrPlugin"
Q_DECLARE_INTERFACE(IAisDvrPlugin, IAisDvrPlugin_iid)
