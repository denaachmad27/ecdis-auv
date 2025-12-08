#ifndef AIVDOENCODER_H
#define AIVDOENCODER_H

#include <QString>
#include <QBitArray>
#include <QDateTime>

struct AISData {
    // Umum
    int type = 0;
    int mmsi = 0;

    // Posisi dan gerakan (type 1,2,3,18)
    double latitude = 91.0;    // 91.0 = not available
    double longitude = 181.0;
    double cog = 360.0;        // course over ground
    double sog = 102.3;        // speed over ground
    int heading = 511;         // true heading
    int rot = -128;            // rate of turn (type 1-3)
    int navStatus = 15;        // 15 = not defined
    int posAcc = 0;            // position accuracy
    int timestamp = 60;        // 60 = not available
    int maneuverIndicator = 0;
    bool raim = false;
    int radioStatus = 0;

    // Static info (type 5, 24)
    QString name;
    QString callsign;
    QString destination;
    int shipType = 0;

    // Dimensi kapal (type 5, 24)
    double length = 0.0;
    double width = 0.0;
    double dimA = 0.0;  // bow to GPS
    double dimB = 0.0;  // stern to GPS
    double dimC = 0.0;  // port to GPS
    double dimD = 0.0;  // starboard to GPS

    // Type 4 timestamp
    QDateTime utc;
};

struct AisDecoded {
    QString source;     // "AIVDO" / "AIVDM"
    int type;           // AIS message type (1–27)
    int mmsi;
    AISData data;       // Semua field seperti lat, lon, sog, dll
    int partNumber;     // untuk type 24
    QString callsign;
    QString name;
    QString destination;
    int shipType;
    double length;
    double width;
};

class AIVDOEncoder {
public:
    // ENCODER
    static QString encodeAIVDO(int mmsi, double latitude, double longitude, double speed, double course);
    static QString encodeAIVDO1(double _lat, double _lon, double _cog, double _sog, double _hdg, int _timestamp, int _type);
    static QString encodeAIVDM(int _type, int _mmsi, int _navStatus, int _rot, double _sog, int _posAccuracy, double _lat, double _lon, double _cog, double _hdg, int _timestamp, int _manIndicator, bool _raim, int _radioStatus);
    static QString encodeType4(int mmsi, QDateTime timestamp, double longitude, double latitude);
    static QStringList encodeType5(int mmsi, QString callsign, QString name, int shipType, double length, double width, QString destination);
    static QStringList encodeVesselNameType5(int mmsi, const QString &vesselName);
    static QStringList encodeVesselNameType5(int mmsi, const QString &callSign, const QString &vesselName);

  // Custom encoder for Type 5 that preserves spaces properly
  static QStringList encodeType5V2(int mmsi, QString callsign, QString name, int shipType, double length, double width, QString destination);
    static QString encodeType18(int mmsi, double lat, double lon, double sog, double cog, double heading);
    static QString encodeType24A(int mmsi, QString name);
    static QString encodeType24B(int mmsi, QString callsign, int shipType, double length, double width);


    // DECODER
    static AisDecoded decodeNMEALine(const QString &line);
    static QString decode6bitToString(const QString &bitstream);
    static int decodeSigned(const QString &bits, int len);
    static QString sixbitToBinary(const QString &payload);

    // DEBUG METHODS
    static QString testEncode6bitString(const QString &text, int maxLen);

    private:
    static QStringList splitPayloadToVDM(const QString &payload);
    static QString encode6bitString(const QString &text, int maxLen);
    static QString calculateNMEAChecksum(const QString &sentence);
    static QString binaryToAIS6Bit(const QString &bitstream);
};

#endif // AIVDOENCODER_H
