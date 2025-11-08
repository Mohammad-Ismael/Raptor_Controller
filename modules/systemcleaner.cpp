#include "systemcleaner.h"
#include "../mainwindow.h"
#include "../ui_mainwindow.h"
#include <QProcess>
#include <QMessageBox>
#include <QTimer>
#include <QtConcurrent/QtConcurrent>
#include <QListWidgetItem>
#include <QProgressDialog>
#include <QApplication>
#include <QThread>
#include <QRegularExpression>

SystemCleaner::SystemCleaner(MainWindow *mainWindow, QObject *parent)
    : QObject(parent), m_mainWindow(mainWindow), m_isScanning(false), m_isSystemScanning(false)
{
    m_scanWatcher = new QFutureWatcher<QVector<double>>(this);
    m_systemScanWatcher = new QFutureWatcher<QVector<double>>(this);
    setupConnections();
}

SystemCleaner::~SystemCleaner()
{
    delete m_scanWatcher;
    delete m_systemScanWatcher;
}

void SystemCleaner::setupConnections()
{
    connect(m_scanWatcher, &QFutureWatcher<QVector<double>>::finished,
            this, &SystemCleaner::onQuickScanFinished);
    connect(m_systemScanWatcher, &QFutureWatcher<QVector<double>>::finished,
            this, &SystemCleaner::onSystemScanFinished);
}

void SystemCleaner::performQuickScan()
{
    if (m_isScanning)
    {
        m_mainWindow->ui->quickCleanResults->setPlainText("Scan already in progress...");
        return;
    }

    m_mainWindow->ui->quickCleanResults->setPlainText("Scanning for junk files...\n(You can navigate to other tabs while scanning)");
    m_mainWindow->ui->scanQuickButton->setEnabled(false);
    m_mainWindow->ui->cleanQuickButton->setEnabled(false);
    m_mainWindow->ui->quickCleanProgressBar->setValue(0);

    m_isScanning = true;

    // Start the scan in background thread using lambda
    QFuture<QVector<double>> future = QtConcurrent::run([]()
                                                        { return SystemCleaner::performScan(); });

    m_scanWatcher->setFuture(future);
}

void SystemCleaner::performQuickClean()
{
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(m_mainWindow, "Confirm Clean",
                                  "This will delete temporary files, cache, and other junk files.\n\n"
                                  "Are you sure you want to continue?",
                                  QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::No)
    {
        return;
    }

    m_mainWindow->ui->quickCleanResults->append("\n\nCleaning in progress...");
    m_mainWindow->ui->cleanQuickButton->setEnabled(false);

    QProcess process;

    // Clean temporary files
    process.start("powershell", QStringList() << "-Command" << "Get-ChildItem -Path $env:TEMP -Recurse -File -ErrorAction SilentlyContinue | ForEach-Object { "
                                                               "try { Remove-Item $_.FullName -Force -ErrorAction Stop } "
                                                               "catch { Write-Warning \"Failed to remove: $($_.FullName)\" } "
                                                               "}");
    process.waitForFinished();

    // Clean Windows Temp
    process.start("powershell", QStringList() << "-Command" << "Get-ChildItem -Path 'C:\\Windows\\Temp' -Recurse -File -ErrorAction SilentlyContinue | Where-Object { "
                                                               "$_.CreationTime -lt (Get-Date).AddDays(-1) } | ForEach-Object { "
                                                               "try { Remove-Item $_.FullName -Force -ErrorAction Stop } catch { } }");
    process.waitForFinished();

    // Clean Memory Dump Files
    process.start("powershell", QStringList() << "-Command" << "Get-ChildItem -Path 'C:\\Windows\\*.dmp' -ErrorAction SilentlyContinue | ForEach-Object { "
                                                               "try { Remove-Item $_.FullName -Force -ErrorAction Stop } catch { } }; "
                                                               "Get-ChildItem -Path 'C:\\Windows\\LiveKernelReports\\*.dmp' -ErrorAction SilentlyContinue | ForEach-Object { "
                                                               "try { Remove-Item $_.FullName -Force -ErrorAction Stop } catch { } }");
    process.waitForFinished();

    // Clean Thumbnail Cache (this requires Explorer restart)
    process.start("powershell", QStringList() << "-Command" << "Stop-Process -Name 'explorer' -Force -ErrorAction SilentlyContinue; "
                                                               "Start-Sleep -Seconds 2; "
                                                               "Get-ChildItem -Path '$env:LOCALAPPDATA\\Microsoft\\Windows\\Explorer\\thumbcache_*.db' -ErrorAction SilentlyContinue | ForEach-Object { "
                                                               "try { Remove-Item $_.FullName -Force -ErrorAction Stop } catch { } }; ");
    process.waitForFinished();

    // Clean Prefetch Files (only old ones)
    process.start("powershell", QStringList() << "-Command" << "Get-ChildItem -Path 'C:\\Windows\\Prefetch\\*.pf' -ErrorAction SilentlyContinue | Where-Object { "
                                                               "$_.LastWriteTime -lt (Get-Date).AddDays(-7) } | ForEach-Object { "
                                                               "try { Remove-Item $_.FullName -Force -ErrorAction Stop } catch { } }");
    process.waitForFinished();

    m_mainWindow->ui->quickCleanResults->append("Cleaning completed! Rescanning to verify...");
    m_mainWindow->ui->spaceSavedLabel->setText("Cleaning process finished");

    // Rescan after a delay to show new sizes
    QTimer::singleShot(3000, this, [this]()
                       { performQuickScan(); });
}

