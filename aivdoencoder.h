#ifndef AIVDOENCODER_H
#define AIVDOENCODER_H

#include <QString>
#include <QBitArray>

class AIVDOEncoder {
public:
    static QString encodeAIVDO(int mmsi, double latitude, double longitude, double speed, double course);

private:
    static QString calculateNMEAChecksum(const QString &sentence);
    static QString binaryToAIS6Bit(const QString &bitstream);
};

#endif // AIVDOENCODER_H
