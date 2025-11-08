#include "networkmanager.h"
#include "../mainwindow.h"
#include "../ui_mainwindow.h"
#include <QTimer>
#include <QTableWidgetItem>
#include <QMap>

NetworkManager::NetworkManager(MainWindow *mainWindow, QObject *parent)
    : QObject(parent), m_mainWindow(mainWindow), 
      m_pingTimer(nullptr), m_tracerouteTimer(nullptr), m_scanTimer(nullptr),
      m_pingCount(0), m_tracerouteHop(0), m_currentScanPort(0), 
      m_scanEndPort(0), m_openPortsFound(0)
{
}

NetworkManager::~NetworkManager()
{
    stopPing();
    stopTraceroute();
    stopPortScan();
}

void NetworkManager::refreshNetworkStatus()
{
    // Simulate refreshing network status
    QStringList connectionTypes = {"WiFi", "Ethernet", "Mobile", "Disconnected"};
    QStringList statuses = {"Connected", "Connecting", "Disconnected", "Limited Access"};
    QStringList internetAccess = {"Available", "Unavailable", "Limited"};

    int randomType = rand() % connectionTypes.size();
    int randomStatus = rand() % statuses.size();
    int randomInternet = rand() % internetAccess.size();

    // Update connection status
    m_mainWindow->ui->label_connectionStatusValue->setText(statuses[randomStatus]);
    m_mainWindow->ui->label_connectionTypeValue->setText(connectionTypes[randomType]);
    m_mainWindow->ui->label_internetAccessValue->setText(internetAccess[randomInternet]);

    // Set color based on status
    if (statuses[randomStatus] == "Connected")
    {
        m_mainWindow->ui->label_connectionStatusValue->setStyleSheet("color: #27ae60; font-weight: bold;");
        m_mainWindow->ui->label_internetAccessValue->setStyleSheet("color: #27ae60; font-weight: bold;");
    }
    else if (statuses[randomStatus] == "Limited Access")
    {
        m_mainWindow->ui->label_connectionStatusValue->setStyleSheet("color: #e67e22; font-weight: bold;");
        m_mainWindow->ui->label_internetAccessValue->setStyleSheet("color: #e67e22; font-weight: bold;");
    }
    else
    {
        m_mainWindow->ui->label_connectionStatusValue->setStyleSheet("color: #e74c3c; font-weight: bold;");
        m_mainWindow->ui->label_internetAccessValue->setStyleSheet("color: #e74c3c; font-weight: bold;");
    }

    // Update IP addresses with random values
    QString ipv4 = QString("192.168.%1.%2").arg(rand() % 255).arg(rand() % 255);
    QString ipv6 = QString("2001:db8:%1:%2::%3").arg(rand() % 9999).arg(rand() % 9999).arg(rand() % 9999);
    QString gateway = QString("192.168.%1.1").arg(rand() % 255);

    m_mainWindow->ui->label_ipv4Value->setText(ipv4);
    m_mainWindow->ui->label_ipv6Value->setText(ipv6);
    m_mainWindow->ui->label_gatewayValue->setText(gateway);

    // Random DNS servers
    QStringList dnsServers = {"8.8.8.8, 8.8.4.4", "1.1.1.1, 1.0.0.1", "9.9.9.9, 149.112.112.112", "208.67.222.222, 208.67.220.220"};
    m_mainWindow->ui->label_dnsValue->setText(dnsServers[rand() % dnsServers.size()]);

    // Update SSID if connected via WiFi
    if (connectionTypes[randomType] == "WiFi")
    {
        QStringList ssids = {"HomeWiFi_5G", "Office_Network", "Guest_WiFi", "TP-Link_2.4G"};
        m_mainWindow->ui->label_ssidValue->setText(ssids[rand() % ssids.size()]);
    }
    else
    {
        m_mainWindow->ui->label_ssidValue->setText("N/A");
    }
}

void NetworkManager::testConnection()
{
    m_mainWindow->ui->textEdit_pingOutput->setPlainText("Testing internet connection...\n");

    // Simulate connection test with delay
    QTimer::singleShot(1000, this, [this]()
                       {
        m_mainWindow->ui->textEdit_pingOutput->append("✓ DNS resolution: Working");
        QTimer::singleShot(500, this, [this]() {
            m_mainWindow->ui->textEdit_pingOutput->append("✓ Gateway reachable: Yes");
            QTimer::singleShot(500, this, [this]() {
                m_mainWindow->ui->textEdit_pingOutput->append("✓ Internet access: Available");
                QTimer::singleShot(500, this, [this]() {
                    m_mainWindow->ui->textEdit_pingOutput->append("✓ Latency: 24ms");
                    m_mainWindow->ui->textEdit_pingOutput->append("\nConnection test completed successfully!");
                });
            });
        }); });
}