void SystemCleaner::performSystemScan()
{
    if (m_isSystemScanning)
    {
        return;
    }

    if (!m_mainWindow || !m_mainWindow->ui)
        return;

    QListWidget *systemList = m_mainWindow->ui->systemCleanerList;
    if (!systemList)
        return;

    // Show scanning message - remove checkmarks during scan
    for (int i = 0; i < systemList->count(); ++i)
    {
        QListWidgetItem *item = systemList->item(i);
        QString text = item->text();
        // Remove any existing checkmark and show scanning
        text.replace("✅", "");
        text.replace("❌", "");
        text.replace(QRegularExpression("\\(.*\\)"), "(Scanning...)");
        item->setText(text);
    }

    m_mainWindow->ui->scanSystemButton->setEnabled(false);
    m_mainWindow->ui->cleanSystemButton->setEnabled(false);
    m_isSystemScanning = true;

    // Start system scan in background
    QFuture<QVector<double>> future = QtConcurrent::run([]()
                                                        { return SystemCleaner::performSystemScanInternal(); });

    m_systemScanWatcher->setFuture(future);
}

void SystemCleaner::performSystemClean()
{
    if (!m_mainWindow || !m_mainWindow->ui)
        return;

    QListWidget *systemList = m_mainWindow->ui->systemCleanerList;
    if (!systemList)
        return;

    // Get selected items - now we'll check which items have checkmarks
    QList<QListWidgetItem *> itemsToClean;
    double totalSelectedSize = 0;

    for (int i = 0; i < systemList->count(); ++i)
    {
        QListWidgetItem *item = systemList->item(i);
        QString itemText = item->text();

        // Check if item has checkmark (✅) and extract size
        if (itemText.contains("✅"))
        {
            itemsToClean.append(item);

            // Extract size for total calculation
            QRegularExpression sizeRegex("\\(([0-9.]+) ([KMG]B)\\)");
            QRegularExpressionMatch match = sizeRegex.match(itemText);

            if (match.hasMatch())
            {
                double size = match.captured(1).toDouble();
                QString unit = match.captured(2);

                // Convert to MB for total
                if (unit == "KB")
                    size /= 1024;
                else if (unit == "GB")
                    size *= 1024;

                totalSelectedSize += size;
            }
        }
    }

    if (itemsToClean.isEmpty())
    {
        QMessageBox::information(m_mainWindow, "System Cleaner",
                                 "Please select items to clean by clicking on them first.\n\n"
                                 "Click on items to toggle the checkmark ✅.");
        return;
    }

    // Show confirmation dialog with selected items
    QString confirmationText = QString("This will delete the following %1 selected items:\n\n").arg(itemsToClean.size());

    for (int i = 0; i < itemsToClean.size() && i < 5; ++i)
    {
        QString itemText = itemsToClean[i]->text();
        // Clean up the text for display
        itemText.replace("✅", "").replace("❌", "").trimmed();
        confirmationText += "• " + itemText + "\n";
    }

    if (itemsToClean.size() > 5)
    {
        confirmationText += QString("• ... and %1 more items\n").arg(itemsToClean.size() - 5);
    }

    confirmationText += QString("\nTotal size: %1 MB\n\nAre you sure you want to continue?")
                            .arg(totalSelectedSize, 0, 'f', 1);

    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(m_mainWindow, "Confirm System Clean",
                                  confirmationText,
                                  QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::No)
    {
        return;
    }

    // Show cleaning progress
    QProgressDialog progress("Cleaning selected system files...", "Cancel", 0, itemsToClean.size(), m_mainWindow);
    progress.setWindowModality(Qt::WindowModal);
    progress.show();

    double totalCleaned = 0;
    QProcess process;

    for (int i = 0; i < itemsToClean.size(); ++i)
    {
        if (progress.wasCanceled())
            break;

        QListWidgetItem *item = itemsToClean[i];
        QString itemText = item->text();

        // Extract size from item text for reporting
        QRegularExpression sizeRegex("\\(([0-9.]+) ([KMG]B)\\)");
        QRegularExpressionMatch match = sizeRegex.match(itemText);

        if (match.hasMatch())
        {
            double size = match.captured(1).toDouble();
            QString unit = match.captured(2);

            // Convert to MB for total
            if (unit == "KB")
                size /= 1024;
            else if (unit == "GB")
                size *= 1024;

            totalCleaned += size;
        }

        // Clean based on category
        if (itemText.contains("Temporary Files"))
        {
            process.start("powershell", QStringList() << "-Command" << "Get-ChildItem -Path $env:TEMP, 'C:\\Windows\\Temp' -Recurse -File -ErrorAction SilentlyContinue | ForEach-Object { "
                                                                       "try { Remove-Item $_.FullName -Force -ErrorAction Stop } catch { } }");
            process.waitForFinished();
        }
        else if (itemText.contains("Windows Update Cache"))
        {
            process.start("powershell", QStringList() << "-Command" << "Get-ChildItem -Path 'C:\\Windows\\SoftwareDistribution\\Download' -Recurse -File -ErrorAction SilentlyContinue | ForEach-Object { "
                                                                       "try { Remove-Item $_.FullName -Force -ErrorAction Stop } catch { } }");
            process.waitForFinished();
        }
        else if (itemText.contains("System Log Files"))
        {
            process.start("powershell", QStringList() << "-Command" << "Get-ChildItem -Path 'C:\\Windows\\Logs' -Recurse -File -ErrorAction SilentlyContinue | Where-Object { "
                                                                       "$_.LastWriteTime -lt (Get-Date).AddDays(-30) } | ForEach-Object { "
                                                                       "try { Remove-Item $_.FullName -Force -ErrorAction Stop } catch { } }");
            process.waitForFinished();
        }
        else if (itemText.contains("Memory Dump Files"))
        {
            process.start("powershell", QStringList() << "-Command" << "Get-ChildItem -Path 'C:\\Windows\\*.dmp', 'C:\\Windows\\LiveKernelReports\\*.dmp' -File -ErrorAction SilentlyContinue | ForEach-Object { "
                                                                       "try { Remove-Item $_.FullName -Force -ErrorAction Stop } catch { } }");
            process.waitForFinished();
        }
        else if (itemText.contains("Thumbnail Cache"))
        {
            process.start("powershell", QStringList() << "-Command" << "Stop-Process -Name 'explorer' -Force -ErrorAction SilentlyContinue; "
                                                                       "Start-Sleep -Seconds 2; "
                                                                       "Get-ChildItem -Path '$env:LOCALAPPDATA\\Microsoft\\Windows\\Explorer\\thumbcache_*.db' -ErrorAction SilentlyContinue | ForEach-Object { "
                                                                       "try { Remove-Item $_.FullName -Force -ErrorAction Stop } catch { } }; ");
            process.waitForFinished();
        }
        else if (itemText.contains("Error Reports"))
        {
            process.start("powershell", QStringList() << "-Command" << "Get-ChildItem -Path 'C:\\ProgramData\\Microsoft\\Windows\\WER', '$env:LOCALAPPDATA\\Microsoft\\Windows\\WER' -Recurse -File -ErrorAction SilentlyContinue | Where-Object { "
                                                                       "$_.LastWriteTime -lt (Get-Date).AddDays(-7) } | ForEach-Object { "
                                                                       "try { Remove-Item $_.FullName -Force -ErrorAction Stop } catch { } }");
            process.waitForFinished();
        }
        else if (itemText.contains("Recycle Bin"))
        {
            process.start("powershell", QStringList() << "-Command" << "Clear-RecycleBin -Force -ErrorAction SilentlyContinue");
            process.waitForFinished();
        }
        else if (itemText.contains("Prefetch Files"))
        {
            process.start("powershell", QStringList() << "-Command" << "Get-ChildItem -Path 'C:\\Windows\\Prefetch\\*.pf' -File -ErrorAction SilentlyContinue | ForEach-Object { "
                                                                       "try { Remove-Item $_.FullName -Force -ErrorAction Stop } catch { } }");
            process.waitForFinished();
        }
        else if (itemText.contains("Font Cache"))
        {
            process.start("powershell", QStringList() << "-Command" << "Get-ChildItem -Path \"$env:LOCALAPPDATA\\Microsoft\\Windows\\FontCache\" -Recurse -File -ErrorAction SilentlyContinue | ForEach-Object { "
                                                                       "try { Remove-Item $_.FullName -Force -ErrorAction Stop } catch { } }");
            process.waitForFinished();
        }
        else if (itemText.contains("Delivery Optimization"))
        {
            process.start("powershell", QStringList() << "-Command" << "Get-ChildItem -Path \"$env:LOCALAPPDATA\\Microsoft\\Windows\\DeliveryOptimization\" -Recurse -File -ErrorAction SilentlyContinue | ForEach-Object { "
                                                                       "try { Remove-Item $_.FullName -Force -ErrorAction Stop } catch { } }");
            process.waitForFinished();
        }
        else if (itemText.contains("Error Reporting Archive"))
        {
            process.start("powershell", QStringList() << "-Command" << "Get-ChildItem -Path \"$env:LOCALAPPDATA\\Microsoft\\Windows\\WER\\ReportArchive\" -Recurse -File -ErrorAction SilentlyContinue | ForEach-Object { "
                                                                       "try { Remove-Item $_.FullName -Force -ErrorAction Stop } catch { } }");
            process.waitForFinished();
        }
        else if (itemText.contains("Windows Defender Scans"))
        {
            process.start("powershell", QStringList() << "-Command" << "Get-ChildItem -Path \"C:\\ProgramData\\Microsoft\\Windows Defender\\Scans\" -Recurse -File -ErrorAction SilentlyContinue | ForEach-Object { "
                                                                       "try { Remove-Item $_.FullName -Force -ErrorAction Stop } catch { } }");
            process.waitForFinished();
        }

        // Update progress
        progress.setValue(i + 1);
        QApplication::processEvents();
        QThread::msleep(300);
    }

    progress.close();

    // Show completion message
    QString sizeText;
    if (totalCleaned < 1)
    {
        sizeText = QString("%1 KB").arg(int(totalCleaned * 1024));
    }
    else if (totalCleaned < 1024)
    {
        sizeText = QString("%1 MB").arg(totalCleaned, 0, 'f', 1);
    }
    else
    {
        sizeText = QString("%1 GB").arg(totalCleaned / 1024, 0, 'f', 1);
    }

    QMessageBox::information(m_mainWindow, "System Cleaner",
                             QString("Successfully cleaned %1 of data!").arg(sizeText));

    // Reset the UI
    m_mainWindow->ui->cleanSystemButton->setEnabled(false);

    // Rescan to show new sizes
    performSystemScan();
}

