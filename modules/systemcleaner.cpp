#include "systemcleaner.h"
#include "../mainwindow.h"
#include "../ui_mainwindow.h"
#include <QProcess>
#include <QMessageBox>
#include <QTimer>
#include <QtConcurrent/QtConcurrent>

SystemCleaner::SystemCleaner(MainWindow *mainWindow, QObject *parent)
    : QObject(parent), m_mainWindow(mainWindow), m_isScanning(false)
{
    m_scanWatcher = new QFutureWatcher<QVector<double>>(this);
    setupConnections();
}

SystemCleaner::~SystemCleaner()
{
    delete m_scanWatcher;
}

void SystemCleaner::setupConnections()
{
    connect(m_scanWatcher, &QFutureWatcher<QVector<double>>::finished, 
            this, &SystemCleaner::onQuickScanFinished);
}

void SystemCleaner::performQuickScan()
{
    if (m_isScanning) {
        m_mainWindow->ui->quickCleanResults->setPlainText("Scan already in progress...");
        return;
    }
    
    m_mainWindow->ui->quickCleanResults->setPlainText("Scanning for junk files...\n(You can navigate to other tabs while scanning)");
    m_mainWindow->ui->scanQuickButton->setEnabled(false);
    m_mainWindow->ui->cleanQuickButton->setEnabled(false);
    m_mainWindow->ui->quickCleanProgressBar->setValue(0);
    
    m_isScanning = true;
    
    // Start the scan in background thread using lambda
    QFuture<QVector<double>> future = QtConcurrent::run([]() {
        return SystemCleaner::performScan();
    });
    
    m_scanWatcher->setFuture(future);
}

void SystemCleaner::performQuickClean()
{
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(m_mainWindow, "Confirm Clean", 
                                 "This will delete temporary files, cache, and other junk files.\n\n"
                                 "Are you sure you want to continue?",
                                 QMessageBox::Yes | QMessageBox::No);
    
    if (reply == QMessageBox::No) {
        return;
    }
    
    m_mainWindow->ui->quickCleanResults->append("\n\nCleaning in progress...");
    m_mainWindow->ui->cleanQuickButton->setEnabled(false);
    
    QProcess process;
    
    // Clean temporary files
    process.start("powershell", QStringList() << "-Command" << 
        "Get-ChildItem -Path $env:TEMP -Recurse -File -ErrorAction SilentlyContinue | ForEach-Object { "
        "try { Remove-Item $_.FullName -Force -ErrorAction Stop } "
        "catch { Write-Warning \"Failed to remove: $($_.FullName)\" } "
        "}");
    process.waitForFinished();
    
    // Clean Windows Temp
    process.start("powershell", QStringList() << "-Command" << 
        "Get-ChildItem -Path 'C:\\Windows\\Temp' -Recurse -File -ErrorAction SilentlyContinue | Where-Object { "
        "$_.CreationTime -lt (Get-Date).AddDays(-1) } | ForEach-Object { "
        "try { Remove-Item $_.FullName -Force -ErrorAction Stop } catch { } }");
    process.waitForFinished();
    
    // Clean Memory Dump Files
    process.start("powershell", QStringList() << "-Command" << 
        "Get-ChildItem -Path 'C:\\Windows\\*.dmp' -ErrorAction SilentlyContinue | ForEach-Object { "
        "try { Remove-Item $_.FullName -Force -ErrorAction Stop } catch { } }; "
        "Get-ChildItem -Path 'C:\\Windows\\LiveKernelReports\\*.dmp' -ErrorAction SilentlyContinue | ForEach-Object { "
        "try { Remove-Item $_.FullName -Force -ErrorAction Stop } catch { } }");
    process.waitForFinished();
    
    // Clean Thumbnail Cache (this requires Explorer restart)
    process.start("powershell", QStringList() << "-Command" << 
        "Stop-Process -Name 'explorer' -Force -ErrorAction SilentlyContinue; "
        "Start-Sleep -Seconds 2; "
        "Get-ChildItem -Path '$env:LOCALAPPDATA\\Microsoft\\Windows\\Explorer\\thumbcache_*.db' -ErrorAction SilentlyContinue | ForEach-Object { "
        "try { Remove-Item $_.FullName -Force -ErrorAction Stop } catch { } }; "
        "Start-Process 'explorer.exe'");
    process.waitForFinished();
    
    // Clean Prefetch Files (only old ones)
    process.start("powershell", QStringList() << "-Command" << 
        "Get-ChildItem -Path 'C:\\Windows\\Prefetch\\*.pf' -ErrorAction SilentlyContinue | Where-Object { "
        "$_.LastWriteTime -lt (Get-Date).AddDays(-7) } | ForEach-Object { "
        "try { Remove-Item $_.FullName -Force -ErrorAction Stop } catch { } }");
    process.waitForFinished();
    
    m_mainWindow->ui->quickCleanResults->append("Cleaning completed! Rescanning to verify...");
    m_mainWindow->ui->spaceSavedLabel->setText("Cleaning process finished");

    // Rescan after a delay to show new sizes
    QTimer::singleShot(3000, this, [this]() { performQuickScan(); });
}

void SystemCleaner::performSystemScan()
{
    // Update list items with simulated sizes using QRegularExpression
    QRegularExpression regex("\\(.*\\)");
    for (int i = 0; i < m_mainWindow->ui->systemCleanerList->count(); ++i)
    {
        QListWidgetItem *item = m_mainWindow->ui->systemCleanerList->item(i);
        QString text = item->text();
        text.replace(regex, "(125 MB)");
        item->setText(text);
    }
    m_mainWindow->ui->cleanSystemButton->setEnabled(true);
}

void SystemCleaner::performSystemClean()
{
    m_mainWindow->ui->cleanSystemButton->setEnabled(false);
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
    
    if (results.size() >= 6) {
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