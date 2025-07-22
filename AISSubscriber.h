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
    void navLatReceived(double lat);
    void navLongReceived(double lon);
    void navDepthReceived(double depth);
    void navHeadingReceived(double heading);
    void navHeadingOGReceived(double headingOg);
    void navSpeedReceived(double speed);
    void navSpeedOGReceived(double speedOg);
    void navYawReceived(double yaw);
    void navZReceived(double z);

    void mapInfoReqReceived(QString map);
    void waisNmeaReceived(QString nmea);

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
