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
#include <QCoreApplication>
#include <QHeaderView>
#include <QProgressBar>
#include <QLabel>
#include <QHBoxLayout>

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
    // Set cancel flags first
    m_cancelLargeFilesScan = true;
    m_cancelDuplicateFilesScan = true;
    
    // Cancel any running operations
    if (m_largeFilesWatcher && m_largeFilesWatcher->isRunning()) {
        m_largeFilesWatcher->cancel();
    }
    if (m_duplicateFilesWatcher && m_duplicateFilesWatcher->isRunning()) {
        m_duplicateFilesWatcher->cancel();
    }
    
    // Process events to allow cancellation to take effect
    QCoreApplication::processEvents();
    
    // Delete watchers
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
    m_mainWindow->ui->deleteLargeFilesButton->setEnabled(false);

    // Clear previous results
    m_mainWindow->ui->largeFilesTable->setRowCount(0);
    m_mainWindow->ui->largeFilesTable->clearContents();

    QFuture<QVector<FileInfo>> future = QtConcurrent::run([path, minSizeBytes, this]() {
        return FilesChecker::performLargeFilesScan(path, minSizeBytes, m_cancelLargeFilesScan);
    });

    m_largeFilesWatcher->setFuture(future);
}

void FilesChecker::cancelLargeFilesScan()
{
    if (!m_largeFilesWatcher || !m_largeFilesWatcher->isRunning()) {
        return;
    }

    m_cancelLargeFilesScan = true;
    m_largeFilesWatcher->cancel();
    
    if (m_mainWindow && m_mainWindow->ui) {
        m_mainWindow->ui->largeFilesResults->append("\nScan cancelled by user.");
    }
}

void FilesChecker::scanDuplicateFiles(const QString &path)
{
    if (!m_mainWindow || !m_mainWindow->ui) return;

    // Reset cancel flag
    m_cancelDuplicateFilesScan = false;

    m_mainWindow->ui->duplicateFilesResults->setPlainText("Scanning for duplicate files...\nThis may take a while as it calculates file hashes.");
    m_mainWindow->ui->scanDuplicateFilesButton->setEnabled(false);
    m_mainWindow->ui->cancelDuplicateFilesButton->setEnabled(true);
    m_mainWindow->ui->deleteDuplicateFilesButton->setEnabled(false);

    // Clear previous results
    m_mainWindow->ui->duplicateFilesTree->clear();

    QFuture<QVector<DuplicateFile>> future = QtConcurrent::run([path, this]() {
        return FilesChecker::performDuplicateFilesScan(path, m_cancelDuplicateFilesScan);
    });

    m_duplicateFilesWatcher->setFuture(future);
}

