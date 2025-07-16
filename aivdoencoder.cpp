#include "AIVDOEncoder.h"
#include "aisdecoder.h"
#include <QDebug>
#include <QString>
#include <QByteArray>
#include <bitset>
#include <iomanip>

// AIS 6-bit encoding character set
const QString AIS_CHARSET = "@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^- !\"#$%&'()*+,-./0123456789:;<=>?";

// Fungsi menghitung checksum NMEA
QString AIVDOEncoder::calculateNMEAChecksum(const QString &sentence) {
    int checksum = 0;
    for (int i = 1; i < sentence.length(); ++i) {
        checksum ^= sentence[i].toLatin1();
    }
    return QString("*%1\r\n").arg(checksum, 2, 16, QLatin1Char('0')).toUpper();
}

// Fungsi mengonversi bitstream ke 6-bit ASCII AIS
QString AIVDOEncoder::binaryToAIS6Bit(const QString &bitstream) {
    QString padded = bitstream.leftJustified(168, '0');

    QString encoded;
    for (int i = 0; i < 168; i += 6) {
        QString chunk = padded.mid(i, 6);
        int value = chunk.toInt(nullptr, 2);
        value += 48;
        if (value > 87) value += 8;
        encoded += QChar(value);
    }

    return encoded;
}

// Fungsi utama untuk encoding AIS Message Type 1 (AIVDO)
QString AIVDOEncoder::encodeAIVDO(int mmsi, double latitude, double longitude, double speed, double course) {
    QString bitstream;

    // Message Type (6 bit)
    bitstream += QString::number(1, 2).rightJustified(6, '0');

    // Repeat Indicator (2 bit)
    bitstream += "00";

    // MMSI (30 bit)
    bitstream += QString::number(mmsi, 2).rightJustified(30, '0');

    // Navigational Status (4 bit)
    bitstream += "0000";

    // Rate of Turn (8 bit)
    bitstream += QString::number(0, 2).rightJustified(8, '0');

    // Speed over Ground (10 bit)
    bitstream += QString::number(static_cast<int>(speed * 10), 2).rightJustified(10, '0');

    // Position Accuracy (1 bit)
    bitstream += "1";

    // Longitude (28 bit, dalam 1/600000 derajat)
    int lonEncoded = static_cast<int>(longitude * 600000);
    if (lonEncoded < 0) lonEncoded += (1 << 28); // Handle signed encoding
    bitstream += QString::number(lonEncoded, 2).rightJustified(28, '0');

    // Latitude (27 bit, dalam 1/600000 derajat)
    int latEncoded = static_cast<int>(latitude * 600000);
    if (latEncoded < 0) latEncoded += (1 << 27); // Handle signed encoding
    bitstream += QString::number(latEncoded, 2).rightJustified(27, '0');

    // Course over Ground (12 bit)
    bitstream += QString::number(static_cast<int>(course * 10), 2).rightJustified(12, '0');

    // True Heading (9 bit)
    bitstream += QString::number(static_cast<int>(course * 10), 2).rightJustified(9, '0');

    // Time Stamp (6 bit)
    bitstream += QString::number(0, 2).rightJustified(6, '0');

    // Maneuver Indicator (2 bit)
    bitstream += "00";

    // Spare Bits (3 bit)
    bitstream += "000";

    // RAIM flag (1 bit)
    bitstream += "0";

    // Radio Status (19 bit)
    bitstream += QString::number(0, 2).rightJustified(19, '0');

    // Konversi bitstream ke format 6-bit ASCII AIS
    QString aisPayload = binaryToAIS6Bit(bitstream);

    // Buat pesan NMEA lengkap
    QString aivdoMessage = QString("!AIVDO,1,1,,,%1,0").arg(aisPayload);

    // Tambahkan checksum NMEA
    aivdoMessage += calculateNMEAChecksum(aivdoMessage);

    return aivdoMessage;
}

