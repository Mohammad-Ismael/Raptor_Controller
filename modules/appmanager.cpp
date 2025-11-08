#include "appmanager.h"
#include "../mainwindow.h"
#include "../ui_mainwindow.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>

AppManager::AppManager(MainWindow *mainWindow, QObject *parent)
    : QObject(parent), m_mainWindow(mainWindow)
{
}

void AppManager::addApp()
{
    // Placeholder for adding app functionality
    m_mainWindow->ui->appsLabel->setText("Add App dialog would open here to browse for applications");
}

void AppManager::refreshApps()
{
    // Clear existing apps
    QLayoutItem *item;
    while ((item = m_mainWindow->ui->appsGridLayout->takeAt(0)) != nullptr)
    {
        delete item->widget();
        delete item;
    }

    // Sample favorite apps with emojis
    QVector<QPair<QString, QString>> apps = {
        {"🌐 Chrome", "Web Browser"},
        {"📝 VS Code", "Code Editor"},
        {"🎵 Spotify", "Music Player"},
        {"💬 Discord", "Chat App"},
        {"🖼️ Photoshop", "Image Editor"},
        {"📊 Excel", "Spreadsheet"},
        {"🎮 Steam", "Game Platform"},
        {"📧 Outlook", "Email Client"},
        {"📁 Explorer", "File Manager"},
        {"🎥 VLC", "Media Player"},
        {"📚 Adobe Reader", "PDF Viewer"},
        {"⚙️ Settings", "System Settings"}};

    int row = 0;
    int col = 0;
    const int maxCols = 4;

    for (const auto &app : apps)
    {
        QPushButton *appButton = new QPushButton();
        appButton->setFixedSize(80, 90);
        appButton->setStyleSheet(
            "QPushButton {"
            "    background-color: white;"
            "    border: 2px solid #ecf0f1;"
            "    border-radius: 10px;"
            "    padding: 5px;"
            "}"
            "QPushButton:hover {"
            "    border-color: #1abc9c;"
            "    background-color: #f1f8f6;"
            "}"
            "QPushButton:pressed {"
            "    background-color: #1abc9c;"
            "    color: white;"
            "}");

        QVBoxLayout *buttonLayout = new QVBoxLayout(appButton);
        buttonLayout->setSpacing(5);
        buttonLayout->setContentsMargins(5, 5, 5, 5);

        QLabel *iconLabel = new QLabel(app.first.split(' ')[0]); // Get emoji part
        iconLabel->setAlignment(Qt::AlignCenter);
        iconLabel->setStyleSheet("font-size: 24px; background: transparent;");

        QLabel *nameLabel = new QLabel(app.first.split(' ')[1]); // Get name part
        nameLabel->setAlignment(Qt::AlignCenter);
        nameLabel->setStyleSheet(
            "font-size: 9px;"
            "color: #2c3e50;"
            "background: transparent;"
            "font-weight: bold;");

        QLabel *descLabel = new QLabel(app.second);
        descLabel->setAlignment(Qt::AlignCenter);
        descLabel->setStyleSheet(
            "font-size: 7px;"
            "color: #7f8c8d;"
            "background: transparent;");

        buttonLayout->addWidget(iconLabel);
        buttonLayout->addWidget(nameLabel);
        buttonLayout->addWidget(descLabel);

        // Connect button click
        connect(appButton, &QPushButton::clicked, this, [this, app]()
                {
            m_mainWindow->ui->selectedAppName->setText(app.first);
            m_mainWindow->ui->selectedAppPath->setText(app.second);
            m_mainWindow->ui->selectedAppIcon->setText(app.first.split(' ')[0]);
            m_mainWindow->ui->launchAppButton->setEnabled(true);
            m_mainWindow->ui->removeAppButton->setEnabled(true); });

        m_mainWindow->ui->appsGridLayout->addWidget(appButton, row, col);

        col++;
        if (col >= maxCols)
        {
            col = 0;
            row++;
        }
    }

    // Add stretch to push items to top
    m_mainWindow->ui->appsGridLayout->setRowStretch(row + 1, 1);
    m_mainWindow->ui->appsGridLayout->setColumnStretch(maxCols, 1);
}

void AppManager::launchApp()
{
    QString appName = m_mainWindow->ui->selectedAppName->text();
    if (appName != "No app selected")
    {
        m_mainWindow->ui->appsLabel->setText(QString("Launching: %1").arg(appName));
        // In real implementation, this would launch the actual application
    }
}

void AppManager::removeApp()
{
    QString appName = m_mainWindow->ui->selectedAppName->text();
    if (appName != "No app selected")
    {
        m_mainWindow->ui->appsLabel->setText(QString("Removed from favorites: %1").arg(appName));
        m_mainWindow->ui->selectedAppName->setText("No app selected");
        m_mainWindow->ui->selectedAppPath->setText("Select an app to view details");
        m_mainWindow->ui->selectedAppIcon->setText("🚀");
        m_mainWindow->ui->launchAppButton->setEnabled(false);
        m_mainWindow->ui->removeAppButton->setEnabled(false);
    }
}

void AppManager::searchApps(const QString &searchText)
{
    // Simple search functionality
    for (int i = 0; i < m_mainWindow->ui->appsGridLayout->count(); ++i)
    {
        QWidget *widget = m_mainWindow->ui->appsGridLayout->itemAt(i)->widget();
        if (widget)
        {
            QPushButton *appButton = qobject_cast<QPushButton *>(widget);
            if (appButton)
            {
                // Get the app name from the button's layout
                QVBoxLayout *layout = qobject_cast<QVBoxLayout *>(appButton->layout());
                if (layout && layout->itemAt(1))
                {
                    QLabel *nameLabel = qobject_cast<QLabel *>(layout->itemAt(1)->widget());
                    if (nameLabel)
                    {
                        bool match = searchText.isEmpty() ||
                                     nameLabel->text().contains(searchText, Qt::CaseInsensitive);
                        appButton->setVisible(match);
                    }
                }
            }
        }
    }
}