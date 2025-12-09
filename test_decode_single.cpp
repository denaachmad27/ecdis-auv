#include <QCoreApplication>
#include <QDebug>
#include "aivdoencoder.h"
#include "aisdecoder.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    QString nmea = "!AIVDM,1,1,,B,17lcM@0000846L5spFLLL2LJ04CL,0*54";

    qDebug() << "Original NMEA:" << nmea;

    // Decode using our decoder
    AisDecoder::AISData decoded = AisDecoder::decodeAIVDM(nmea);

    qDebug() << "\n=== Decoded Data ===";
    qDebug() << "Message Type:" << decoded.type;
    qDebug() << "MMSI:" << decoded.mmsi;
    qDebug() << "Latitude:" << decoded.latitude;
    qDebug() << "Longitude:" << decoded.longitude;
    qDebug() << "SOG:" << decoded.sog;
    qDebug() << "COG:" << decoded.cog;
    qDebug() << "True Heading:" << decoded.heading;
    qDebug() << "Navigation Status:" << decoded.navStatus;
    qDebug() << "Position Accuracy:" << decoded.posAccuracy;

    // Manual decode to verify
    qDebug() << "\n=== Manual Binary Analysis ===";

    // Extract payload
    int start = nmea.indexOf(',') + 1;
    int end = nmea.indexOf(',', start);
    int comma1 = start;
    int comma2 = nmea.indexOf(',', comma1 + 1);
    int comma3 = nmea.indexOf(',', comma2 + 1);
    int comma4 = nmea.indexOf(',', comma3 + 1);
    int comma5 = nmea.indexOf(',', comma4 + 1);

    QString payload = nmea.mid(comma4 + 1, comma5 - comma4 - 1);
    qDebug() << "Payload:" << payload;

    // Convert to 6-bit binary
    QString binary;
    for (int i = 0; i < payload.length(); i++) {
        int val = payload[i].toLatin1() - 48;
        if (val > 40) val -= 8;
        QString bits = QString::number(val, 2).rightJustified(6, '0');
        binary += bits;
    }

    qDebug() << "Binary length:" << binary.length() << "bits";
    qDebug() << "Binary:" << binary.left(168); // Show first 168 bits (Type 1 required)

    // Parse Type 1 fields manually
    if (binary.length() >= 168) {
        int pos = 0;

        // Message Type (6 bits)
        QString typeBits = binary.mid(pos, 6);
        int msgType = typeBits.toInt(nullptr, 2);
        qDebug() << "\nType:" << msgType << "(bits:" << typeBits << ")";
        pos += 6;

        // Repeat Indicator (2 bits)
        pos += 2;

        // MMSI (30 bits)
        QString mmsiBits = binary.mid(pos, 30);
        int mmsi = mmsiBits.toInt(nullptr, 2);
        qDebug() << "MMSI:" << mmsi << "(bits:" << mmsiBits << ")";
        pos += 30;

        // Navigation Status (4 bits)
        QString navBits = binary.mid(pos, 4);
        int navStatus = navBits.toInt(nullptr, 2);
        qDebug() << "Nav Status:" << navStatus << "(bits:" << navBits << ")";
        pos += 4;

        // Rate of Turn (8 bits)
        pos += 8;

        // Speed Over Ground (10 bits)
        QString sogBits = binary.mid(pos, 10);
        int sogRaw = sogBits.toInt(nullptr, 2);
        double sog = sogRaw / 10.0;
        qDebug() << "SOG:" << sog << "knots (raw:" << sogRaw << ", bits:" << sogBits << ")";
        pos += 10;

        // Position Accuracy (1 bit)
        pos += 1;

        // Longitude (28 bits)
        pos += 28;

        // Latitude (27 bits)
        pos += 27;

        // Course Over Ground (12 bits)
        QString cogBits = binary.mid(pos, 12);
        int cogRaw = cogBits.toInt(nullptr, 2);
        double cog = cogRaw / 10.0;
        qDebug() << "COG:" << cog << "° (raw:" << cogRaw << ", bits:" << cogBits << ")";
        pos += 12;

        // True Heading (9 bits)
        QString hdgBits = binary.mid(pos, 9);
        int hdgRaw = hdgBits.toInt(nullptr, 2);
        qDebug() << "True Heading:" << hdgRaw << "° (bits:" << hdgBits << ")";

        // Check if heading is 511 (not available)
        if (hdgRaw == 511) {
            qDebug() << "*** HEADING = 511 (Not Available) ***";
        }

        pos += 9;

        // Time Stamp (6 bits)
        QString tsBits = binary.mid(pos, 6);
        int ts = tsBits.toInt(nullptr, 2);
        qDebug() << "Timestamp:" << ts << "(bits:" << tsBits << ")";
        if (ts == 60) {
            qDebug() << "*** TIMESTAMP = 60 (Not Available) ***";
        }
    }

    return 0;
}