// ENCODE OWNSHIP
QString AIVDOEncoder::encodeAIVDO1(double _lat, double _lon, double _cog, double _sog, double _hdg, int _timestamp, int _type) {
    QString bitstream;

    // Message Type (1, 2, 3)
    bitstream += QString::number(_type, 2).rightJustified(6, '0');

    // Repeat Indicator
    bitstream += "00";

    // MMSI (biasanya 0 untuk Own Ship dummy)
    bitstream += QString::number(0, 2).rightJustified(30, '0');

    // Navigational Status
    bitstream += "0000";  // default: under way using engine

    // Rate of Turn (ROT)
    int rot = 128; // Not available
    bitstream += QString::number(rot, 2).rightJustified(8, '0');

    // Speed Over Ground
    int sog = _sog < 102.3 ? static_cast<int>(_sog * 10) : 1023; // 1023 = not available
    bitstream += QString::number(sog, 2).rightJustified(10, '0');

    // Position Accuracy
    bitstream += "0"; // default low accuracy

    // Longitude
    int lon = static_cast<int>(_lon * 600000);
    if (lon < 0) lon += (1 << 28);
    bitstream += QString::number(lon, 2).rightJustified(28, '0');

    // Latitude
    int lat = static_cast<int>(_lat * 600000);
    if (lat < 0) lat += (1 << 27);
    bitstream += QString::number(lat, 2).rightJustified(27, '0');

    // Course Over Ground
    int cog = _cog < 360.0 ? static_cast<int>(_cog * 10) : 3600;
    bitstream += QString::number(cog, 2).rightJustified(12, '0');

    // True Heading
    int hdg = _hdg < 360 ? static_cast<int>(_hdg) : 511;
    bitstream += QString::number(hdg, 2).rightJustified(9, '0');

    // Timestamp
    bitstream += QString::number(_timestamp, 2).rightJustified(6, '0');

    // Maneuver Indicator
    bitstream += "00"; // Not available

    // Spare
    bitstream += "000";

    // RAIM
    bitstream += "0";

    // Radio Status
    bitstream += QString::number(0, 2).rightJustified(19, '0'); // Dummy

    bitstream = bitstream.leftJustified(168, '0');  // fix: pastikan 168 bit

    // Encode to 6-bit ASCII
    QString payload = binaryToAIS6Bit(bitstream);

    // Buat !AIVDO message
    QString msg = QString("!AIVDO,1,1,,A,%1,0").arg(payload);
    msg += calculateNMEAChecksum(msg);

    // CATATAN PENTING: QT TIDAK BISA PRINT KARAKTER < DAN >, SEHINGGA DI DEBUG TIDAK AKAN TERBACA
    /*
    QString visible = payload;
    visible.replace("<", "[LT]");
    visible.replace(">", "[GT]");
    qDebug().noquote() << "payload:" << visible;
    */

    return msg;
}

QString AIVDOEncoder::encodeAIVDM(int _type, int _mmsi, int _navStatus, int _rot, double _sog, int _posAccuracy, double _lat, double _lon, double _cog, double _hdg, int _timestamp, int _manIndicator, bool _raim, int _radioStatus) {
    QString bitstream;

    // Message Type (1, 2, atau 3)
    bitstream += QString::number(_type, 2).rightJustified(6, '0');

    // Repeat Indicator (2 bit)
    bitstream += "00";

    // MMSI (30 bit)
    bitstream += QString::number(_mmsi, 2).rightJustified(30, '0');

    // Navigational Status (4 bit)
    bitstream += QString::number(_navStatus, 2).rightJustified(4, '0');

    // Rate of Turn (8 bit, signed)
    int rot = static_cast<int>(_rot); // pastikan sudah diolah ke skala AIS
    if (rot < 0)
        rot = (1 << 8) + rot;
    bitstream += QString::number(rot, 2).rightJustified(8, '0');

    // Speed Over Ground (10 bit)
    int sog = static_cast<int>(_sog * 10.0);
    bitstream += QString::number(sog, 2).rightJustified(10, '0');

    // Position Accuracy (1 bit)
    bitstream += _posAccuracy ? "1" : "0";

    // Longitude (28 bit)
    int lon = static_cast<int>(_lon * 600000);
    if (lon < 0) lon += (1 << 28);
    bitstream += QString::number(lon, 2).rightJustified(28, '0');

    // Latitude (27 bit)
    int lat = static_cast<int>(_lat * 600000);
    if (lat < 0) lat += (1 << 27);
    bitstream += QString::number(lat, 2).rightJustified(27, '0');

    // Course Over Ground (12 bit)
    int cog = static_cast<int>(_cog * 10.0);
    bitstream += QString::number(cog, 2).rightJustified(12, '0');

    // True Heading (9 bit)
    int hdg = static_cast<int>(hdg);
    bitstream += QString::number(hdg, 2).rightJustified(9, '0');

    // Time Stamp (6 bit)
    bitstream += QString::number(_timestamp, 2).rightJustified(6, '0');

    // Maneuver Indicator (2 bit)
    bitstream += QString::number(_manIndicator, 2).rightJustified(2, '0');

    // Spare (3 bit)
    bitstream += "000";

    // RAIM flag (1 bit)
    bitstream += _raim ? "1" : "0";

    // Radio Status (19 bit)
    bitstream += QString::number(_radioStatus, 2).rightJustified(19, '0');

    // Konversi ke 6-bit ASCII payload
    QString payload = binaryToAIS6Bit(bitstream);

    // Buat pesan NMEA lengkap
    QString nmea = QString("!AIVDM,1,1,,A,%1,0").arg(payload);
    nmea += calculateNMEAChecksum(nmea);

    return nmea;
}

