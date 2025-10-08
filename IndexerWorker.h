#pragma once
#include <QString>
#include <QList>
#include <functional>

class IndexerWorker {
public:
    QString filePath;
    std::function<void(int)> onProgress;
    std::function<void(const QList<qint64>&)> onFinished;

    void process();
};
