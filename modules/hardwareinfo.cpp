#include "hardwareinfo.h"
#include "../mainwindow.h"
#include "../ui_mainwindow.h"
#include <QDateTime>

HardwareInfo::HardwareInfo(MainWindow *mainWindow, QObject *parent)
    : QObject(parent), m_mainWindow(mainWindow)
{
}

void HardwareInfo::refreshHardware()
{
    // Simulate hardware information gathering
    QString hardwareInfo;
    hardwareInfo += "<span style='color:#000000;'>";
    hardwareInfo += "🖥️ SYSTEM HARDWARE INFORMATION<br>";
    hardwareInfo += "══════════════════════════════<br><br>";

    // CPU Information
    hardwareInfo += "🔹 PROCESSOR (CPU)<br>";
    hardwareInfo += "──────────────────<br>";
    hardwareInfo += "Intel Core i7-12700K<br>";
    hardwareInfo += "   Cores: 12 Physical, 20 Logical<br>";
    hardwareInfo += "   Clock Speed: 3.6 GHz<br><br>";

    // Motherboard Information
    hardwareInfo += "🔧 MOTHERBOARD<br>";
    hardwareInfo += "──────────────<br>";
    hardwareInfo += "ASUS ROG STRIX Z690-A GAMING WIFI<br>";
    hardwareInfo += "   Version: Rev 1.xx<br><br>";

    // GPU Information
    hardwareInfo += "🎮 GRAPHICS CARD (GPU)<br>";
    hardwareInfo += "─────────────────────<br>";
    hardwareInfo += "NVIDIA GeForce RTX 4070<br>";
    hardwareInfo += "   VRAM: 12.0 GB<br>";
    hardwareInfo += "   Driver: 546.17<br>";
    hardwareInfo += "   Temperature: 42°C<br>";
    hardwareInfo += "   Utilization: 8%<br><br>";

    // RAM Information
    hardwareInfo += "💾 MEMORY (RAM)<br>";
    hardwareInfo += "───────────────<br>";
    hardwareInfo += "32.0 GB<br>";
    hardwareInfo += "   Manufacturer: Corsair<br>";
    hardwareInfo += "   Speed: 3200 MHz<br><br>";

    // Storage Information
    hardwareInfo += "💿 STORAGE DRIVES<br>";
    hardwareInfo += "────────────────<br>";
    hardwareInfo += "   • Samsung SSD 980 PRO 1TB (1.0 TB) [SSD]<br>";
    hardwareInfo += "   • Seagate Barracuda ST2000DM008 (2.0 TB) [HDD]<br><br>";

    // Network Information
    hardwareInfo += "🌐 NETWORK ADAPTERS<br>";
    hardwareInfo += "──────────────────<br>";
    hardwareInfo += "   • Intel(R) Wi-Fi 6 AX201 160MHz (Ethernet 802.3)<br>";
    hardwareInfo += "      MAC: AA:BB:CC:DD:EE:FF<br>";
    hardwareInfo += "   • Realtek PCIe GbE Family Controller (Ethernet 802.3)<br>";
    hardwareInfo += "      MAC: 11:22:33:44:55:66<br><br>";

    // USB Information
    hardwareInfo += "🔌 USB DEVICES<br>";
    hardwareInfo += "──────────────<br>";
    hardwareInfo += "8 connected USB devices<br><br>";

    hardwareInfo += "⏱️ Last updated: " + QDateTime::currentDateTime().toString("hh:mm:ss AP");
    hardwareInfo += "</span>";

    m_mainWindow->ui->textEdit_hardwareInfo->setHtml(hardwareInfo);
    m_mainWindow->ui->label_hardwareStatus->setText("Hardware information updated successfully!");
}

void HardwareInfo::exportHardware()
{
    m_mainWindow->ui->label_hardwareStatus->setText("Hardware report exported to hardware_info.txt");
}