#ifndef LOGGER_H
#define LOGGER_H

#include <QObject>
#include <QTextEdit>
#include <QFile>

class Logger : public QObject {
    Q_OBJECT

public:
    Logger(QTextEdit *widget, QObject *parent = nullptr);
    static void customHandler(QtMsgType type, const QMessageLogContext &, const QString &msg);

private:
    static QTextEdit *textEdit;
    static QFile logFile;
};

#endif // LOGGER_H
