#include "aisdecoder.h"
#include <cmath>
#include <QDebug>

QString AisDecoder::decodeAis(const QString &nmea) {
    if (!isValidNmea(nmea, "!AIVDM") && !isValidNmea(nmea, "!AIVDO"))
        return "Not an AIS message";

    AisData data = parseAisMessage(nmea);

    if (std::isnan(data.latitude) || std::isnan(data.longitude))
        return "Invalid position";

    return QString("TYPE: %1, LAT: %2, LONG: %3, SOG: %4, COG: %5, ROT: %6")
        .arg(QString::number(data.messageType))
        .arg(QString::number(data.latitude,'f', 6))
        .arg(QString::number(data.longitude, 'f', 6))
        .arg(QString::number(data.sog, 'f', 1))
        .arg(QString::number(data.cog, 'f', 1))
        .arg(QString::number(data.rot));
}

double AisDecoder::decodeAisOption(const QString &nmea, const QString &option, const QString &aivd) {
    if (!isValidNmea(nmea, aivd))
        return 0;

    AisData data = parseAisMessage(nmea);

    if (option == "messageType") return data.messageType;
    if (option == "mmsi") return data.mmsi;
    if (option == "navStatus") return data.navStatus;
    if (option == "rot") return data.rot;
    if (option == "sog") return data.sog;
    if (option == "posAccuracy") return data.posAccuracy ? 1 : 0;
    if (option == "latitude") return data.latitude;
    if (option == "longitude") return data.longitude;
    if (option == "cog") return data.cog;
    if (option == "heading") return data.heading;
    if (option == "timestamp") return data.timestamp;

    return 0;
}

AisData AisDecoder::parseAisMessage(const QString &nmea) {
    AisData result;

    QStringList parts = nmea.split(',');
    if (parts.length() < 7 || parts[5].isEmpty())
        return result;

    QString payload = parts[5];
    QString bin = sixbitToBinary(payload);
    if (bin.length() < 6)
        return result;

    result.messageType = binaryToInt(bin, 0, 6);

    if (bin.length() >= 38)
        result.mmsi = binaryToInt(bin, 8, 30);

    switch (result.messageType) {
    case 1:
    case 2:
    case 3:
        if (bin.length() >= 168) {
            result.navStatus = binaryToInt(bin, 38, 4);
            result.rot = binaryToInt(bin, 42, 8, true);
            result.sog = binaryToInt(bin, 50, 10);
            result.posAccuracy = binaryToInt(bin, 60, 1);
            int lonRaw = binaryToInt(bin, 61, 28, true);
            int latRaw = binaryToInt(bin, 89, 27, true);
            result.longitude = decodeLongitude(lonRaw);
            result.latitude = decodeLatitude(latRaw);
            result.cog = binaryToInt(bin, 116, 12);
            result.heading = binaryToInt(bin, 128, 9);
            result.timestamp = binaryToInt(bin, 137, 6);
        }
        break;
    case 4:
        if (bin.length() >= 168) {
            int lonRaw = binaryToInt(bin, 79, 28, true);
            int latRaw = binaryToInt(bin, 107, 27, true);
            result.longitude = decodeLongitude(lonRaw);
            result.latitude = decodeLatitude(latRaw);
        }
        break;
    case 5:
        if (bin.length() >= 424) {
            result.imo = binaryToInt(bin, 40, 30);
            result.callsign = decodeSixbitAscii(bin.mid(70, 42));
            result.shipname = decodeSixbitAscii(bin.mid(112, 120));
            result.shiptype = binaryToInt(bin, 232, 8);
            result.dimA = binaryToInt(bin, 240, 9);
            result.dimB = binaryToInt(bin, 249, 9);
            result.dimC = binaryToInt(bin, 258, 6);
            result.dimD = binaryToInt(bin, 264, 6);
        }
        break;
    case 18:
        if (bin.length() >= 168) {
            result.sog = binaryToInt(bin, 46, 10);
            int lonRaw = binaryToInt(bin, 57, 28, true);
            int latRaw = binaryToInt(bin, 85, 27, true);
            result.longitude = decodeLongitude(lonRaw);
            result.latitude = decodeLatitude(latRaw);
            result.cog = binaryToInt(bin, 112, 12);
            result.heading = binaryToInt(bin, 124, 9);
        }
        break;

    case 24:
        if (bin.length() >= 160) {
            int partNumber = binaryToInt(bin, 38, 2);
            if (partNumber == 0) {
                result.shipname = decodeSixbitAscii(bin.mid(40, 120));
            } else if (partNumber == 1) {
                result.shiptype = binaryToInt(bin, 40, 8);
                result.callsign = decodeSixbitAscii(bin.mid(48, 42));
                result.dimA = binaryToInt(bin, 90, 9);
                result.dimB = binaryToInt(bin, 99, 9);
                result.dimC = binaryToInt(bin, 108, 6);
                result.dimD = binaryToInt(bin, 114, 6);
            }
        }
        break;
    default:
        // Other message types can be added as needed
        break;
    }

    return result;
}

QString AisDecoder::decodeSixbitAscii(const QString &bin) {
    QString result;
    for (int i = 0; i + 6 <= bin.length(); i += 6) {
        int val = binaryToInt(bin, i, 6);
        QChar ch;
        if (val < 32) ch = QChar(val + 64); // @ to _
        else ch = QChar(val);
        if (ch != '@') result.append(ch); // Remove padding '@'
    }
    return result.trimmed();
}

QString AisDecoder::sixbitToBinary(const QString &payload) {
    QString bin;
    for (QChar ch : payload) {
        int val = ch.toLatin1() - 48;
        if (val > 40) val -= 8;
        bin += QString("%1").arg(val, 6, 2, QLatin1Char('0'));
    }
    return bin;
}

int AisDecoder::binaryToInt(const QString &bin, int start, int length, bool signedInt) {
    QString segment = bin.mid(start, length);
    if (!signedInt) return segment.toInt(nullptr, 2);

    int value = segment.toInt(nullptr, 2);
    if (segment[0] == '1') {
        value -= (1 << length);
    }
    return value;
}

double AisDecoder::decodeLatitude(int rawLat) {
    if (rawLat == 0x3412140 || rawLat == 0xFFFFFFF) return NAN; // not available
    return rawLat / 600000.0;
}

double AisDecoder::decodeLongitude(int rawLon) {
    if (rawLon == 0x6791AC0 || rawLon == 0xFFFFFFF) return NAN; // not available
    return rawLon / 600000.0;
}

bool AisDecoder::isValidNmea(const QString &nmea, const QString &aivd) {
    return nmea.startsWith(aivd) && nmea.contains(',');
}
