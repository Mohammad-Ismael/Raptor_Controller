#include "fileschecker.h"
#include "../mainwindow.h"
#include "../ui_mainwindow.h"
#include <QtConcurrent/QtConcurrent>
#include <QProgressDialog>
#include <QMessageBox>
#include <QDesktopServices>
#include <QCryptographicHash>
#include <QFile>
#include <QStorageInfo>
#include <QCompleter>
#include <QFileDialog>
#include <QHeaderView>

FilesChecker::FilesChecker(MainWindow *mainWindow, QObject *parent)
    : QObject(parent), m_mainWindow(mainWindow),
      m_cancelLargeFilesScan(false), m_cancelDuplicateFilesScan(false)
{
    m_largeFilesWatcher = new QFutureWatcher<QVector<FileInfo>>(this);
    m_duplicateFilesWatcher = new QFutureWatcher<QVector<DuplicateFile>>(this);
    
    connect(m_largeFilesWatcher, &QFutureWatcher<QVector<FileInfo>>::finished,
            this, &FilesChecker::onLargeFilesScanFinished);
    connect(m_duplicateFilesWatcher, &QFutureWatcher<QVector<DuplicateFile>>::finished,
            this, &FilesChecker::onDuplicateFilesScanFinished);
}

FilesChecker::~FilesChecker()
{
    // Cancel any running operations
    cancelLargeFilesScan();
    cancelDuplicateFilesScan();
    
    // Wait for operations to finish
    if (m_largeFilesWatcher->isRunning()) {
        m_largeFilesWatcher->waitForFinished();
    }
    if (m_duplicateFilesWatcher->isRunning()) {
        m_duplicateFilesWatcher->waitForFinished();
    }
    
    delete m_largeFilesWatcher;
    delete m_duplicateFilesWatcher;
}

void FilesChecker::scanLargeFiles(const QString &path, double minSizeGB)
{
    if (!m_mainWindow || !m_mainWindow->ui) return;

    // Reset cancel flag
    m_cancelLargeFilesScan = false;

    qint64 minSizeBytes = static_cast<qint64>(minSizeGB * 1024 * 1024 * 1024);
    
    m_mainWindow->ui->largeFilesResults->setPlainText("Scanning for large files...\nThis may take a while for large directories.");
    m_mainWindow->ui->scanLargeFilesButton->setEnabled(false);
    m_mainWindow->ui->cancelLargeFilesButton->setEnabled(true);

    // Clear previous results
    m_mainWindow->ui->largeFilesTable->setRowCount(0);

    QFuture<QVector<FileInfo>> future = QtConcurrent::run([path, minSizeBytes, this]() {
        return FilesChecker::performLargeFilesScan(path, minSizeBytes, m_cancelLargeFilesScan);
    });

    m_largeFilesWatcher->setFuture(future);
}

void FilesChecker::cancelLargeFilesScan()
{
    if (!m_largeFilesWatcher->isRunning()) {
        return; // No scan running
    }

    m_cancelLargeFilesScan = true;
    m_largeFilesWatcher->cancel();
    
    // Don't wait for finished here - let the onLargeFilesScanFinished handle it
    m_mainWindow->ui->largeFilesResults->append("\nCancelling scan...");
}

void FilesChecker::scanDuplicateFiles(const QString &path)
{
    if (!m_mainWindow || !m_mainWindow->ui) return;

    // Reset cancel flag
    m_cancelDuplicateFilesScan = false;

    m_mainWindow->ui->duplicateFilesResults->setPlainText("Scanning for duplicate files...\nThis may take a while as it calculates file hashes.");
    m_mainWindow->ui->scanDuplicateFilesButton->setEnabled(false);
    m_mainWindow->ui->cancelDuplicateFilesButton->setEnabled(true);

    // Clear previous results
    m_mainWindow->ui->duplicateFilesTree->clear();

    QFuture<QVector<DuplicateFile>> future = QtConcurrent::run([path, this]() {
        return FilesChecker::performDuplicateFilesScan(path, m_cancelDuplicateFilesScan);
    });

    m_duplicateFilesWatcher->setFuture(future);
}

void FilesChecker::cancelDuplicateFilesScan()
{
    if (!m_duplicateFilesWatcher->isRunning()) {
        return; // No scan running
    }

    m_cancelDuplicateFilesScan = true;
    m_duplicateFilesWatcher->cancel();
    
    // Don't wait for finished here - let the onDuplicateFilesScanFinished handle it
    m_mainWindow->ui->duplicateFilesResults->append("\nCancelling scan...");
}