QString AIVDOEncoder::encodeType4(int mmsi, QDateTime timestamp, double longitude, double latitude) {
    QString bitstream;

    bitstream += QString::number(4, 2).rightJustified(6, '0'); // Message Type 4
    bitstream += "00"; // Repeat Indicator
    bitstream += QString::number(mmsi, 2).rightJustified(30, '0');

    // UTC Date/Time
    bitstream += QString::number(timestamp.date().year(), 2).rightJustified(14, '0');
    bitstream += QString::number(timestamp.date().month(), 2).rightJustified(4, '0');
    bitstream += QString::number(timestamp.date().day(), 2).rightJustified(5, '0');
    bitstream += QString::number(timestamp.time().hour(), 2).rightJustified(5, '0');
    bitstream += QString::number(timestamp.time().minute(), 2).rightJustified(6, '0');
    bitstream += QString::number(timestamp.time().second(), 2).rightJustified(6, '0');

    bitstream += "0"; // Fix quality

    int lonEncoded = static_cast<int>(longitude * 600000);
    if (lonEncoded < 0) lonEncoded += (1 << 28);
    bitstream += QString::number(lonEncoded, 2).rightJustified(28, '0');

    int latEncoded = static_cast<int>(latitude * 600000);
    if (latEncoded < 0) latEncoded += (1 << 27);
    bitstream += QString::number(latEncoded, 2).rightJustified(27, '0');

    bitstream += "001010"; // Position accuracy, RAIM, sync, etc
    bitstream += QString(59, '0'); // Spare, GNSS type, etc.

    QString payload = binaryToAIS6Bit(bitstream);
    QString msg = QString("!AIVDM,1,1,,,%1,0").arg(payload);
    msg += calculateNMEAChecksum(msg);

    return msg;
}

QStringList AIVDOEncoder::encodeType5(int mmsi, QString callsign, QString name, int shipType, double length, double width, QString destination) {
    QString bitstream;

    bitstream += QString::number(5, 2).rightJustified(6, '0'); // Type 5
    bitstream += "00"; // Repeat
    bitstream += QString::number(mmsi, 2).rightJustified(30, '0');
    bitstream += QString::number(0, 2).rightJustified(2, '0'); // AIS Version
    bitstream += encode6bitString(callsign, 7);  // 7 chars
    bitstream += encode6bitString(name, 20);     // 20 chars
    bitstream += QString::number(shipType, 2).rightJustified(8, '0');
    bitstream += QString::number(static_cast<int>(length), 2).rightJustified(9, '0');
    bitstream += QString::number(static_cast<int>(width), 2).rightJustified(9, '0');
    bitstream += QString(30, '0'); // Position reference and ETA dummy
    bitstream += encode6bitString(destination, 20); // 20 chars
    bitstream += "0"; // DTE
    bitstream += "000000"; // Spare

    QString payload = binaryToAIS6Bit(bitstream);
    return splitPayloadToVDM(payload); // Fungsi bantu pecah payload jadi 2 fragment
}

QString AIVDOEncoder::encodeType18(int mmsi, double lat, double lon, double sog, double cog, double heading) {
    QString bitstream;

    bitstream += QString::number(18, 2).rightJustified(6, '0'); // Type 18
    bitstream += "00";
    bitstream += QString::number(mmsi, 2).rightJustified(30, '0');

    bitstream += QString(8, '0'); // Reserved
    bitstream += QString::number(static_cast<int>(sog * 10), 2).rightJustified(10, '0');
    bitstream += "1"; // Position Accuracy

    int lonEncoded = static_cast<int>(lon * 600000);
    if (lonEncoded < 0) lonEncoded += (1 << 28);
    bitstream += QString::number(lonEncoded, 2).rightJustified(28, '0');

    int latEncoded = static_cast<int>(lat * 600000);
    if (latEncoded < 0) latEncoded += (1 << 27);
    bitstream += QString::number(latEncoded, 2).rightJustified(27, '0');

    bitstream += QString::number(static_cast<int>(cog * 10), 2).rightJustified(12, '0');
    bitstream += QString::number(static_cast<int>(heading), 2).rightJustified(9, '0');
    bitstream += QString(6, '0'); // Timestamp
    bitstream += QString(8, '0'); // Class B flags & RAIM

    QString payload = binaryToAIS6Bit(bitstream);
    QString msg = QString("!AIVDM,1,1,,B,%1,0").arg(payload);
    msg += calculateNMEAChecksum(msg);

    return msg;
}

