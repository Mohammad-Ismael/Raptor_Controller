#include "softwaremanager.h"
#include "../mainwindow.h"
#include "../ui_mainwindow.h"
#include <QTableWidgetItem>

SoftwareManager::SoftwareManager(MainWindow *mainWindow, QObject *parent)
    : QObject(parent), m_mainWindow(mainWindow)
{
}

void SoftwareManager::refreshSoftware()
{
    populateSoftwareTable();
}

void SoftwareManager::searchSoftware(const QString &searchText)
{
    // Simple search filter
    if (searchText.isEmpty())
    {
        // Show all items
        for (int i = 0; i < m_mainWindow->ui->softwareTable->rowCount(); ++i)
        {
            m_mainWindow->ui->softwareTable->setRowHidden(i, false);
        }
    }
    else
    {
        // Hide non-matching items
        for (int i = 0; i < m_mainWindow->ui->softwareTable->rowCount(); ++i)
        {
            QTableWidgetItem *item = m_mainWindow->ui->softwareTable->item(i, 0); // Name column
            bool match = item && item->text().contains(searchText, Qt::CaseInsensitive);
            m_mainWindow->ui->softwareTable->setRowHidden(i, !match);
        }
    }
}

void SoftwareManager::onSoftwareSelectionChanged()
{
    QList<QTableWidgetItem *> selectedItems = m_mainWindow->ui->softwareTable->selectedItems();
    bool hasSelection = !selectedItems.isEmpty();

    m_mainWindow->ui->uninstallSoftwareButton->setEnabled(hasSelection);
    m_mainWindow->ui->forceUninstallButton->setEnabled(hasSelection);

    if (hasSelection)
    {
        int row = m_mainWindow->ui->softwareTable->currentRow();
        QString name = m_mainWindow->ui->softwareTable->item(row, 0)->text();
        QString version = m_mainWindow->ui->softwareTable->item(row, 1)->text();
        QString size = m_mainWindow->ui->softwareTable->item(row, 2)->text();

        m_mainWindow->ui->selectedSoftwareInfo->setText(
            QString("Selected: %1 %2 (%3)").arg(name).arg(version).arg(size));
    }
    else
    {
        m_mainWindow->ui->selectedSoftwareInfo->setText("No software selected");
    }
}

void SoftwareManager::uninstallSoftware()
{
    // Placeholder for uninstall functionality
    int row = m_mainWindow->ui->softwareTable->currentRow();
    if (row >= 0)
    {
        QString softwareName = m_mainWindow->ui->softwareTable->item(row, 0)->text();
        m_mainWindow->ui->selectedSoftwareInfo->setText(
            QString("Would uninstall: %1 (normal mode)").arg(softwareName));
    }
}

void SoftwareManager::forceUninstallSoftware()
{
    // Placeholder for force uninstall functionality
    int row = m_mainWindow->ui->softwareTable->currentRow();
    if (row >= 0)
    {
        QString softwareName = m_mainWindow->ui->softwareTable->item(row, 0)->text();
        m_mainWindow->ui->selectedSoftwareInfo->setText(
            QString("Would force uninstall: %1").arg(softwareName));
    }
}

void SoftwareManager::populateSoftwareTable()
{
    // Clear existing items
    m_mainWindow->ui->softwareTable->setRowCount(0);

    // Add sample data - in real implementation, this would query the system
    QStringList sampleSoftware = {
        "Google Chrome", "Mozilla Firefox", "Microsoft Edge", "Visual Studio Code",
        "Adobe Reader", "VLC Media Player", "7-Zip", "WinRAR", "Spotify", "Discord"};

    QStringList versions = {
        "96.0.4664.110", "95.0.2", "96.0.1054.62", "1.63.0",
        "2021.011.20039", "3.0.16", "21.07", "6.02", "1.1.68.610", "1.0.9003"};

    QStringList sizes = {
        "350 MB", "280 MB", "320 MB", "450 MB",
        "650 MB", "85 MB", "2.5 MB", "3.1 MB", "180 MB", "140 MB"};

    QStringList dates = {
        "2023-11-15", "2023-11-10", "2023-11-20", "2023-11-05",
        "2023-10-28", "2023-11-12", "2023-09-15", "2023-10-20", "2023-11-18", "2023-11-22"};

    for (int i = 0; i < sampleSoftware.size(); ++i)
    {
        int row = m_mainWindow->ui->softwareTable->rowCount();
        m_mainWindow->ui->softwareTable->insertRow(row);

        m_mainWindow->ui->softwareTable->setItem(row, 0, new QTableWidgetItem(sampleSoftware[i]));
        m_mainWindow->ui->softwareTable->setItem(row, 1, new QTableWidgetItem(versions[i]));
        m_mainWindow->ui->softwareTable->setItem(row, 2, new QTableWidgetItem(sizes[i]));
        m_mainWindow->ui->softwareTable->setItem(row, 3, new QTableWidgetItem(dates[i]));
    }

    // Resize columns to content
    m_mainWindow->ui->softwareTable->resizeColumnsToContents();
}