void FilesChecker::deleteSelectedFiles(const QVector<FileInfo> &files)
{
    if (files.isEmpty()) return;

    int selectedCount = 0;
    qint64 totalSize = 0;
    for (const auto &file : files) {
        if (file.isSelected) {
            selectedCount++;
            totalSize += file.size;
        }
    }

    if (selectedCount == 0) {
        QMessageBox::information(m_mainWindow, "Delete Files", "No files selected for deletion.");
        return;
    }

    QMessageBox::StandardButton reply = QMessageBox::question(
        m_mainWindow, 
        "Confirm Delete",
        QString("Are you sure you want to delete %1 selected file(s)?\nTotal size: %2\n\nThis action cannot be undone.")
            .arg(selectedCount)
            .arg(formatFileSize(totalSize)),
        QMessageBox::Yes | QMessageBox::No
    );

    if (reply == QMessageBox::No) return;

    int deletedCount = 0;
    int failedCount = 0;

    for (const auto &file : files) {
        if (file.isSelected) {
            QFile qfile(file.path);
            if (qfile.remove()) {
                deletedCount++;
            } else {
                failedCount++;
                qDebug() << "Failed to delete:" << file.path << "Error:" << qfile.errorString();
            }
        }
    }

    QString result;
    if (deletedCount > 0) {
        result += QString("Successfully deleted %1 file(s).\n").arg(deletedCount);
    }
    if (failedCount > 0) {
        result += QString("Failed to delete %1 file(s).\n").arg(failedCount);
    }

    QMessageBox::information(m_mainWindow, "Delete Complete", result);
}

void FilesChecker::openFileLocation(const QString &filePath)
{
    QFileInfo fileInfo(filePath);
    QString directoryPath = fileInfo.absolutePath();
    
    if (QFileInfo::exists(directoryPath)) {
        QUrl url = QUrl::fromLocalFile(directoryPath);
        if (!QDesktopServices::openUrl(url)) {
            QMessageBox::warning(m_mainWindow, "Open Location", 
                                QString("Could not open file location:\n%1").arg(directoryPath));
        }
    } else {
        QMessageBox::warning(m_mainWindow, "Open Location", 
                            QString("Directory does not exist:\n%1").arg(directoryPath));
    }
}

void FilesChecker::refreshDiskSpace()
{
    if (!m_mainWindow || !m_mainWindow->ui) return;

    // Get disk space information using QStorageInfo
    QList<QStorageInfo> drives = QStorageInfo::mountedVolumes();
    
    QString diskInfo;
    diskInfo += "💾 DISK SPACE OVERVIEW\n";
    diskInfo += "══════════════════════\n\n";

    for (const QStorageInfo &drive : drives) {
        if (drive.isValid() && drive.isReady()) {
            QString name = drive.name();
            if (name.isEmpty()) name = "Local Disk";
            
            qint64 total = drive.bytesTotal();
            qint64 free = drive.bytesFree();
            qint64 used = total - free;
            int percentUsed = total > 0 ? (used * 100) / total : 0;

            // Color coding based on usage
            QString usageColor;
            if (percentUsed > 90) usageColor = "#e74c3c"; // Red
            else if (percentUsed > 70) usageColor = "#e67e22"; // Orange
            else usageColor = "#27ae60"; // Green

            diskInfo += QString("📁 <b>%1 (%2)</b>\n").arg(name).arg(drive.rootPath());
            diskInfo += QString("   Total: %1\n").arg(formatFileSize(total));
            diskInfo += QString("   Used:  <span style='color:%1'>%2 (%3%)</span>\n").arg(usageColor).arg(formatFileSize(used)).arg(percentUsed);
            diskInfo += QString("   Free:  %1\n\n").arg(formatFileSize(free));
        }
    }

    m_mainWindow->ui->diskSpaceOverview->setHtml(diskInfo);
}

QStringList FilesChecker::getCommonPaths()
{
    QStringList paths;
    
    // System drives
    QList<QStorageInfo> drives = QStorageInfo::mountedVolumes();
    for (const QStorageInfo &drive : drives) {
        if (drive.isValid() && drive.isReady()) {
            paths << drive.rootPath();
        }
    }
    
    // Common user directories
    paths << QDir::homePath();
    paths << QDir::homePath() + "/Desktop";
    paths << QDir::homePath() + "/Documents";
    paths << QDir::homePath() + "/Downloads";
    paths << QDir::homePath() + "/Pictures";
    paths << QDir::homePath() + "/Music";
    paths << QDir::homePath() + "/Videos";
    
    // Common program directories
    paths << "C:/Program Files";
    paths << "C:/Program Files (x86)";
    paths << "C:/Windows/Temp";
    paths << QDir::tempPath();
    
    return paths;
}

