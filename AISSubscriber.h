#ifndef AISSUBSCRIBER_H
#define AISSUBSCRIBER_H

#include <QObject>
#include <QTcpSocket>

class AISSubscriber : public QObject {
    Q_OBJECT

public:
    explicit AISSubscriber(QObject *parent = nullptr);
    void connectToHost(const QString &host, quint16 port);
    //void disconnectFromHost();

signals:
    // OWNSHIP
    void navLatReceived(const double lat);
    void navLongReceived(const double lon);
    void navDepthReceived(const double depth);
    void navHeadingReceived(const double heading);
    void navHeadingOGReceived(const double headingOg);
    void navSpeedReceived(const double speed);
    void navSpeedOGReceived(const double speedOg);
    void navYawReceived(const double yaw);
    void navZReceived(const double z);
    void navStwReceived(const double stw);
    void navDriftReceived(const double drift);
    void navDriftAngleReceived(const double driftAngle);
    void navSetReceived(const double set);
    void navRotReceived(const double rot);
    void navDepthBelowKeelReceived(const double DepthBelowKeel);

    void mapInfoReqReceived(QString map);
    void waisNmeaReceived(QString nmea);

    // ROUTES INFORMATION
    void rteWpBrgReceived(const QString &value);
    void rteXtdReceived(const QString &value);
    void rteCrsReceived(const QString &value);
    void rteCtmReceived(const QString &value);
    void rteDtgReceived(const QString &value);
    void rteDtgMReceived(const QString &value);
    void rteTtgReceived(const QString &value);
    void rteEtaReceived(const QString &value);

    void errorOccurred(const QString &message);
    void disconnected();

    void processingData(double, double, double, double, double, double, double, double, double);
    void processingAis(QString);

private slots:
    void onReadyRead();
    void onSocketError(QAbstractSocket::SocketError socketError);
    void onDisconnected();

public slots:
    void disconnectFromHost();

private:
    QTcpSocket *socket = nullptr;
};

#endif // AISSUBSCRIBER_H
