#include "gribdata.h"

GribData::GribData()
    : fileName(QString())
    , filePath(QString())
    , fileSize(0)
    , globalMinLat(0.0)
    , globalMaxLat(0.0)
    , globalMinLon(0.0)
    , globalMaxLon(0.0)
    , parameterName(QString())
    , parameterUnits(QString())
    , generatingCenter(QString())
    , model(QString())
{
}

GribData::~GribData()
{
    clear();
}

void GribData::clear()
{
    messages.clear();
    fileName.clear();
    filePath.clear();
    fileSize = 0;
    globalMinLat = 0.0;
    globalMaxLat = 0.0;
    globalMinLon = 0.0;
    globalMaxLon = 0.0;
    parameterName.clear();
    parameterUnits.clear();
    generatingCenter.clear();
    model.clear();
}

QStringList GribData::getTimeStepLabels() const
{
    QStringList labels;
    for (const auto& msg : messages) {
        QString label;
        if (msg.forecastHour == 0) {
            label = QString("Analysis (%1)")
                        .arg(msg.referenceTime.toString("yyyy-MM-dd HH:mm"));
        } else {
            label = QString("+%1h (%2)")
                        .arg(msg.forecastHour)
                        .arg(msg.forecastTime.toString("MM-dd HH:mm"));
        }
        labels << label;
    }
    return labels;
}

void GribData::updateBounds()
{
    if (messages.isEmpty()) {
        globalMinLat = globalMaxLat = 0.0;
        globalMinLon = globalMaxLon = 0.0;
        return;
    }

    globalMinLat = messages.first().minLat;
    globalMaxLat = messages.first().maxLat;
    globalMinLon = messages.first().minLon;
    globalMaxLon = messages.first().maxLon;

    for (const auto& msg : messages) {
        if (msg.minLat < globalMinLat) globalMinLat = msg.minLat;
        if (msg.maxLat > globalMaxLat) globalMaxLat = msg.maxLat;
        if (msg.minLon < globalMinLon) globalMinLon = msg.minLon;
        if (msg.maxLon > globalMaxLon) globalMaxLon = msg.maxLon;
    }
}