void FilesChecker::onLargeFilesScanFinished()
{
    if (!m_mainWindow || !m_mainWindow->ui) return;

    QVector<FileInfo> results;
    if (m_largeFilesWatcher->isCanceled()) {
        m_mainWindow->ui->largeFilesResults->setPlainText("Scan was cancelled.");
    } else {
        results = m_largeFilesWatcher->result();
        
        // Sort by size (descending)
        std::sort(results.begin(), results.end(), 
                  [](const FileInfo &a, const FileInfo &b) { return a.size > b.size; });

        // Update UI with results
        m_mainWindow->ui->largeFilesTable->setRowCount(0);
        
        for (const FileInfo &file : results) {
            int row = m_mainWindow->ui->largeFilesTable->rowCount();
            m_mainWindow->ui->largeFilesTable->insertRow(row);

            // Checkbox for selection with emojis
            QTableWidgetItem *checkItem = new QTableWidgetItem();
            checkItem->setCheckState(Qt::Unchecked);
            checkItem->setText("❌"); // Start with X
            checkItem->setFlags(checkItem->flags() | Qt::ItemIsUserCheckable);
            m_mainWindow->ui->largeFilesTable->setItem(row, 0, checkItem);

            // File path
            QTableWidgetItem *pathItem = new QTableWidgetItem(file.path);
            pathItem->setFlags(pathItem->flags() & ~Qt::ItemIsEditable); // Make read-only
            m_mainWindow->ui->largeFilesTable->setItem(row, 1, pathItem);
            
            // Size
            QTableWidgetItem *sizeItem = new QTableWidgetItem(file.sizeFormatted);
            sizeItem->setFlags(sizeItem->flags() & ~Qt::ItemIsEditable);
            m_mainWindow->ui->largeFilesTable->setItem(row, 2, sizeItem);
            
            // Last modified
            QTableWidgetItem *dateItem = new QTableWidgetItem(file.lastModified.toString("yyyy-MM-dd hh:mm:ss"));
            dateItem->setFlags(dateItem->flags() & ~Qt::ItemIsEditable);
            m_mainWindow->ui->largeFilesTable->setItem(row, 3, dateItem);
        }

        // Resize columns to content
        m_mainWindow->ui->largeFilesTable->resizeColumnsToContents();
        
        m_mainWindow->ui->largeFilesResults->setPlainText(
            QString("Scan completed! Found %1 large files.").arg(results.size()));
    }

    m_mainWindow->ui->scanLargeFilesButton->setEnabled(true);
    m_mainWindow->ui->cancelLargeFilesButton->setEnabled(false);
    m_mainWindow->ui->deleteLargeFilesButton->setEnabled(false);
    m_mainWindow->ui->openFileLocationButton->setEnabled(false);
    m_cancelLargeFilesScan = false;
}

void FilesChecker::onDuplicateFilesScanFinished()
{
    if (!m_mainWindow || !m_mainWindow->ui) return;

    QVector<DuplicateFile> results;
    if (m_duplicateFilesWatcher->isCanceled()) {
        m_mainWindow->ui->duplicateFilesResults->setPlainText("Scan was cancelled.");
    } else {
        results = m_duplicateFilesWatcher->result();
        
        // Update UI with results
        m_mainWindow->ui->duplicateFilesTree->clear();
        
        int totalDuplicates = 0;
        qint64 totalWastedSpace = 0;

        for (const DuplicateFile &duplicate : results) {
            QTreeWidgetItem *groupItem = new QTreeWidgetItem(m_mainWindow->ui->duplicateFilesTree);
            groupItem->setText(0, QString("Duplicate Group (%1 files, %2 each)")
                .arg(duplicate.files.size())
                .arg(formatFileSize(duplicate.files.first().size)));

            for (const FileInfo &file : duplicate.files) {
                QTreeWidgetItem *fileItem = new QTreeWidgetItem(groupItem);
                fileItem->setText(0, file.path);
                fileItem->setText(1, file.sizeFormatted);
                fileItem->setText(2, file.lastModified.toString("yyyy-MM-dd"));
                fileItem->setCheckState(0, Qt::Unchecked);
            }

            totalDuplicates += duplicate.files.size() - 1; // -1 because one file is the original
            totalWastedSpace += duplicate.totalSize - duplicate.files.first().size;
        }

        m_mainWindow->ui->duplicateFilesResults->setPlainText(
            QString("Scan completed! Found %1 duplicate groups.\n"
                    "Potential space savings: %2")
                .arg(results.size())
                .arg(formatFileSize(totalWastedSpace)));
    }

    m_mainWindow->ui->scanDuplicateFilesButton->setEnabled(true);
    m_mainWindow->ui->cancelDuplicateFilesButton->setEnabled(false);
    m_cancelDuplicateFilesScan = false;
}

