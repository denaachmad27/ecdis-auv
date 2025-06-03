#include "logger.h"
#include <QTextStream>
#include <QDateTime>

QTextEdit *Logger::textEdit = nullptr;
QFile Logger::logFile;

Logger::Logger(QTextEdit *widget, QObject *parent)
    : QObject(parent)
{
    textEdit = widget;
    logFile.setFileName("log_output.txt");
    logFile.open(QIODevice::Append | QIODevice::Text);
    qInstallMessageHandler(Logger::customHandler);
}

void Logger::customHandler(QtMsgType type, const QMessageLogContext &, const QString &msg)
{
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

    QString text = QString("[%1] %2: %3")
                       .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"))
                       .arg(level)
                       .arg(msg);

    QString coloredText = QString("<span style=\"color:%1;\">%2</span>").arg(color, text);

    if (textEdit) {
        QMetaObject::invokeMethod(textEdit, "append", Qt::QueuedConnection, Q_ARG(QString, coloredText));
    }

    if (logFile.isOpen()) {
        QTextStream out(&logFile);
        out << text << "\n";
        out.flush();
    }

    if (type == QtFatalMsg)
        abort();
}
