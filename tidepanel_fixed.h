#ifndef TIDEPANEL_H
#define TIDEPANEL_H

#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QListWidget>
#include <QTimer>

class TidePanel : public QWidget
{
    Q_OBJECT

public:
    explicit TidePanel(QWidget *parent = nullptr);
    ~TidePanel();

private slots:
    void onTestClicked();

private:
    QVBoxLayout *m_layout;
    QLabel *m_title;
    QLabel *m_status;
    QListWidget *m_list;
    QPushButton *m_button;
};

#endif