void SystemCleaner::performBrowserScan()
{
    m_mainWindow->ui->cleanBrowsersButton->setEnabled(true);
}

void SystemCleaner::performBrowserClean()
{
    m_mainWindow->ui->cleanBrowsersButton->setEnabled(false);
}

void SystemCleaner::onQuickScanFinished()
{
    m_isScanning = false;

    // Get results from the future
    QVector<double> results = m_scanWatcher->result();

    if (results.size() >= 6)
    {
        double tempSize = results[0];
        double updateSize = results[1];
        double logSize = results[2];
        double dumpSize = results[3];
        double thumbSize = results[4];
        double prefetchSize = results[5];

        double totalSize = tempSize + updateSize + logSize + dumpSize + thumbSize + prefetchSize;

        QString resultsText = QString("Scanning completed!\n\n"
                                      "• Temporary Files: %1 MB\n"
                                      "• Windows Update Cache: %2 MB\n"
                                      "• System Log Files: %3 MB\n"
                                      "• Memory Dump Files: %4 MB\n"
                                      "• Thumbnail Cache: %5 MB\n"
                                      "• Prefetch Files: %6 MB\n\n"
                                      "Total: %7 MB")
                                  .arg(tempSize, 0, 'f', 1)
                                  .arg(updateSize, 0, 'f', 1)
                                  .arg(logSize, 0, 'f', 1)
                                  .arg(dumpSize, 0, 'f', 1)
                                  .arg(thumbSize, 0, 'f', 1)
                                  .arg(prefetchSize, 0, 'f', 1)
                                  .arg(totalSize, 0, 'f', 1);

        m_mainWindow->ui->quickCleanResults->setPlainText(resultsText);
        m_mainWindow->ui->spaceSavedLabel->setText(QString("Total space to be freed: %1 MB").arg(totalSize, 0, 'f', 1));
        m_mainWindow->ui->cleanQuickButton->setEnabled(totalSize > 0);
    }

    m_mainWindow->ui->scanQuickButton->setEnabled(true);
    m_mainWindow->ui->quickCleanProgressBar->setValue(100);
}

