#ifndef LOGGER_H
#define LOGGER_H

#include <QObject>
#include <QTextEdit>
#include <QFile>

class Logger : public QObject
{
    Q_OBJECT
public:
    explicit Logger(QTextEdit *widget = nullptr, QObject *parent = nullptr);

    static void customHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg);

private:
    static QTextEdit *textEdit;
    static QFile logFile;
    static QString currentDate;

    static void rotateLogFileIfNeeded();
};

#endif // LOGGER_H
