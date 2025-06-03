#ifndef NMEADECODER_H
#define NMEADECODER_H

#pragma once

#include <QString>
#include <QStringList>

struct AisData {
    QString mmsi;
    double lat = 0.0;
    double lon = 0.0;
    double sog = 0.0;
    double cog = 0.0;
    double heading = 0.0;
    double rot = 0.0;
};

struct OwnshipData {
    double lat = 0.0;
    double lon = 0.0;
    double sog = 0.0;
    double cog = 0.0;
    double heading = 0.0;
    double rot = 0.0;
};

class NmeaDecoder {
public:
    AisData decodeAIVDO(const QString &nmea);
    OwnshipData decodeOwnship(const QString &nmea);

private:
    QStringList tokenize(const QString &nmea);
    double parseDouble(const QString &str);
    double parseLatLon(const QString &val, const QString &dir);
};


#endif // NMEADECODER_H
