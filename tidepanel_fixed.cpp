#include "tidepanel.h"
#include <QDebug>

TidePanel::TidePanel(QWidget *parent)
    : QWidget(parent)
{
    qDebug() << "[TIDE] CONSTRUCTOR STARTED";

    // MOST BASIC SETUP POSSIBLE
    setWindowTitle("Tide Panel");
    resize(400, 300);

    // Create layout
    m_layout = new QVBoxLayout(this);
    setLayout(m_layout);

    qDebug() << "[TIDE] Layout created:" << (m_layout != nullptr);

    // Create widgets
    m_title = new QLabel("=== TIDE PREDICTIONS ===", this);
    m_status = new QLabel("Status: Ready", this);
    m_list = new QListWidget(this);
    m_button = new QPushButton("Test Button", this);

    qDebug() << "[TIDE] Widgets created";

    // Add to layout
    m_layout->addWidget(m_title);
    m_layout->addWidget(m_status);
    m_layout->addWidget(m_list);
    m_layout->addWidget(m_button);

    qDebug() << "[TIDE] Widgets added to layout";

    // Add test items
    m_list->addItem("Test Item 1");
    m_list->addItem("Test Item 2");

    // Connect button
    connect(m_button, &QPushButton::clicked, this, &TidePanel::onTestClicked);

    qDebug() << "[TIDE] CONSTRUCTOR FINISHED";
    qDebug() << "[TIDE] Title visible:" << m_title->isVisible();
    qDebug() << "[TIDE] Layout children count:" << m_layout->count();
}

TidePanel::~TidePanel()
{
    qDebug() << "[TIDE] DESTRUCTOR";
}

void TidePanel::onTestClicked()
{
    m_status->setText("Button clicked! " + QString::number(m_list->count()) + " items");
    m_list->addItem("New item " + QString::number(m_list->count() + 1));
    qDebug() << "[TIDE] Button clicked";
}