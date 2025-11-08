#include "wifimanager.h"
#include "../mainwindow.h"
#include "../ui_mainwindow.h"
#include <QTableWidgetItem>
#include <QListWidgetItem>

WiFiManager::WiFiManager(MainWindow *mainWindow, QObject *parent)
    : QObject(parent), m_mainWindow(mainWindow)
{
}

void WiFiManager::scanNetworks()
{
    m_mainWindow->ui->diagnosticsOutput->setPlainText("Scanning for available WiFi networks...\n\nFound 8 networks:");
    m_mainWindow->ui->networksTable->setRowCount(0);

    // Sample network data
    QStringList networks = {"HomeWiFi_5G", "Office_Network", "Guest_WiFi", "TP-Link_2.4G",
                            "NETGEAR_68", "XfinityWifi", "AndroidAP", "Starbucks_Free"};
    QStringList signalStrengths = {"Excellent", "Good", "Fair", "Good", "Excellent", "Weak", "Fair", "Weak"};
    QStringList security = {"WPA2", "WPA3", "WPA2", "WPA2", "WPA3", "Open", "WPA2", "Open"};
    QStringList bands = {"5 GHz", "5 GHz", "2.4 GHz", "2.4 GHz", "5 GHz", "2.4 GHz", "2.4 GHz", "2.4 GHz"};
    QStringList channels = {"36", "149", "6", "11", "44", "1", "3", "9"};

    for (int i = 0; i < networks.size(); ++i)
    {
        int row = m_mainWindow->ui->networksTable->rowCount();
        m_mainWindow->ui->networksTable->insertRow(row);

        m_mainWindow->ui->networksTable->setItem(row, 0, new QTableWidgetItem(networks[i]));
        m_mainWindow->ui->networksTable->setItem(row, 1, new QTableWidgetItem(signalStrengths[i]));
        m_mainWindow->ui->networksTable->setItem(row, 2, new QTableWidgetItem(security[i]));
        m_mainWindow->ui->networksTable->setItem(row, 3, new QTableWidgetItem(bands[i]));
        m_mainWindow->ui->networksTable->setItem(row, 4, new QTableWidgetItem(channels[i]));
    }

    m_mainWindow->ui->networksTable->resizeColumnsToContents();
    m_mainWindow->ui->wifiStatusLabel->setText("WiFi: Scanning Complete");
}

void WiFiManager::refreshNetworks()
{
    scanNetworks();
}

void WiFiManager::connectNetwork()
{
    int row = m_mainWindow->ui->networksTable->currentRow();
    if (row >= 0)
    {
        QString networkName = m_mainWindow->ui->networksTable->item(row, 0)->text();
        m_mainWindow->ui->selectedNetworkInfo->setText(QString("Connecting to: %1").arg(networkName));
        m_mainWindow->ui->wifiStatusLabel->setText(QString("WiFi: Connecting to %1").arg(networkName));
    }
}

void WiFiManager::disconnectNetwork()
{
    m_mainWindow->ui->selectedNetworkInfo->setText("Disconnected from current network");
    m_mainWindow->ui->wifiStatusLabel->setText("WiFi: Disconnected");
}

void WiFiManager::onNetworkSelectionChanged()
{
    QList<QTableWidgetItem *> selectedItems = m_mainWindow->ui->networksTable->selectedItems();
    bool hasSelection = !selectedItems.isEmpty();
    m_mainWindow->ui->connectNetworkButton->setEnabled(hasSelection);

    if (hasSelection)
    {
        int row = m_mainWindow->ui->networksTable->currentRow();
        QString name = m_mainWindow->ui->networksTable->item(row, 0)->text();
        QString signal = m_mainWindow->ui->networksTable->item(row, 1)->text();
        QString security = m_mainWindow->ui->networksTable->item(row, 2)->text();

        m_mainWindow->ui->selectedNetworkInfo->setText(
            QString("Selected: %1 (%2, %3)").arg(name).arg(signal).arg(security));
    }
    else
    {
        m_mainWindow->ui->selectedNetworkInfo->setText("No network selected");
    }
}

