#include "testpanel.h"

TestPanel::TestPanel(QWidget *parent)
    : QWidget(parent)
    , m_clickCount(0)
{
    // Set window properties
    setWindowTitle("TEST PANEL - SIMPLE");
    setMinimumSize(300, 200);

    // Create layout
    QVBoxLayout *layout = new QVBoxLayout(this);
    setLayout(layout);

    // Title
    m_title = new QLabel("🔥 TEST PANEL WORKING 🔥", this);
    m_title->setStyleSheet("font-size: 18px; font-weight: bold; color: white; background: red; padding: 10px; text-align: center;");
    m_title->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_title);

    // Status
    m_status = new QLabel("Status: Panel initialized successfully!", this);
    m_status->setStyleSheet("font-size: 14px; background: yellow; color: black; padding: 8px; border: 2px solid black;");
    m_status->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_status);

    // Button
    m_button = new QPushButton("CLICK ME TO TEST", this);
    m_button->setStyleSheet("font-size: 16px; font-weight: bold; background: green; color: white; padding: 15px; border: none;");
    layout->addWidget(m_button);

    // Connect signal
    connect(m_button, &QPushButton::clicked, this, &TestPanel::onButtonClicked);
}

TestPanel::~TestPanel()
{
}

void TestPanel::onButtonClicked()
{
    m_clickCount++;
    m_status->setText(QString("Button clicked %1 times! ✅").arg(m_clickCount));

    // Change colors each click
    if (m_clickCount % 2 == 1) {
        m_title->setStyleSheet("font-size: 18px; font-weight: bold; color: black; background: lime; padding: 10px; text-align: center;");
    } else {
        m_title->setStyleSheet("font-size: 18px; font-weight: bold; color: white; background: blue; padding: 10px; text-align: center;");
    }
}