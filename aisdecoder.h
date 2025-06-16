#ifndef AISDECODER_H
#define AISDECODER_H

#include <QString>

struct AisData {
    int messageType = 0;
    int mmsi = 0;
    int navStatus = 0;
    int rot = 0;
    int sog = 0;
    bool posAccuracy = false;
    double latitude = 0.0;
    double longitude = 0.0;
    int cog = 0;
    int heading = 0;
    int timestamp = 0;
};

class AisDecoder {
public:
    static QString decodeAis(const QString &nmea);
    static double decodeAisOption(const QString &nmea, const QString &option, const QString &aivd);

private:
    static QString sixbitToBinary(const QString &payload);
    static int binaryToInt(const QString &bin, int start, int length, bool signedInt = false);
    static double decodeLatitude(int rawLat);
    static double decodeLongitude(int rawLon);
    static bool isValidNmea(const QString &nmea, const QString &aivd);
    static AisData parseAisMessage(const QString &nmea);
};

#endif // AISDECODER_H
