#include "AIVDOEncoder.h"
#include <QDebug>
#include <QString>
#include <QByteArray>
#include <bitset>
#include <iomanip>

// AIS 6-bit encoding character set
const QString AIS_CHARSET = "@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^- !\"#$%&'()*+,-./0123456789:;<=>?";

// Fungsi menghitung checksum NMEA
QString AIVDOEncoder::calculateNMEAChecksum(const QString &sentence) {
    quint8 checksum = 0;
    for (int i = 1; i < sentence.length(); ++i) {
        checksum ^= static_cast<quint8>(sentence[i].toLatin1());
    }
    return QString("*%1").arg(checksum, 2, 16, QLatin1Char('0')).toUpper();
}

// Fungsi mengonversi bitstream ke 6-bit ASCII AIS
QString AIVDOEncoder::binaryToAIS6Bit(const QString &bitstream) {
    QString encoded;
    for (int i = 0; i < bitstream.length(); i += 6) {
        bool ok;
        int value = bitstream.mid(i, 6).toInt(&ok, 2);
        if (!ok) continue; // Cegah error parsing
        value += 48;  // Offset ASCII sesuai ITU-R M.1371
        if (value > 87) value += 8;  // Koreksi karakter di atas 87
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

// Fungsi utama untuk encoding AIS Message Type 1 (AIVDO)
QString AIVDOEncoder::encodeAIVDM(int mmsi, double latitude, double longitude, double speed, double course) {
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
    bitstream += QString::number(static_cast<int>(course), 2).rightJustified(9, '0');

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
    QString aivdoMessage = QString("!AIVDM,1,1,,,%1,0").arg(aisPayload);

    // Tambahkan checksum NMEA
    aivdoMessage += calculateNMEAChecksum(aivdoMessage);

    return aivdoMessage;
}