void NetworkManager::startPing()
{
    QString target = m_mainWindow->ui->lineEdit_pingTarget->text();
    if (target.isEmpty())
    {
        target = "google.com";
        m_mainWindow->ui->lineEdit_pingTarget->setText(target);
    }

    m_mainWindow->ui->textEdit_pingOutput->setPlainText(QString("Pinging %1...\n").arg(target));
    m_mainWindow->ui->pushButton_startPing->setEnabled(false);
    m_mainWindow->ui->pushButton_stopPing->setEnabled(true);

    // Simulate ping results
    m_pingTimer = new QTimer(this);
    m_pingCount = 0;
    connect(m_pingTimer, &QTimer::timeout, this, &NetworkManager::simulatePing);
    m_pingTimer->start(1000);
}

void NetworkManager::stopPing()
{
    if (m_pingTimer && m_pingTimer->isActive())
    {
        m_pingTimer->stop();
        m_pingTimer->deleteLater();
        m_pingTimer = nullptr;
    }

    m_mainWindow->ui->textEdit_pingOutput->append("\nPing stopped by user.");
    m_mainWindow->ui->pushButton_startPing->setEnabled(true);
    m_mainWindow->ui->pushButton_stopPing->setEnabled(false);
}

void NetworkManager::simulatePing()
{
    m_pingCount++;
    if (m_pingCount > 10)
    {
        stopPing();
        return;
    }

    int latency = 20 + (rand() % 30); // Random latency between 20-50ms
    int ttl = 64 - m_pingCount;

    QString pingResult = QString("Reply from %1: bytes=32 time=%2ms TTL=%3")
                             .arg(m_mainWindow->ui->lineEdit_pingTarget->text())
                             .arg(latency)
                             .arg(ttl);

    m_mainWindow->ui->textEdit_pingOutput->append(pingResult);
}

void NetworkManager::startTraceroute()
{
    QString target = m_mainWindow->ui->lineEdit_tracerouteTarget->text();
    if (target.isEmpty())
    {
        target = "google.com";
        m_mainWindow->ui->lineEdit_tracerouteTarget->setText(target);
    }

    m_mainWindow->ui->textEdit_tracerouteOutput->setPlainText(QString("Traceroute to %1...\n").arg(target));
    m_mainWindow->ui->pushButton_startTraceroute->setEnabled(false);
    m_mainWindow->ui->pushButton_stopTraceroute->setEnabled(true);

    // Simulate traceroute results
    m_tracerouteTimer = new QTimer(this);
    m_tracerouteHop = 0;
    connect(m_tracerouteTimer, &QTimer::timeout, this, &NetworkManager::simulateTraceroute);
    m_tracerouteTimer->start(800);
}

void NetworkManager::stopTraceroute()
{
    if (m_tracerouteTimer && m_tracerouteTimer->isActive())
    {
        m_tracerouteTimer->stop();
        m_tracerouteTimer->deleteLater();
        m_tracerouteTimer = nullptr;
    }

    m_mainWindow->ui->textEdit_tracerouteOutput->append("\nTraceroute stopped by user.");
    m_mainWindow->ui->pushButton_startTraceroute->setEnabled(true);
    m_mainWindow->ui->pushButton_stopTraceroute->setEnabled(false);
}

void NetworkManager::simulateTraceroute()
{
    m_tracerouteHop++;
    if (m_tracerouteHop > 15)
    {
        m_mainWindow->ui->textEdit_tracerouteOutput->append("\nTraceroute completed.");
        stopTraceroute();
        return;
    }

    QStringList routers = {
        "192.168.1.1", "10.0.0.1", "172.16.0.1", "203.0.113.1",
        "198.51.100.1", "203.0.113.254", "192.0.2.1", "198.18.0.1",
        "192.88.99.1", "2001:db8::1", "2001:4860:4860::8888"};

    int latency1 = 1 + (rand() % 10);
    int latency2 = 1 + (rand() % 10);
    int latency3 = 1 + (rand() % 10);

    QString router = routers[rand() % routers.size()];
    QString tracerouteResult = QString("%1  %2 ms  %3 ms  %4 ms  %5")
                                   .arg(m_tracerouteHop, 2)
                                   .arg(latency1, 2)
                                   .arg(latency2, 2)
                                   .arg(latency3, 2)
                                   .arg(router);

    m_mainWindow->ui->textEdit_tracerouteOutput->append(tracerouteResult);

    // If we reached the final hop
    if (m_tracerouteHop == 15)
    {
        QString finalResult = QString("15  %1 ms  %2 ms  %3 ms  %4")
                                  .arg(24, 2)
                                  .arg(25, 2)
                                  .arg(23, 2)
                                  .arg(m_mainWindow->ui->lineEdit_tracerouteTarget->text());
        m_mainWindow->ui->textEdit_tracerouteOutput->append(finalResult);
    }
}

