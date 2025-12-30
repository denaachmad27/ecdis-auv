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
    ~Logger(); // CRITICAL: Add destructor to prevent crash

    static void customHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg);

    // CRITICAL: Add cleanup method for safe shutdown
    static void cleanup();

private:
    static QTextEdit *textEdit;
    static QFile logFile;
    static QString currentDate;

    static void rotateLogFileIfNeeded();
};

#endif // LOGGER_H