void FilesChecker::cancelDuplicateFilesScan()
{
    if (!m_duplicateFilesWatcher || !m_duplicateFilesWatcher->isRunning()) {
        return;
    }

    m_cancelDuplicateFilesScan = true;
    m_duplicateFilesWatcher->cancel();
    
    if (m_mainWindow && m_mainWindow->ui) {
        m_mainWindow->ui->duplicateFilesResults->append("\nScan cancelled by user.");
    }
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
    
    // Clear previous disk widgets
    QLayout *layout = m_mainWindow->ui->diskSpaceContainer->layout();
    if (layout) {
        QLayoutItem *item;
        while ((item = layout->takeAt(0)) != nullptr) {
            if (item->widget()) {
                item->widget()->deleteLater();
            }
            delete item;
        }
    }

    for (const QStorageInfo &drive : drives) {
        if (drive.isValid() && drive.isReady()) {
            QString name = drive.name();
            if (name.isEmpty()) name = "Local Disk";
            
            qint64 total = drive.bytesTotal();
            qint64 free = drive.bytesFree();
            qint64 used = total - free;
            int percentUsed = total > 0 ? (used * 100) / total : 0;

            // Create disk widget
            QWidget *diskWidget = new QWidget();
            diskWidget->setStyleSheet(
                "QWidget {"
                "    background-color: #f8f9fa;"
                "    border: 1px solid #ecf0f1;"
                "    border-radius: 8px;"
                "    padding: 10px;"
                "    margin: 2px;"
                "}"
            );

            QHBoxLayout *diskLayout = new QHBoxLayout(diskWidget);
            diskLayout->setSpacing(12);
            diskLayout->setContentsMargins(8, 6, 8, 6);

            // Drive icon and name
            QLabel *iconLabel = new QLabel("💾");
            iconLabel->setStyleSheet("font-size: 16px; background: transparent;");
            
            QLabel *nameLabel = new QLabel(QString("<b>%1</b><br>%2").arg(name).arg(drive.rootPath()));
            nameLabel->setStyleSheet("color: #2c3e50; font-size: 10px; background: transparent;");
            nameLabel->setFixedWidth(160);

            // Progress bar
            QProgressBar *progressBar = new QProgressBar();
            progressBar->setValue(percentUsed);
            progressBar->setMaximum(100);
            progressBar->setMinimum(0);
            progressBar->setFixedHeight(14);
            
            // Set progress bar color based on usage
            QString progressStyle;
            if (percentUsed > 90) {
                progressStyle = "QProgressBar { border: 1px solid #e74c3c; border-radius: 7px; background-color: #f5b7b1; }"
                               "QProgressBar::chunk { background-color: #e74c3c; border-radius: 6px; }";
            } else if (percentUsed > 70) {
                progressStyle = "QProgressBar { border: 1px solid #e67e22; border-radius: 7px; background-color: #fad7a0; }"
                               "QProgressBar::chunk { background-color: #e67e22; border-radius: 6px; }";
            } else {
                progressStyle = "QProgressBar { border: 1px solid #27ae60; border-radius: 7px; background-color: #a9dfbf; }"
                               "QProgressBar::chunk { background-color: #27ae60; border-radius: 6px; }";
            }
            progressBar->setStyleSheet(progressStyle);

            // Usage info
            QLabel *infoLabel = new QLabel(
                QString("Used: %1 / %2<br>Free: %3 (%4%)")
                    .arg(formatFileSize(used))
                    .arg(formatFileSize(total))
                    .arg(formatFileSize(free))
                    .arg(100 - percentUsed)
            );
            infoLabel->setStyleSheet("color: #7f8c8d; font-size: 9px; background: transparent;");
            infoLabel->setFixedWidth(200);

            // Add widgets to disk layout
            diskLayout->addWidget(iconLabel);
            diskLayout->addWidget(nameLabel);
            diskLayout->addWidget(progressBar, 1);
            diskLayout->addWidget(infoLabel);

            // Add disk widget to container
            m_mainWindow->ui->diskSpaceContainer->layout()->addWidget(diskWidget);
        }
    }

    // Add stretch to push items to top
    static_cast<QVBoxLayout*>(m_mainWindow->ui->diskSpaceContainer->layout())->addStretch();
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
        try {
            results = m_largeFilesWatcher->result();
            
            qDebug() << "Scan completed with" << results.size() << "files found";
            
            // Sort by size (descending)
            std::sort(results.begin(), results.end(), 
                      [](const FileInfo &a, const FileInfo &b) { return a.size > b.size; });

            // Update UI with results
            m_mainWindow->ui->largeFilesTable->setRowCount(0);
            m_mainWindow->ui->largeFilesTable->setSortingEnabled(false);
            
            for (int i = 0; i < results.size(); ++i) {
                const FileInfo &file = results[i];
                int row = m_mainWindow->ui->largeFilesTable->rowCount();
                m_mainWindow->ui->largeFilesTable->insertRow(row);

                // EMOJI ONLY APPROACH - NO QT CHECKBOX FLAGS
                QTableWidgetItem *checkItem = new QTableWidgetItem("❌");
                checkItem->setData(Qt::UserRole, false); // Store checked state
                checkItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
                checkItem->setTextAlignment(Qt::AlignCenter);
                checkItem->setFont(QFont("Segoe UI Emoji", 12));
                m_mainWindow->ui->largeFilesTable->setItem(row, 0, checkItem);

                // File path
                QTableWidgetItem *pathItem = new QTableWidgetItem(file.path);
                pathItem->setFlags(pathItem->flags() & ~Qt::ItemIsEditable);
                m_mainWindow->ui->largeFilesTable->setItem(row, 1, pathItem);
                
                // Size
                QTableWidgetItem *sizeItem = new QTableWidgetItem(file.sizeFormatted);
                sizeItem->setFlags(sizeItem->flags() & ~Qt::ItemIsEditable);
                sizeItem->setTextAlignment(Qt::AlignRight);
                m_mainWindow->ui->largeFilesTable->setItem(row, 2, sizeItem);
                
                // Last modified
                QTableWidgetItem *dateItem = new QTableWidgetItem(file.lastModified.toString("yyyy-MM-dd hh:mm:ss"));
                dateItem->setFlags(dateItem->flags() & ~Qt::ItemIsEditable);
                m_mainWindow->ui->largeFilesTable->setItem(row, 3, dateItem);
            }

            // Set column widths
            m_mainWindow->ui->largeFilesTable->setColumnWidth(0, 60);
            m_mainWindow->ui->largeFilesTable->setColumnWidth(1, 500);
            m_mainWindow->ui->largeFilesTable->setColumnWidth(2, 100);
            m_mainWindow->ui->largeFilesTable->setColumnWidth(3, 150);

            // Enable sorting and refresh
            m_mainWindow->ui->largeFilesTable->setSortingEnabled(true);
            m_mainWindow->ui->largeFilesTable->viewport()->update();

            m_mainWindow->ui->largeFilesResults->setPlainText(
                QString("Scan completed! Found %1 large files.").arg(results.size()));
                
        } catch (const std::exception& e) {
            qDebug() << "Error processing scan results:" << e.what();
            m_mainWindow->ui->largeFilesResults->setPlainText("Error processing scan results.");
        }
    }

    m_mainWindow->ui->scanLargeFilesButton->setEnabled(true);
    m_mainWindow->ui->cancelLargeFilesButton->setEnabled(false);
    m_mainWindow->ui->deleteLargeFilesButton->setEnabled(false);
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

            totalDuplicates += duplicate.files.size() - 1;
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
    // Simplified implementation for now
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
    QByteArray buffer(bufferSize, 0);

    while (!file.atEnd() && !cancelFlag) {
        qint64 bytesRead = file.read(buffer.data(), bufferSize);
        if (bytesRead > 0) {
            hash.addData(QByteArrayView(buffer.constData(), bytesRead));
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