void NetworkManager::startPortScan()
{
    QString target = m_mainWindow->ui->lineEdit_scannerTarget->text();
    if (target.isEmpty())
    {
        target = "localhost";
        m_mainWindow->ui->lineEdit_scannerTarget->setText(target);
    }

    int startPort = m_mainWindow->ui->spinBox_startPort->value();
    int endPort = m_mainWindow->ui->spinBox_endPort->value();

    if (startPort > endPort)
    {
        m_mainWindow->ui->label_scanSummary->setText("Error: Start port cannot be greater than end port");
        return;
    }

    m_mainWindow->ui->tableWidget_scanResults->setRowCount(0);
    m_mainWindow->ui->pushButton_startScan->setEnabled(false);
    m_mainWindow->ui->pushButton_stopScan->setEnabled(true);
    m_mainWindow->ui->progressBar_scan->setValue(0);

    // Simulate port scanning
    m_scanTimer = new QTimer(this);
    m_currentScanPort = startPort;
    m_scanEndPort = endPort;
    m_openPortsFound = 0;

    connect(m_scanTimer, &QTimer::timeout, this, &NetworkManager::simulatePortScan);
    m_scanTimer->start(50); // Fast simulation
}

void NetworkManager::stopPortScan()
{
    if (m_scanTimer && m_scanTimer->isActive())
    {
        m_scanTimer->stop();
        m_scanTimer->deleteLater();
        m_scanTimer = nullptr;
    }

    m_mainWindow->ui->pushButton_startScan->setEnabled(true);
    m_mainWindow->ui->pushButton_stopScan->setEnabled(false);
    m_mainWindow->ui->progressBar_scan->setValue(100);

    m_mainWindow->ui->label_scanSummary->setText(
        QString("Scan stopped. %1 ports scanned, %2 open ports found")
            .arg(m_currentScanPort - m_mainWindow->ui->spinBox_startPort->value())
            .arg(m_openPortsFound));
}

void NetworkManager::simulatePortScan()
{
    if (m_currentScanPort > m_scanEndPort)
    {
        stopPortScan();
        m_mainWindow->ui->label_scanSummary->setText(
            QString("Scan completed. %1 ports scanned, %2 open ports found")
                .arg(m_scanEndPort - m_mainWindow->ui->spinBox_startPort->value() + 1)
                .arg(m_openPortsFound));
        return;
    }

    // Update progress
    int totalPorts = m_scanEndPort - m_mainWindow->ui->spinBox_startPort->value() + 1;
    int progress = ((m_currentScanPort - m_mainWindow->ui->spinBox_startPort->value()) * 100) / totalPorts;
    m_mainWindow->ui->progressBar_scan->setValue(progress);

    // Common open ports with their services
    QMap<int, QString> commonPorts = {
        {21, "FTP"}, {22, "SSH"}, {23, "Telnet"}, {25, "SMTP"}, {53, "DNS"}, {80, "HTTP"}, {110, "POP3"}, {143, "IMAP"}, {443, "HTTPS"}, {993, "IMAPS"}, {995, "POP3S"}, {1433, "MSSQL"}, {3306, "MySQL"}, {3389, "RDP"}, {5432, "PostgreSQL"}, {6379, "Redis"}, {27017, "MongoDB"}};

    // Simulate finding open ports (random chance + common ports)
    bool isOpen = false;
    QString service = "Unknown";

    if (commonPorts.contains(m_currentScanPort))
    {
        // Higher chance for common ports to be "open" in simulation
        isOpen = (rand() % 100) < 30; // 30% chance
        service = commonPorts[m_currentScanPort];
    }
    else
    {
        // Lower chance for other ports
        isOpen = (rand() % 100) < 5; // 5% chance
        service = "Unknown";
    }

    if (isOpen)
    {
        m_openPortsFound++;
        int row = m_mainWindow->ui->tableWidget_scanResults->rowCount();
        m_mainWindow->ui->tableWidget_scanResults->insertRow(row);

        m_mainWindow->ui->tableWidget_scanResults->setItem(row, 0, new QTableWidgetItem(QString::number(m_currentScanPort)));
        m_mainWindow->ui->tableWidget_scanResults->setItem(row, 1, new QTableWidgetItem(service));

        QTableWidgetItem *statusItem = new QTableWidgetItem("Open");
        statusItem->setForeground(QBrush(QColor("#27ae60")));
        m_mainWindow->ui->tableWidget_scanResults->setItem(row, 2, statusItem);
    }

    m_currentScanPort++;
}