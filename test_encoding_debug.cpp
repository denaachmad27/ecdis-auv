#include <QCoreApplication>
#include <QDebug>
#include <QtSql>
#include "aisdatabasemanager.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    // Open database
    AisDatabaseManager::instance().initialize("nmea_data.db");

    // Get today's targets
    QDateTime today = QDateTime::currentDateTime();
    QList<AisDatabaseManager::TargetData> targets = AisDatabaseManager::instance().getTargetsForDate(today);

    qDebug() << "Found" << targets.size() << "targets for today";

    for (int i = 0; i < targets.size() && i < 3; ++i) {
        const auto& target = targets[i];
        qDebug() << "\n=== Target" << (i+1) << "===";
        qDebug() << "MMSI:" << target.mmsi;
        qDebug() << "Name:" << target.vesselName;
        qDebug() << "Position:" << target.latitude << "," << target.longitude;
        qDebug() << "SOG:" << target.sog << "knots";
        qDebug() << "COG:" << target.cog << "degrees";
        qDebug() << "Heading:" << target.heading << "degrees";

        // Test encoding this target
        AIVDOEncoder encoder;
        QString nmea = encoder.encodeAIVDM(
            1, // Type 1
            target.mmsi,
            0, // nav status
            0, // ROT
            target.sog,
            1, // pos accuracy
            target.latitude,
            target.longitude,
            target.cog,
            static_cast<int>(target.heading),
            60, // timestamp
            0, // maneuver
            false, // RAIM
            0 // radio status
        );

        qDebug() << "Encoded NMEA:" << nmea;

        // Test decoding back
        AisDecoder::AISData decoded = AisDecoder::decodeAIVDM(nmea);
        qDebug() << "Decoded SOG:" << decoded.sog;
        qDebug() << "Decoded COG:" << decoded.cog;
        qDebug() << "Decoded Heading:" << decoded.heading;

        // Compare
        qDebug() << "SOG match:" << (qAbs(target.sog - decoded.sog) < 0.1);
        qDebug() << "COG match:" << (qAbs(target.cog - decoded.cog) < 1.0);
        qDebug() << "Heading match:" << (qAbs(target.heading - decoded.heading) < 1.0);
    }

    return 0;
}