QString AIVDOEncoder::encodeType24A(int mmsi, QString name) {
    QString bitstream;
    bitstream += QString::number(24, 2).rightJustified(6, '0');
    bitstream += "00";
    bitstream += QString::number(mmsi, 2).rightJustified(30, '0');
    bitstream += "0000000000000000"; // Spare + part A
    bitstream += encode6bitString(name, 20);
    bitstream += QString(8, '0'); // Spare

    QString payload = binaryToAIS6Bit(bitstream);
    QString msg = QString("!AIVDM,1,1,,B,%1,0").arg(payload);
    msg += calculateNMEAChecksum(msg);
    return msg;
}

QString AIVDOEncoder::encodeType24B(int mmsi, QString callsign, int shipType, double length, double width) {
    QString bitstream;
    bitstream += QString::number(24, 2).rightJustified(6, '0');
    bitstream += "00";
    bitstream += QString::number(mmsi, 2).rightJustified(30, '0');
    bitstream += "0000000000000001"; // Spare + part B
    bitstream += encode6bitString(callsign, 7);
    bitstream += QString::number(shipType, 2).rightJustified(8, '0');
    bitstream += QString::number(static_cast<int>(length), 2).rightJustified(9, '0');
    bitstream += QString::number(static_cast<int>(width), 2).rightJustified(9, '0');
    bitstream += QString(30, '0'); // Position ref & spare

    QString payload = binaryToAIS6Bit(bitstream);
    QString msg = QString("!AIVDM,1,1,,B,%1,0").arg(payload);
    msg += calculateNMEAChecksum(msg);
    return msg;
}

QString AIVDOEncoder::encode6bitString(const QString &text, int maxLen) {
    QString result;
    QString truncated = text.leftJustified(maxLen).left(maxLen).toUpper();
    for (int i = 0; i < truncated.length(); ++i) {
        QChar ch = truncated.at(i);
        int val;

        if (ch == '@' || ch == ' ')
            val = 0;
        else if (ch >= 'A' && ch <= 'W')
            val = ch.toLatin1() - 'A' + 1;
        else if (ch >= '0' && ch <= '9')
            val = ch.toLatin1() - '0' + 48;
        else if (ch >= 'X' && ch <= 'Z')
            val = ch.toLatin1() - 'X' + 33;
        else
            val = 0; // unknown char to @ (0)

        result += QString::number(val, 2).rightJustified(6, '0');
    }
    return result;
}

QStringList AIVDOEncoder::splitPayloadToVDM(const QString &payload) {
    QStringList result;

    int maxPayloadPerFragment = 62;  // sesuai spesifikasi NMEA
    int totalFragments = (payload.length() + maxPayloadPerFragment - 1) / maxPayloadPerFragment;

    QString messageId = ""; // kosongkan jika tak pakai message ID
    QString channel = "A";  // default channel

    for (int i = 0; i < totalFragments; ++i) {
        QString fragmentPayload = payload.mid(i * maxPayloadPerFragment, maxPayloadPerFragment);

        QString sentence = QString("!AIVDM,%1,%2,%3,%4,%5,0")
                               .arg(totalFragments)          // total fragments
                               .arg(i + 1)                   // current fragment number
                               .arg(messageId)               // sequential message ID (kosongkan)
                               .arg(channel)                 // radio channel
                               .arg(fragmentPayload);        // actual payload

        sentence += calculateNMEAChecksum(sentence);
        result << sentence;
    }

    return result;
}

/// DECODERRRRR /////////