void SystemCleaner::onSystemScanFinished()
{
    m_isSystemScanning = false;

    // Get results from the future
    QVector<double> results = m_systemScanWatcher->result();
    updateSystemScanResults(results);
}

void SystemCleaner::updateScanProgress(int value)
{
    m_mainWindow->ui->quickCleanProgressBar->setValue(value);
}

QVector<double> SystemCleaner::performScan()
{
    QVector<double> results;
    results.resize(6);

    QProcess process;

    // Get temporary files size
    process.start("powershell", QStringList() << "-Command" << "(Get-ChildItem $env:TEMP -Recurse -File -ErrorAction SilentlyContinue | Measure-Object -Property Length -Sum).Sum / 1MB");
    process.waitForFinished();
    results[0] = process.readAllStandardOutput().trimmed().toDouble();

    // Get Windows Update cache size
    process.start("powershell", QStringList() << "-Command" << "(Get-ChildItem 'C:\\Windows\\Temp' -Recurse -File -ErrorAction SilentlyContinue | Measure-Object -Property Length -Sum).Sum / 1MB");
    process.waitForFinished();
    results[1] = process.readAllStandardOutput().trimmed().toDouble();

    // Get system log files size
    process.start("powershell", QStringList() << "-Command" << "(Get-ChildItem 'C:\\Windows\\Logs' -Recurse -File -ErrorAction SilentlyContinue | Measure-Object -Property Length -Sum).Sum / 1MB");
    process.waitForFinished();
    results[2] = process.readAllStandardOutput().trimmed().toDouble();

    // Get memory dump files size
    process.start("powershell", QStringList() << "-Command" << "((Get-ChildItem 'C:\\Windows\\*.dmp' -File -ErrorAction SilentlyContinue | Measure-Object -Property Length -Sum).Sum + (Get-ChildItem 'C:\\Windows\\LiveKernelReports\\*.dmp' -File -ErrorAction SilentlyContinue | Measure-Object -Property Length -Sum).Sum) / 1MB");
    process.waitForFinished();
    results[3] = process.readAllStandardOutput().trimmed().toDouble();

    // Get thumbnail cache size
    process.start("powershell", QStringList() << "-Command" << "(Get-ChildItem \"$env:LOCALAPPDATA\\Microsoft\\Windows\\Explorer\\thumbcache_*.db\" -File -ErrorAction SilentlyContinue | Measure-Object -Property Length -Sum).Sum / 1MB");
    process.waitForFinished();
    results[4] = process.readAllStandardOutput().trimmed().toDouble();

    // Get prefetch files size
    process.start("powershell", QStringList() << "-Command" << "(Get-ChildItem 'C:\\Windows\\Prefetch\\*.pf' -File -ErrorAction SilentlyContinue | Measure-Object -Property Length -Sum).Sum / 1MB");
    process.waitForFinished();
    results[5] = process.readAllStandardOutput().trimmed().toDouble();

    return results;
}