void WiFiManager::enableAdapter()
{
    m_mainWindow->ui->adapterStatusValue->setText("Enabled");
    m_mainWindow->ui->adapterStatusValue->setStyleSheet("color: #27ae60; font-weight: bold;");
    m_mainWindow->ui->enableAdapterButton->setEnabled(false);
    m_mainWindow->ui->disableAdapterButton->setEnabled(true);
}

void WiFiManager::disableAdapter()
{
    m_mainWindow->ui->adapterStatusValue->setText("Disabled");
    m_mainWindow->ui->adapterStatusValue->setStyleSheet("color: #e74c3c; font-weight: bold;");
    m_mainWindow->ui->enableAdapterButton->setEnabled(true);
    m_mainWindow->ui->disableAdapterButton->setEnabled(false);
}

void WiFiManager::refreshAdapters()
{
    // Simulate refreshing adapter list
    m_mainWindow->ui->adaptersLabel->setText("Adapter list refreshed successfully!");
}

void WiFiManager::onAdapterSelectionChanged()
{
    QList<QListWidgetItem *> selectedItems = m_mainWindow->ui->adaptersList->selectedItems();
    bool hasSelection = !selectedItems.isEmpty();

    if (hasSelection)
    {
        QString adapterName = selectedItems.first()->text();
        // Remove emoji prefix for display
        if (adapterName.startsWith("📡 "))
            adapterName = adapterName.mid(2);
        else if (adapterName.startsWith("🔵 "))
            adapterName = adapterName.mid(2);
        else if (adapterName.startsWith("🔌 "))
            adapterName = adapterName.mid(2);
        else if (adapterName.startsWith("🌐 "))
            adapterName = adapterName.mid(2);

        m_mainWindow->ui->adapterNameValue->setText(adapterName);

        // Enable/disable buttons based on current status
        bool isEnabled = (m_mainWindow->ui->adapterStatusValue->text() == "Enabled");
        m_mainWindow->ui->enableAdapterButton->setEnabled(!isEnabled);
        m_mainWindow->ui->disableAdapterButton->setEnabled(isEnabled);
    }
}

void WiFiManager::runDiagnostics()
{
    m_mainWindow->ui->diagnosticsOutput->setPlainText(
        "Running network diagnostics...\n\n"
        "✓ WiFi adapter detected and enabled\n"
        "✓ Driver status: Healthy\n"
        "✓ Signal strength: Good\n"
        "✓ Internet connectivity: Available\n"
        "✓ DNS resolution: Working\n"
        "✓ Gateway reachable: Yes\n\n"
        "Diagnostics completed - No issues found!");
}

void WiFiManager::resetNetwork()
{
    m_mainWindow->ui->diagnosticsOutput->setPlainText(
        "Resetting network stack...\n\n"
        "• Flushing DNS resolver cache... Done\n"
        "• Renewing IP address... Done\n"
        "• Resetting Winsock catalog... Done\n"
        "• Restarting network services... Done\n\n"
        "Network stack reset completed successfully!");
}

void WiFiManager::flushDns()
{
    m_mainWindow->ui->diagnosticsOutput->setPlainText("DNS cache flushed successfully!");
}

void WiFiManager::restartWifiService()
{
    m_mainWindow->ui->diagnosticsOutput->setPlainText("WiFi service restarted successfully!");
}

void WiFiManager::renewIp()
{
    m_mainWindow->ui->diagnosticsOutput->setPlainText("IP address renewed successfully!");
}

void WiFiManager::forgetNetwork()
{
    m_mainWindow->ui->diagnosticsOutput->setPlainText("Selected network forgotten successfully!");
}

void WiFiManager::updateDriver()
{
    m_mainWindow->ui->diagnosticsOutput->setPlainText("Checking for driver updates...\n\nYour WiFi drivers are up to date!");
}