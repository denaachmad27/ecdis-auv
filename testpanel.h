#ifndef TESTPANEL_H
#define TESTPANEL_H

#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>

class TestPanel : public QWidget
{
    Q_OBJECT

public:
    explicit TestPanel(QWidget *parent = nullptr);
    ~TestPanel();

private slots:
    void onButtonClicked();

private:
    QLabel *m_title;
    QLabel *m_status;
    QPushButton *m_button;
    int m_clickCount;
};

#endif // TESTPANEL_H