QVector<double> SystemCleaner::performSystemScanInternal()
{
    QVector<double> results;
    results.resize(12);

    QProcess process;

    // 1. Temporary Files (%TEMP% and Windows Temp)
    process.start("powershell", QStringList() << "-Command" << "try { ((Get-ChildItem -Path $env:TEMP -Recurse -File -ErrorAction SilentlyContinue | Measure-Object -Property Length -Sum).Sum + "
                                                               "(Get-ChildItem -Path 'C:\\Windows\\Temp' -Recurse -File -ErrorAction SilentlyContinue | Measure-Object -Property Length -Sum).Sum) / 1MB } catch { 0 }");
    process.waitForFinished();
    results[0] = process.readAllStandardOutput().trimmed().toDouble();

    // 2. Windows Update Cache
    process.start("powershell", QStringList() << "-Command" << "try { (Get-ChildItem -Path 'C:\\Windows\\SoftwareDistribution\\Download' -Recurse -File -ErrorAction SilentlyContinue | Measure-Object -Property Length -Sum).Sum / 1MB } catch { 0 }");
    process.waitForFinished();
    results[1] = process.readAllStandardOutput().trimmed().toDouble();

    // 3. System Log Files (only old logs > 30 days)
    process.start("powershell", QStringList() << "-Command" << "try { (Get-ChildItem -Path 'C:\\Windows\\Logs' -Recurse -File -ErrorAction SilentlyContinue | Where-Object { $_.LastWriteTime -lt (Get-Date).AddDays(-30) } | Measure-Object -Property Length -Sum).Sum / 1MB } catch { 0 }");
    process.waitForFinished();
    results[2] = process.readAllStandardOutput().trimmed().toDouble();

    // 4. Memory Dump Files
    process.start("powershell", QStringList() << "-Command" << "try { ((Get-ChildItem -Path 'C:\\Windows\\*.dmp' -File -ErrorAction SilentlyContinue | Measure-Object -Property Length -Sum).Sum + "
                                                               "(Get-ChildItem -Path 'C:\\Windows\\LiveKernelReports\\*.dmp' -File -ErrorAction SilentlyContinue | Measure-Object -Property Length -Sum).Sum) / 1MB } catch { 0 }");
    process.waitForFinished();
    results[3] = process.readAllStandardOutput().trimmed().toDouble();

    // 5. Thumbnail Cache
    process.start("powershell", QStringList() << "-Command" << "try { (Get-ChildItem -Path \"$env:LOCALAPPDATA\\Microsoft\\Windows\\Explorer\\thumbcache_*.db\" -File -ErrorAction SilentlyContinue | Measure-Object -Property Length -Sum).Sum / 1MB } catch { 0 }");
    process.waitForFinished();
    results[4] = process.readAllStandardOutput().trimmed().toDouble();

    // 6. Error Reports
    process.start("powershell", QStringList() << "-Command" << "try { ((Get-ChildItem -Path 'C:\\ProgramData\\Microsoft\\Windows\\WER' -Recurse -File -ErrorAction SilentlyContinue | Where-Object { $_.LastWriteTime -lt (Get-Date).AddDays(-7) } | Measure-Object -Property Length -Sum).Sum + "
                                                               "(Get-ChildItem -Path \"$env:LOCALAPPDATA\\Microsoft\\Windows\\WER\" -Recurse -File -ErrorAction SilentlyContinue | Where-Object { $_.LastWriteTime -lt (Get-Date).AddDays(-7) } | Measure-Object -Property Length -Sum).Sum) / 1MB } catch { 0 }");
    process.waitForFinished();
    results[5] = process.readAllStandardOutput().trimmed().toDouble();

    // 7. Recycle Bin - Use COM object to get accurate size
    process.start("powershell", QStringList() << "-Command" << "try { "
                                                               "$shell = New-Object -ComObject Shell.Application; "
                                                               "$recycleBin = $shell.NameSpace(0x0a); "
                                                               "$totalSize = 0; "
                                                               "foreach ($item in $recycleBin.Items()) { $totalSize += $item.Size }; "
                                                               "$totalSize / 1MB "
                                                               "} catch { 0 }");
    process.waitForFinished();
    results[6] = process.readAllStandardOutput().trimmed().toDouble();

    // 8. Windows Prefetch
    process.start("powershell", QStringList() << "-Command" << "try { (Get-ChildItem -Path 'C:\\Windows\\Prefetch\\*.pf' -File -ErrorAction SilentlyContinue | Where-Object { $_.LastWriteTime -lt (Get-Date).AddDays(-7) } | Measure-Object -Property Length -Sum).Sum / 1MB } catch { 0 }");
    process.waitForFinished();
    results[7] = process.readAllStandardOutput().trimmed().toDouble();

    // 9. Windows Font Cache
    process.start("powershell", QStringList() << "-Command" << "try { (Get-ChildItem -Path \"$env:LOCALAPPDATA\\Microsoft\\Windows\\FontCache\" -Recurse -File -ErrorAction SilentlyContinue | Measure-Object -Property Length -Sum).Sum / 1MB } catch { 0 }");
    process.waitForFinished();
    results[8] = process.readAllStandardOutput().trimmed().toDouble();

    // 10. Delivery Optimization Files
    process.start("powershell", QStringList() << "-Command" << "try { (Get-ChildItem -Path \"$env:LOCALAPPDATA\\Microsoft\\Windows\\DeliveryOptimization\\Cache\" -Recurse -File -ErrorAction SilentlyContinue | Measure-Object -Property Length -Sum).Sum / 1MB } catch { 0 }");
    process.waitForFinished();
    results[9] = process.readAllStandardOutput().trimmed().toDouble();

    // 11. Windows Error Reporting Archive
    process.start("powershell", QStringList() << "-Command" << "try { (Get-ChildItem -Path \"$env:LOCALAPPDATA\\Microsoft\\Windows\\WER\\ReportArchive\" -Recurse -File -ErrorAction SilentlyContinue | Where-Object { $_.LastWriteTime -lt (Get-Date).AddDays(-7) } | Measure-Object -Property Length -Sum).Sum / 1MB } catch { 0 }");
    process.waitForFinished();
    results[10] = process.readAllStandardOutput().trimmed().toDouble();

    // 12. Windows Defender Scans
    process.start("powershell", QStringList() << "-Command" << "try { (Get-ChildItem -Path \"C:\\ProgramData\\Microsoft\\Windows Defender\\Scans\\History\" -Recurse -File -ErrorAction SilentlyContinue | Where-Object { $_.LastWriteTime -lt (Get-Date).AddDays(-30) } | Measure-Object -Property Length -Sum).Sum / 1MB } catch { 0 }");
    process.waitForFinished();
    results[11] = process.readAllStandardOutput().trimmed().toDouble();

    // Debug: Print results to console
    qDebug() << "Scan results:";
    for (int i = 0; i < results.size(); ++i)
    {
        qDebug() << "Category" << i << ":" << results[i] << "MB";
    }

    return results;
}

