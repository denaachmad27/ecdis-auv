#include "logger.h"
#include <QTextStream>
#include <QDateTime>
#include <QDir>

QTextEdit *Logger::textEdit = nullptr;
QFile Logger::logFile;
QString Logger::currentDate = QDate::currentDate().toString("yyyy-MM-dd");

Logger::Logger(QTextEdit *widget, QObject *parent)
    : QObject(parent)
{
    textEdit = widget;

    // Buat folder "logs" jika belum ada
    QDir dir;
    if (!dir.exists("logs")) {
        dir.mkdir("logs");
    }

    // Set file log berdasarkan tanggal hari ini
    logFile.setFileName(QString("logs/log_%1.txt").arg(currentDate));
    logFile.open(QIODevice::Append | QIODevice::Text);

    // Pasang custom handler untuk qDebug(), qWarning(), dst.
    qInstallMessageHandler(Logger::customHandler);
}

void Logger::rotateLogFileIfNeeded()
{
    QString today = QDate::currentDate().toString("yyyy-MM-dd");

    if (today != currentDate) {
        // Tutup file lama
        if (logFile.isOpen())
            logFile.close();

        // Update tanggal
        currentDate = today;

        // Set nama file baru dan buka
        logFile.setFileName(QString("logs/log_%1.txt").arg(currentDate));
        logFile.open(QIODevice::Append | QIODevice::Text);
    }
}

void Logger::customHandler(QtMsgType type, const QMessageLogContext &, const QString &msg)
{
    rotateLogFileIfNeeded();  // Cek apakah perlu buat file log baru

    QString level;
    QString color;

    switch (type) {
    case QtDebugMsg:
        level = "DEBUG";
        color = "#000000";  // hitam
        break;
    case QtWarningMsg:
        level = "WARNING";
        color = "#FFA500";  // oranye
        break;
    case QtCriticalMsg:
        level = "ERROR";
        color = "#FF0000";  // merah terang
        break;
    case QtFatalMsg:
        level = "FATAL";
        color = "#8B0000";  // merah gelap
        break;
    }

    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    QString plainText = QString("[%1] %2: %3").arg(timestamp, level, msg);
    QString htmlText = QString("<span style=\"color:%1;\">%2</span>").arg(color, plainText);

    // Tampilkan di QTextEdit jika tersedia
    if (textEdit) {
        QMetaObject::invokeMethod(textEdit, "append", Qt::QueuedConnection, Q_ARG(QString, htmlText));
    }

    // Tulis ke file log
    if (logFile.isOpen()) {
        QTextStream out(&logFile);
        out << plainText << "\n";
        out.flush();
    }

    if (type == QtFatalMsg)
        abort();  // fatal error
}