AisDecoded AIVDOEncoder::decodeNMEALine(const QString &line) {
    AisDecoded result;

    if (!line.startsWith("!AIVDM") && !line.startsWith("!AIVDO"))
        return result;

    QStringList parts = line.split(',');
    if (parts.length() < 6)
        return result;

    result.source = line.startsWith("!AIVDO") ? "AIVDO" : "AIVDM";

    QString payload = parts[5];
    QString bin = sixbitToBinary(payload);

    if (bin.length() < 168) // minimum bits for message type 1/2/3
        return result;

    result.type = bin.mid(0, 6).toInt(nullptr, 2);

    // Handle MMSI (bit 8–37, 30 bit)
    result.mmsi = bin.mid(8, 30).toInt(nullptr, 2);
    if (result.source == "AIVDO" && result.mmsi == 0) {
        result.mmsi = -1; // mark as ownship with unknown MMSI
    }

    switch (result.type) {
    case 1:
    case 2:
    case 3:
        result.data.navStatus = bin.mid(38, 4).toInt(nullptr, 2);
        result.data.rot = decodeSigned(bin.mid(42, 8), 8);
        result.data.sog = bin.mid(50, 10).toInt(nullptr, 2) / 10.0;
        result.data.posAcc = bin.mid(60, 1) == "1";
        result.data.longitude = decodeSigned(bin.mid(61, 28), 28) / 600000.0;
        result.data.latitude  = decodeSigned(bin.mid(89, 27), 27) / 600000.0;
        result.data.cog = bin.mid(116, 12).toInt(nullptr, 2) / 10.0;
        result.data.heading = bin.mid(128, 9).toInt(nullptr, 2);
        result.data.timestamp = bin.mid(137, 6).toInt(nullptr, 2);
        result.data.maneuverIndicator = bin.mid(143, 2).toInt(nullptr, 2);
        result.data.raim = bin.mid(148, 1) == "1";
        result.data.radioStatus = bin.mid(149, 19).toInt(nullptr, 2);
        break;

    case 4:
        result.data.longitude = decodeSigned(bin.mid(79, 28), 28) / 600000.0;
        result.data.latitude  = decodeSigned(bin.mid(107, 27), 27) / 600000.0;
        break;

    case 5:
        // Extended message, typically requires multipart handling
        break;

    case 18:
        result.data.sog = bin.mid(46, 10).toInt(nullptr, 2) / 10.0;
        result.data.longitude = decodeSigned(bin.mid(57, 28), 28) / 600000.0;
        result.data.latitude  = decodeSigned(bin.mid(85, 27), 27) / 600000.0;
        result.data.cog = bin.mid(112, 12).toInt(nullptr, 2) / 10.0;
        result.data.heading = bin.mid(124, 9).toInt(nullptr, 2);
        break;

    case 24: {
        result.partNumber = bin.mid(38, 8).toInt(nullptr, 2);
        if (result.partNumber == 0) {
            result.name = decode6bitToString(bin.mid(46, 120));
        } else if (result.partNumber == 1) {
            result.callsign = decode6bitToString(bin.mid(46, 42));
            result.shipType = bin.mid(88, 8).toInt(nullptr, 2);
            int dimA = bin.mid(96, 9).toInt(nullptr, 2);
            int dimB = bin.mid(105, 9).toInt(nullptr, 2);
            int dimC = bin.mid(114, 6).toInt(nullptr, 2);
            int dimD = bin.mid(120, 6).toInt(nullptr, 2);
            result.length = dimA + dimB;
            result.width  = dimC + dimD;
        }
        break;
    }

    default:
        break;
    }

    return result;
}

int AIVDOEncoder::decodeSigned(const QString &bits, int len) {
    if (bits.length() != len) return 0;

    bool ok;
    int value = bits.toInt(&ok, 2);
    if (!ok) return 0;

    // Interpretasi sebagai signed integer
    if (bits[0] == '1')
        value -= (1 << len);

    return value;
}

QString AIVDOEncoder::decode6bitToString(const QString &bitstream) {
    QString result;

    for (int i = 0; i + 6 <= bitstream.length(); i += 6) {
        QString sixBits = bitstream.mid(i, 6);
        bool ok;
        int value = sixBits.toInt(&ok, 2);
        if (!ok) continue;

        // AIS 6-bit ASCII mapping
        char c;
        if (value < 32)
            c = value + 64;
        else
            c = value;

        if (c == '@') c = ' ';
        result += QChar(c);
    }

    return result.trimmed();
}

QString AIVDOEncoder::sixbitToBinary(const QString &payload) {
    QString binary;

    for (QChar ch : payload) {
        int ascii = ch.toLatin1();
        int val;

        if (ascii >= 48 && ascii <= 87) {
            val = ascii - 48;
        } else if (ascii >= 88 && ascii <= 119) {
            val = ascii - 56;
        } else {
            continue; // karakter tidak valid
        }

        binary += QString::number(val, 2).rightJustified(6, '0');
    }

    return binary;
}
