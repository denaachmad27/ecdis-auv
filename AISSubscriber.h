#ifndef AISSUBSCRIBER_H
#define AISSUBSCRIBER_H

#include <QObject>
#include <QTcpSocket>
#include <QTimer>

// forward declerations1
class MainWindow;

class AISSubscriber : public QObject {
    Q_OBJECT

public:
    explicit AISSubscriber(QObject *parent = nullptr);
    //void disconnectFromHost();
    void setMainWindow(MainWindow*);
    bool dataFlag = false;
    bool hasData();
    void setDialogOpen(bool open);

signals:
    // OWNSHIP
    void navLatReceived(const double lat);
    void navLongReceived(const double lon);
    void navLatDmsReceived(const QString latDms);
    void navLongDmsReceived(const QString lonDms);
    void navLatDmmReceived(const QString latDmm);
    void navLongDmmReceived(const QString lonDmm);
    void navDepthReceived(const double depth);
    void navHeadingReceived(const double heading);
    void navHeadingOGReceived(const double headingOg);
    void navCourseOGReceived(const double courseOg);
    void navSpeedReceived(const double speed);
    void navSpeedOGReceived(const double speedOg);
    void navSOGReceived(const double sog);
    void navYawReceived(const double yaw);
    void navZReceived(const double z);
    void navStwReceived(const double stw);
    void navDriftReceived(const double drift);
    void navDraftReceived(const double drift);
    void navDriftAngleReceived(const double driftAngle);
    void navSetReceived(const double set);
    void navRotReceived(const double rot);
    void navDepthBelowKeelReceived(const double DepthBelowKeel);
    void navDeadReckonReceived(const QString deadReckon);

    void mapInfoReqReceived(QString map);
    void waisNmeaReceived(QString nmea);

    // ROUTES INFORMATION
    void rteWpBrgReceived(const double &value);
    void rteXtdReceived(const QString &value);
    void rteCrsReceived(const double &value);
    void rteCtmReceived(const double &value);
    void rteDtgReceived(const double &value);
    void rteDtgMReceived(const double &value);
    void rteTtgReceived(const QString &value);
    void rteEtaReceived(const QString &value);

    void publishToMOOSDB(const QString &key, const QString &value);

    void errorOccurred(const QString &message);
    void disconnected();

    void processingData(double, double, double, double, double, double, double, double, double);
    void processingAis(QString);

    // CONNECTION STATUS
    void connectionStatusChanged(bool connected);

    void startDrawTimer();

private slots:
    void onReadyRead();
    void onSocketError(QAbstractSocket::SocketError socketError);
    void onDisconnected();
    void onConnected();
    void tryReconnect();

public slots:
    void connectToHost(const QString &host, quint16 port);
    void disconnectFromHost();

private:
    MainWindow* mainWindow = nullptr;
    QTcpSocket *socket = nullptr;
    bool hasReceivedData = false;

    QTimer* countdownTimer;
    int countdownSeconds;

    QString lastHost;
    quint16 lastPort;

    QString formatCountdownTime(int totalSeconds);

    int reconnectAttempts;
    const int baseDelay;   // e.g., 5000 ms
    const int maxDelay;    // e.g., 3600000 ms

    bool dialogIsOpen;
};

#endif // AISSUBSCRIBER_H
