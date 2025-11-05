#ifndef POI_H
#define POI_H

#include <QString>
#include <QDateTime>
#include <limits>

#include <eckernel.h>

struct PoiEntry
{
    int id = -1;
    QString label;
    QString description;
    EcPoiCategory category = EC_POI_GENERIC;
    double latitude = std::numeric_limits<double>::quiet_NaN();
    double longitude = std::numeric_limits<double>::quiet_NaN();
    double depth = std::numeric_limits<double>::quiet_NaN();
    UINT32 flags = EC_POI_FLAG_ACTIVE | EC_POI_FLAG_PERSISTENT;
    EcPoiHandle handle = nullptr;
    QDateTime createdAt = QDateTime::currentDateTimeUtc();
    QDateTime updatedAt = createdAt;
};

#endif // POI_H
