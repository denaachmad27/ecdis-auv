// nmea_decoder.cpp
#include "nmeadecoder.h"

#include <QBitArray>
#include <QByteArray>

QStringList NmeaDecoder::tokenize(const QString &nmea) {
    QString trimmed = nmea.trimmed();
    if (trimmed.startsWith("$") || trimmed.startsWith("!")) {
        trimmed.remove(0, 1);
    }
    int asteriskIdx = trimmed.indexOf("*");
    if (asteriskIdx != -1) {
        trimmed = trimmed.left(asteriskIdx);
    }
    return trimmed.split(",");
}

double NmeaDecoder::parseDouble(const QString &str) {
    bool ok;
    double val = str.toDouble(&ok);
    return ok ? val : 0.0;
}

double NmeaDecoder::parseLatLon(const QString &val, const QString &dir) {
    if (val.isEmpty() || dir.isEmpty()) return 0.0;
    double deg = val.leftRef(val.indexOf("." ) - 2).toDouble();
    double min = val.midRef(val.indexOf("." ) - 2).toDouble();
    double coord = deg + (min / 60.0);
    if (dir == "S" || dir == "W") coord *= -1;
    return coord;
}

static QByteArray ais6bitToBitstream(const QString &payload) {
    QByteArray bitstream;
    for (QChar ch : payload) {
        int val = ch.toLatin1() - 48;
        if (val > 40) val -= 8;
        for (int i = 5; i >= 0; --i)
            bitstream.append((val >> i) & 0x01);
    }
    return bitstream;
}

static quint32 getBits(const QByteArray &bits, int start, int length) {
    quint32 value = 0;
    for (int i = 0; i < length; ++i) {
        value <<= 1;
        value |= (bits[start + i] & 0x01);
    }
    return value;
}

AisData NmeaDecoder::decodeAIVDO(const QString &nmea) {
    AisData data;
    if (!nmea.startsWith("!AIVDO")) return data;
    QStringList tokens = tokenize(nmea);
    if (tokens.size() < 6) return data;

    QString payload = tokens[5];
    QByteArray bitstream = ais6bitToBitstream(payload);

    quint8 messageType = getBits(bitstream, 0, 6);
    if (messageType < 1 || messageType > 3) return data;  // Only types 1-3 supported

    quint32 mmsi = getBits(bitstream, 8, 30);
    quint32 sog_raw = getBits(bitstream, 50, 10);
    quint32 cog_raw = getBits(bitstream, 116, 12);
    quint32 heading_raw = getBits(bitstream, 128, 9);
    qint32 rot_raw = getBits(bitstream, 42, 8);
    quint32 lat_raw = getBits(bitstream, 61, 28);  // AIS position (28 bits latitude)
    quint32 lon_raw = getBits(bitstream, 89, 28);  // AIS position (28 bits longitude)

    // ROT decoding (Rate of Turn)
    double rot;
    if (rot_raw == 128)
        rot = 0.0;
    else if (rot_raw > 127)
        rot = -((rot_raw - 256) / 4.733);
    else
        rot = rot_raw / 4.733;

    data.mmsi = QString::number(mmsi);
    data.sog = (sog_raw == 1023) ? 0.0 : sog_raw / 10.0;
    data.cog = (cog_raw == 3600) ? 0.0 : cog_raw / 10.0;
    data.heading = (heading_raw == 511) ? 0.0 : heading_raw;
    data.rot = rot;

    // Decode AIS Position (Latitude and Longitude)
    data.lat = (lat_raw == 0) ? 0.0 : lat_raw / 600000.0;  // Decode AIS latitude
    data.lon = (lon_raw == 0) ? 0.0 : lon_raw / 600000.0;  // Decode AIS longitude

    return data;
}

OwnshipData NmeaDecoder::decodeOwnship(const QString &nmea) {
    OwnshipData data;
    QStringList tokens = tokenize(nmea);

    if (nmea.startsWith("$GPRMC") && tokens.size() >= 12) {
        data.lat = parseLatLon(tokens[3], tokens[4]);
        data.lon = parseLatLon(tokens[5], tokens[6]);
        data.sog = parseDouble(tokens[7]);
        data.cog = parseDouble(tokens[8]);
    }
    else if (nmea.startsWith("$HEHDT") && tokens.size() >= 2) {
        data.heading = parseDouble(tokens[1]);
    }
    else if (nmea.startsWith("$ROT") && tokens.size() >= 2) {
        data.rot = parseDouble(tokens[1]);
    }

    return data;
}