void SystemCleaner::updateSystemScanResults(const QVector<double> &results)
{
    if (!m_mainWindow || !m_mainWindow->ui)
        return;

    QListWidget *systemList = m_mainWindow->ui->systemCleanerList;
    if (!systemList)
        return;

    // Define category names matching your UI - now with more options
    QStringList categories = {
        "Temporary Files",
        "Windows Update Cache",
        "System Log Files",
        "Memory Dump Files",
        "Thumbnail Cache",
        "Error Reports",
        "Recycle Bin",
        "Prefetch Files",
        "Font Cache",
        "Delivery Optimization",
        "Error Reporting Archive",
        "Windows Defender Scans"};

    double totalSize = 0;

    for (int i = 0; i < systemList->count() && i < results.size(); ++i)
    {
        QListWidgetItem *item = systemList->item(i);
        double sizeMB = results[i];
        totalSize += sizeMB;

        QString sizeText;
        if (sizeMB < 1)
        {
            sizeText = QString("(%1 KB)").arg(int(sizeMB * 1024));
        }
        else if (sizeMB < 1024)
        {
            sizeText = QString("(%1 MB)").arg(sizeMB, 0, 'f', 1);
        }
        else
        {
            sizeText = QString("(%1 GB)").arg(sizeMB / 1024, 0, 'f', 1);
        }

        // Start with empty checkmark - user can click to toggle
        QString itemText = "❌ " + categories[i] + " " + sizeText;
        item->setText(itemText);

        // Add tooltip with more info
        QString description = getCategoryDescription(categories[i]);
        item->setToolTip(QString("Category: %1\n%2\nEstimated size: %3\nClick to toggle selection ✅/❌")
                             .arg(categories[i])
                             .arg(description)
                             .arg(sizeText));
    }

    // Enable clean button and scan button
    m_mainWindow->ui->cleanSystemButton->setEnabled(totalSize > 0);
    m_mainWindow->ui->scanSystemButton->setEnabled(true);

    qDebug() << "System scan completed. Total size:" << totalSize << "MB";
}

// Add this helper function for category descriptions:
QString SystemCleaner::getCategoryDescription(const QString &category)
{
    static QMap<QString, QString> descriptions = {
        {"Temporary Files", "Application and system temporary files. Safe to delete."},
        {"Windows Update Cache", "Leftover Windows Update installation files. Safe to delete."},
        {"System Log Files", "Old system event logs (keeps recent 30 days). Safe to delete."},
        {"Memory Dump Files", "System crash memory dumps. Safe to delete if not debugging."},
        {"Thumbnail Cache", "File thumbnail cache. Will rebuild automatically. Safe to delete."},
        {"Error Reports", "Windows Error Reporting files. Safe to delete."},
        {"Recycle Bin", "Deleted files waiting for permanent removal. Safe to empty."},
        {"Prefetch Files", "Application launch optimization files. Safe to delete."},
        {"Font Cache", "Cached font data. Will rebuild automatically. Safe to delete."},
        {"Delivery Optimization", "Windows Update cache for peer-to-peer sharing. Safe to delete."},
        {"Error Reporting Archive", "Archived error reports. Safe to delete."},
        {"Windows Defender Scans", "Previous antivirus scan results. Safe to delete."}};

    return descriptions.value(category, "System files that can be safely cleaned.");
}