// Static helper methods
QVector<FileInfo> FilesChecker::performLargeFilesScan(const QString &path, qint64 minSizeBytes, QAtomicInteger<bool> &cancelFlag)
{
    QVector<FileInfo> results;
    QDir dir(path);

    if (!dir.exists() || cancelFlag) return results;

    // Get all files recursively
    QFileInfoList files = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden, QDir::Name);
    
    for (const QFileInfo &fileInfo : files) {
        if (cancelFlag) break;
        
        if (fileInfo.size() >= minSizeBytes) {
            FileInfo file;
            file.path = fileInfo.absoluteFilePath();
            file.size = fileInfo.size();
            file.sizeFormatted = formatFileSize(file.size);
            file.lastModified = fileInfo.lastModified();
            file.isSelected = false;
            results.append(file);
        }
    }

    // Recursively scan subdirectories if not cancelled
    if (!cancelFlag) {
        QFileInfoList subDirs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QFileInfo &subDir : subDirs) {
            if (cancelFlag) break;
            results.append(performLargeFilesScan(subDir.absoluteFilePath(), minSizeBytes, cancelFlag));
        }
    }

    return results;
}

QVector<DuplicateFile> FilesChecker::performDuplicateFilesScan(const QString &path, QAtomicInteger<bool> &cancelFlag)
{
    QVector<DuplicateFile> results;
    QHash<QString, DuplicateFile> duplicateMap;
    
    if (cancelFlag) return results;

    // First pass: group by file size (quick check)
    QHash<qint64, QVector<FileInfo>> sizeMap;
    
    QDir dir(path);
    QFileInfoList allFiles = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden, QDir::Name);
    
    for (const QFileInfo &fileInfo : allFiles) {
        if (cancelFlag) break;
        
        if (fileInfo.size() > 0) { // Skip empty files
            FileInfo file;
            file.path = fileInfo.absoluteFilePath();
            file.size = fileInfo.size();
            file.sizeFormatted = formatFileSize(file.size);
            file.lastModified = fileInfo.lastModified();
            file.isSelected = false;
            
            sizeMap[file.size].append(file);
        }
    }

    // Second pass: calculate hash for files with same size
    for (auto it = sizeMap.begin(); it != sizeMap.end() && !cancelFlag; ++it) {
        if (it.value().size() > 1) { // Potential duplicates
            for (const FileInfo &file : it.value()) {
                if (cancelFlag) break;
                
                QString hash = calculateFileHash(file.path, cancelFlag);
                if (!hash.isEmpty() && !cancelFlag) {
                    duplicateMap[hash].files.append(file);
                    duplicateMap[hash].hash = hash;
                    duplicateMap[hash].totalSize += file.size;
                }
            }
        }
    }

    // Only keep groups with actual duplicates
    for (auto it = duplicateMap.begin(); it != duplicateMap.end(); ++it) {
        if (it->files.size() > 1) {
            results.append(*it);
        }
    }

    return results;
}

QString FilesChecker::calculateFileHash(const QString &filePath, QAtomicInteger<bool> &cancelFlag)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return QString();
    }

    QCryptographicHash hash(QCryptographicHash::Md5);
    const qint64 bufferSize = 8192;
    char buffer[bufferSize];

    while (!file.atEnd() && !cancelFlag) {
        qint64 bytesRead = file.read(buffer, bufferSize);
        if (bytesRead > 0) {
            hash.addData(buffer, bytesRead);
        }
    }

    if (cancelFlag) {
        file.close();
        return QString();
    }

    file.close();
    return hash.result().toHex();
}

QString FilesChecker::formatFileSize(qint64 size)
{
    if (size < 1024) return QString("%1 B").arg(size);
    else if (size < 1024 * 1024) return QString("%1 KB").arg(size / 1024.0, 0, 'f', 1);
    else if (size < 1024 * 1024 * 1024) return QString("%1 MB").arg(size / (1024.0 * 1024.0), 0, 'f', 1);
    else return QString("%1 GB").arg(size / (1024.0 * 1024.0 * 1024.0), 0, 'f', 1);
}

void FilesChecker::openFileDirectory(const QString &filePath)
{
    QFileInfo fileInfo(filePath);
    QString directoryPath = fileInfo.absolutePath();
    
    if (QFileInfo::exists(directoryPath)) {
        QUrl url = QUrl::fromLocalFile(directoryPath);
        if (!QDesktopServices::openUrl(url)) {
            QMessageBox::warning(m_mainWindow, "Open Location", 
                                QString("Could not open file location:\n%1").arg(directoryPath));
        }
    } else {
        QMessageBox::warning(m_mainWindow, "Open Location", 
                            QString("Directory does not exist:\n%1").arg(directoryPath));
    }
}