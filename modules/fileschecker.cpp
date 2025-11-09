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

    // Setup the tree widget
    setupDuplicateFilesTree();
}

FilesChecker::~FilesChecker()
{
    // Set cancel flags first
    m_cancelLargeFilesScan = true;
    m_cancelDuplicateFilesScan = true;

    // Cancel any running operations
    if (m_largeFilesWatcher && m_largeFilesWatcher->isRunning())
    {
        m_largeFilesWatcher->cancel();
    }
    if (m_duplicateFilesWatcher && m_duplicateFilesWatcher->isRunning())
    {
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
    if (!m_mainWindow || !m_mainWindow->ui)
        return;

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

    QFuture<QVector<FileInfo>> future = QtConcurrent::run([path, minSizeBytes, this]()
                                                          { return FilesChecker::performLargeFilesScan(path, minSizeBytes, m_cancelLargeFilesScan); });

    m_largeFilesWatcher->setFuture(future);
}

void FilesChecker::cancelLargeFilesScan()
{
    if (!m_largeFilesWatcher || !m_largeFilesWatcher->isRunning())
    {
        return;
    }

    m_cancelLargeFilesScan = true;
    m_largeFilesWatcher->cancel();

    if (m_mainWindow && m_mainWindow->ui)
    {
        m_mainWindow->ui->largeFilesResults->append("\nScan cancelled by user.");
    }
}

void FilesChecker::scanDuplicateFiles(const QString &path)
{
    if (!m_mainWindow || !m_mainWindow->ui)
        return;

    // Reset cancel flag
    m_cancelDuplicateFilesScan = false;

    m_mainWindow->ui->duplicateFilesResults->setPlainText("Scanning for duplicate files...\nThis may take a while as it calculates file hashes.");
    m_mainWindow->ui->scanDuplicateFilesButton->setEnabled(false);
    m_mainWindow->ui->cancelDuplicateFilesButton->setEnabled(true);
    m_mainWindow->ui->deleteDuplicateFilesButton->setEnabled(false);

    // Clear previous results
    m_mainWindow->ui->duplicateFilesTree->clear();

    QFuture<QVector<DuplicateFile>> future = QtConcurrent::run([path, this]()
                                                               { return FilesChecker::performDuplicateFilesScan(path, m_cancelDuplicateFilesScan); });

    m_duplicateFilesWatcher->setFuture(future);
}

void FilesChecker::cancelDuplicateFilesScan()
{
    if (!m_duplicateFilesWatcher || !m_duplicateFilesWatcher->isRunning())
    {
        return;
    }

    m_cancelDuplicateFilesScan = true;
    m_duplicateFilesWatcher->cancel();

    if (m_mainWindow && m_mainWindow->ui)
    {
        m_mainWindow->ui->duplicateFilesResults->append("\nScan cancelled by user.");
    }
}

void FilesChecker::deleteSelectedFiles(const QVector<FileInfo> &files)
{
    if (files.isEmpty())
        return;

    int selectedCount = 0;
    qint64 totalSize = 0;
    for (const auto &file : files)
    {
        if (file.isSelected)
        {
            selectedCount++;
            totalSize += file.size;
        }
    }

    if (selectedCount == 0)
    {
        QMessageBox::information(m_mainWindow, "Delete Files", "No files selected for deletion.");
        return;
    }

    QMessageBox::StandardButton reply = QMessageBox::question(
        m_mainWindow,
        "Confirm Delete",
        QString("Are you sure you want to delete %1 selected file(s)?\nTotal size: %2\n\nThis action cannot be undone.")
            .arg(selectedCount)
            .arg(formatFileSize(totalSize)),
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::No)
        return;

    int deletedCount = 0;
    int failedCount = 0;

    for (const auto &file : files)
    {
        if (file.isSelected)
        {
            QFile qfile(file.path);
            if (qfile.remove())
            {
                deletedCount++;
            }
            else
            {
                failedCount++;
                qDebug() << "Failed to delete:" << file.path << "Error:" << qfile.errorString();
            }
        }
    }

    QString result;
    if (deletedCount > 0)
    {
        result += QString("Successfully deleted %1 file(s).\n").arg(deletedCount);
    }
    if (failedCount > 0)
    {
        result += QString("Failed to delete %1 file(s).\n").arg(failedCount);
    }

    QMessageBox::information(m_mainWindow, "Delete Complete", result);
}

void FilesChecker::openFileLocation(const QString &filePath)
{
    QFileInfo fileInfo(filePath);
    QString directoryPath = fileInfo.absolutePath();

    if (QFileInfo::exists(directoryPath))
    {
        QUrl url = QUrl::fromLocalFile(directoryPath);
        if (!QDesktopServices::openUrl(url))
        {
            QMessageBox::warning(m_mainWindow, "Open Location",
                                 QString("Could not open file location:\n%1").arg(directoryPath));
        }
    }
    else
    {
        QMessageBox::warning(m_mainWindow, "Open Location",
                             QString("Directory does not exist:\n%1").arg(directoryPath));
    }
}

void FilesChecker::refreshDiskSpace()
{
    if (!m_mainWindow || !m_mainWindow->ui)
        return;

    // Get disk space information using QStorageInfo
    QList<QStorageInfo> drives = QStorageInfo::mountedVolumes();

    // Clear previous disk widgets
    QLayout *layout = m_mainWindow->ui->diskSpaceContainer->layout();
    if (layout)
    {
        QLayoutItem *item;
        while ((item = layout->takeAt(0)) != nullptr)
        {
            if (item->widget())
            {
                item->widget()->deleteLater();
            }
            delete item;
        }
    }

    for (const QStorageInfo &drive : drives)
    {
        if (drive.isValid() && drive.isReady())
        {
            QString name = drive.name();
            if (name.isEmpty())
                name = "Local Disk";

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
                "}");

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
            if (percentUsed > 90)
            {
                progressStyle = "QProgressBar { border: 1px solid #e74c3c; border-radius: 7px; background-color: #f5b7b1; }"
                                "QProgressBar::chunk { background-color: #e74c3c; border-radius: 6px; }";
            }
            else if (percentUsed > 70)
            {
                progressStyle = "QProgressBar { border: 1px solid #e67e22; border-radius: 7px; background-color: #fad7a0; }"
                                "QProgressBar::chunk { background-color: #e67e22; border-radius: 6px; }";
            }
            else
            {
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
                    .arg(100 - percentUsed));
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
    static_cast<QVBoxLayout *>(m_mainWindow->ui->diskSpaceContainer->layout())->addStretch();
}

QStringList FilesChecker::getCommonPaths()
{
    QStringList paths;

    // System drives
    QList<QStorageInfo> drives = QStorageInfo::mountedVolumes();
    for (const QStorageInfo &drive : drives)
    {
        if (drive.isValid() && drive.isReady())
        {
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
    if (!m_mainWindow || !m_mainWindow->ui)
        return;

    QVector<FileInfo> results;
    if (m_largeFilesWatcher->isCanceled())
    {
        m_mainWindow->ui->largeFilesResults->setPlainText("Scan was cancelled.");
    }
    else
    {
        try
        {
            results = m_largeFilesWatcher->result();

            qDebug() << "Scan completed with" << results.size() << "files found";

            // Sort by size (descending)
            std::sort(results.begin(), results.end(),
                      [](const FileInfo &a, const FileInfo &b)
                      { return a.size > b.size; });

            // Update UI with results
            m_mainWindow->ui->largeFilesTable->setRowCount(0);
            m_mainWindow->ui->largeFilesTable->setSortingEnabled(false);

            for (int i = 0; i < results.size(); ++i)
            {
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
        }
        catch (const std::exception &e)
        {
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
    if (!m_mainWindow || !m_mainWindow->ui)
        return;

    QVector<DuplicateFile> results;
    if (m_duplicateFilesWatcher->isCanceled())
    {
        m_mainWindow->ui->duplicateFilesResults->setPlainText("❌ Scan was cancelled by user.");
    }
    else
    {
        results = m_duplicateFilesWatcher->result();

        // Update UI with results
        m_mainWindow->ui->duplicateFilesTree->clear();

        int totalDuplicates = 0;
        qint64 totalWastedSpace = 0;
        int totalFilesScanned = 0;

        // Calculate totals for summary
        for (const DuplicateFile &duplicate : results)
        {
            totalDuplicates += duplicate.files.size() - 1;
            totalWastedSpace += (duplicate.files.size() - 1) * duplicate.files.first().size;
            totalFilesScanned += duplicate.files.size();
        }

        for (int groupIndex = 0; groupIndex < results.size(); ++groupIndex)
        {
            const DuplicateFile &duplicate = results[groupIndex];

            // Create group header with clear information
            QTreeWidgetItem *groupItem = new QTreeWidgetItem(m_mainWindow->ui->duplicateFilesTree);
            groupItem->setText(0, ""); // No action for group header
            groupItem->setText(1, QString("📦 Duplicate Group %1 - %2 files (%3 each)")
                                      .arg(groupIndex + 1)
                                      .arg(duplicate.files.size())
                                      .arg(formatFileSize(duplicate.files.first().size)));
            groupItem->setText(2, formatFileSize(duplicate.totalSize));
            groupItem->setText(3, "");
            groupItem->setText(4, "");

            // Style group header
            QFont groupFont = groupItem->font(1);
            groupFont.setBold(true);
            groupFont.setPointSize(10);
            groupItem->setFont(1, groupFont);
            groupItem->setBackground(1, QBrush(QColor(233, 236, 239)));
            groupItem->setForeground(1, QBrush(QColor(33, 37, 41)));

            // Make group header non-selectable
            groupItem->setFlags(groupItem->flags() & ~Qt::ItemIsSelectable);
            groupItem->setData(0, Qt::UserRole, "group");

            // Sort files by modification date (newest first)
            QVector<FileInfo> sortedFiles = duplicate.files;
            std::sort(sortedFiles.begin(), sortedFiles.end(),
                      [](const FileInfo &a, const FileInfo &b)
                      {
                          return a.lastModified > b.lastModified; // Newest first
                      });

            for (int fileIndex = 0; fileIndex < sortedFiles.size(); ++fileIndex)
            {
                const FileInfo &file = sortedFiles[fileIndex];
                QTreeWidgetItem *fileItem = new QTreeWidgetItem(groupItem);

                QFileInfo fileInfo(file.path);
                QString fileName = fileInfo.fileName();
                QString fileExtension = fileInfo.suffix().toLower();

                // Determine file type icon
                QString fileIcon = "📄"; // Default file icon
                if (fileExtension == "exe")
                    fileIcon = "⚙️";
                else if (fileExtension == "pdf")
                    fileIcon = "📕";
                else if (fileExtension == "jpg" || fileExtension == "png" || fileExtension == "gif")
                    fileIcon = "🖼️";
                else if (fileExtension == "mp4" || fileExtension == "avi" || fileExtension == "mkv")
                    fileIcon = "🎬";
                else if (fileExtension == "mp3" || fileExtension == "wav")
                    fileIcon = "🎵";
                else if (fileExtension == "zip" || fileExtension == "rar")
                    fileIcon = "📦";
                else if (fileExtension == "doc" || fileExtension == "docx")
                    fileIcon = "📝";

                // Action column with clear checkbox
                if (fileIndex == 0)
                {
                    // Keep the newest file by default
                    fileItem->setText(0, "✅ Keep");
                    fileItem->setData(0, Qt::UserRole, true);
                    fileItem->setForeground(0, QBrush(QColor(40, 167, 69))); // Green
                }
                else
                {
                    // Mark others for deletion by default
                    fileItem->setText(0, "🗑️ Delete");
                    fileItem->setData(0, Qt::UserRole, false);
                    fileItem->setForeground(0, QBrush(QColor(220, 53, 69))); // Red
                }
                fileItem->setTextAlignment(0, Qt::AlignCenter);
                fileItem->setFont(0, QFont("Segoe UI Emoji", 9));

                // File name with icon
                fileItem->setText(1, QString("%1 %2").arg(fileIcon).arg(fileName));
                fileItem->setForeground(1, QBrush(Qt::black)); // ✅ Force black

                // Size - right aligned
                fileItem->setText(2, file.sizeFormatted);
                fileItem->setTextAlignment(2, Qt::AlignRight | Qt::AlignVCenter);

                // Modified date with clear formatting
                fileItem->setText(3, file.lastModified.toString("MMM d, yyyy • h:mm AP"));

                // Full path in hidden column
                fileItem->setText(4, file.path);

                // Make file items selectable
                fileItem->setFlags(fileItem->flags() | Qt::ItemIsEnabled | Qt::ItemIsSelectable);

                // Tooltip with full information
                QString tooltip = QString(
                                      "📋 File Information:\n"
                                      "• Name: %1\n"
                                      "• Path: %2\n"
                                      "• Size: %3\n"
                                      "• Modified: %4\n"
                                      "• Type: %5 file\n\n"
                                      "💡 Click the action column to toggle between 'Keep' and 'Delete'")
                                      .arg(fileName, file.path, file.sizeFormatted,
                                           file.lastModified.toString("MMMM d, yyyy 'at' h:mm:ss AP"),
                                           fileExtension.isEmpty() ? "Unknown" : fileExtension.toUpper());

                fileItem->setToolTip(0, tooltip);
                fileItem->setToolTip(1, tooltip);
                fileItem->setToolTip(2, tooltip);
                fileItem->setToolTip(3, tooltip);

                // ✅ Force every column to stay black
                for (int col = 0; col < 5; ++col)
                    fileItem->setForeground(col, QBrush(Qt::black));
            }

            // Expand the group to show all files
            groupItem->setExpanded(true);
        }

        // Auto-resize columns
        m_mainWindow->ui->duplicateFilesTree->resizeColumnToContents(0);
        m_mainWindow->ui->duplicateFilesTree->resizeColumnToContents(2);
        m_mainWindow->ui->duplicateFilesTree->resizeColumnToContents(3);

        // Create detailed results summary
        QString resultsText;
        if (results.isEmpty())
        {
            resultsText = "✅ No duplicate files found! Your files are well organized.";
        }
        else
        {
            resultsText = QString(
                              "🔍 **Scan Complete!**\n\n"
                              "📊 **Summary:**\n"
                              "• Files scanned: %1\n"
                              "• Duplicate groups found: %2\n"
                              "• Total duplicate files: %3\n"
                              "• Potential space savings: **%4**\n\n"
                              "💡 **How to proceed:**\n"
                              "1. Review each duplicate group below\n"
                              "2. Click the action column to toggle between 'Keep' and 'Delete'\n"
                              "3. Keep at least one file from each group\n"
                              "4. Click 'Delete Selected' when ready")
                              .arg(totalFilesScanned)
                              .arg(results.size())
                              .arg(totalDuplicates)
                              .arg(formatFileSize(totalWastedSpace));
        }

        m_mainWindow->ui->duplicateFilesResults->setPlainText(resultsText);
    }

    // Update UI state
    m_mainWindow->ui->scanDuplicateFilesButton->setEnabled(true);
    m_mainWindow->ui->cancelDuplicateFilesButton->setEnabled(false);
    m_mainWindow->ui->deleteDuplicateFilesButton->setEnabled(!results.isEmpty());
    m_cancelDuplicateFilesScan = false;
}

// Static helper methods
QVector<FileInfo> FilesChecker::performLargeFilesScan(const QString &path, qint64 minSizeBytes, QAtomicInteger<bool> &cancelFlag)
{
    QVector<FileInfo> results;
    QDir dir(path);

    if (!dir.exists() || cancelFlag)
        return results;

    // Get all files recursively
    QFileInfoList files = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden, QDir::Name);

    for (const QFileInfo &fileInfo : files)
    {
        if (cancelFlag)
            break;

        if (fileInfo.size() >= minSizeBytes)
        {
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
    if (!cancelFlag)
    {
        QFileInfoList subDirs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QFileInfo &subDir : subDirs)
        {
            if (cancelFlag)
                break;
            results.append(performLargeFilesScan(subDir.absoluteFilePath(), minSizeBytes, cancelFlag));
        }
    }

    return results;
}

QVector<DuplicateFile> FilesChecker::performDuplicateFilesScan(const QString &path, QAtomicInteger<bool> &cancelFlag)
{
    QVector<DuplicateFile> results;
    QHash<QString, QVector<FileInfo>> fileHashGroups;

    QDir dir(path);
    if (!dir.exists() || cancelFlag)
        return results;

    // First pass: collect all files and group by size (quick check)
    QHash<qint64, QVector<FileInfo>> sizeGroups;
    collectFilesBySize(path, sizeGroups, cancelFlag);

    if (cancelFlag)
        return results;

    // Second pass: for files with same size, calculate hash to find exact duplicates
    for (auto it = sizeGroups.begin(); it != sizeGroups.end() && !cancelFlag; ++it)
    {
        if (it->size() > 1)
        { // Only check files that have the same size
            for (const FileInfo &fileInfo : *it)
            {
                if (cancelFlag)
                    break;

                QString fileHash = calculateFileHash(fileInfo.path, cancelFlag);
                if (fileHash.isEmpty())
                    continue; // Skip if hash calculation failed or cancelled

                fileHashGroups[fileHash].append(fileInfo);
            }
        }
    }

    if (cancelFlag)
        return results;

    // Convert hash groups to duplicate file results
    for (auto it = fileHashGroups.begin(); it != fileHashGroups.end() && !cancelFlag; ++it)
    {
        if (it->size() > 1)
        { // Only include groups with duplicates
            DuplicateFile duplicate;
            duplicate.hash = it.key();
            duplicate.files = *it;
            duplicate.totalSize = 0;

            for (const FileInfo &file : duplicate.files)
            {
                duplicate.totalSize += file.size;
            }

            results.append(duplicate);
        }
    }

    return results;
}

QString FilesChecker::calculateFileHash(const QString &filePath, QAtomicInteger<bool> &cancelFlag)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
    {
        qDebug() << "Failed to open file for hashing:" << filePath << file.errorString();
        return QString();
    }

    QCryptographicHash hash(QCryptographicHash::Md5);
    const qint64 bufferSize = 8192;
    QByteArray buffer(bufferSize, 0);

    while (!file.atEnd() && !cancelFlag)
    {
        qint64 bytesRead = file.read(buffer.data(), bufferSize);
        if (bytesRead > 0)
        {
            hash.addData(QByteArrayView(buffer.constData(), bytesRead));
        }
        else
        {
            break; // Error reading file
        }
    }

    file.close();

    if (cancelFlag)
    {
        return QString();
    }

    return hash.result().toHex();
}

QString FilesChecker::formatFileSize(qint64 size)
{
    if (size < 1024)
        return QString("%1 B").arg(size);
    else if (size < 1024 * 1024)
        return QString("%1 KB").arg(size / 1024.0, 0, 'f', 1);
    else if (size < 1024 * 1024 * 1024)
        return QString("%1 MB").arg(size / (1024.0 * 1024.0), 0, 'f', 1);
    else
        return QString("%1 GB").arg(size / (1024.0 * 1024.0 * 1024.0), 0, 'f', 1);
}

void FilesChecker::openFileDirectory(const QString &filePath)
{
    QFileInfo fileInfo(filePath);
    QString directoryPath = fileInfo.absolutePath();

    if (QFileInfo::exists(directoryPath))
    {
        QUrl url = QUrl::fromLocalFile(directoryPath);
        if (!QDesktopServices::openUrl(url))
        {
            QMessageBox::warning(m_mainWindow, "Open Location",
                                 QString("Could not open file location:\n%1").arg(directoryPath));
        }
    }
    else
    {
        QMessageBox::warning(m_mainWindow, "Open Location",
                             QString("Directory does not exist:\n%1").arg(directoryPath));
    }
}

void FilesChecker::collectFilesBySize(const QString &path, QHash<qint64, QVector<FileInfo>> &sizeGroups, QAtomicInteger<bool> &cancelFlag)
{
    QDir dir(path);
    if (!dir.exists() || cancelFlag)
        return;

    // Get all files in current directory
    QFileInfoList files = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System, QDir::Name);

    for (const QFileInfo &fileInfo : files)
    {
        if (cancelFlag)
            break;

        FileInfo file;
        file.path = fileInfo.absoluteFilePath();
        file.size = fileInfo.size();
        file.sizeFormatted = formatFileSize(file.size);
        file.lastModified = fileInfo.lastModified();
        file.isSelected = false;

        sizeGroups[file.size].append(file);
    }

    // Recursively process subdirectories if not cancelled
    if (!cancelFlag)
    {
        QFileInfoList subDirs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QFileInfo &subDir : subDirs)
        {
            if (cancelFlag)
                break;
            collectFilesBySize(subDir.absoluteFilePath(), sizeGroups, cancelFlag);
        }
    }
}

void FilesChecker::deleteSelectedDuplicateFiles()
{
    if (!m_mainWindow || !m_mainWindow->ui)
        return;

    QTreeWidget *tree = m_mainWindow->ui->duplicateFilesTree;
    QVector<FileInfo> filesToDelete;

    // Collect all checked files (keep one from each group)
    for (int i = 0; i < tree->topLevelItemCount(); ++i)
    {
        QTreeWidgetItem *groupItem = tree->topLevelItem(i);
        bool firstFileKept = false;

        for (int j = 0; j < groupItem->childCount(); ++j)
        {
            QTreeWidgetItem *fileItem = groupItem->child(j);
            if (fileItem->checkState(0) == Qt::Checked)
            {
                if (!firstFileKept)
                {
                    // Keep the first checked file in each group
                    firstFileKept = true;
                }
                else
                {
                    // Delete additional checked files in the same group
                    FileInfo fileInfo;
                    fileInfo.path = fileItem->text(0);
                    fileInfo.isSelected = true;
                    filesToDelete.append(fileInfo);
                }
            }
        }
    }

    if (filesToDelete.isEmpty())
    {
        QMessageBox::information(m_mainWindow, "Delete Duplicate Files",
                                 "No duplicate files selected for deletion.\n\n"
                                 "Please check the files you want to delete (keep at least one file from each duplicate group).");
        return;
    }

    // Show confirmation dialog
    QMessageBox::StandardButton reply = QMessageBox::question(
        m_mainWindow,
        "Confirm Delete",
        QString("Are you sure you want to delete %1 duplicate file(s)?\n\n"
                "This action cannot be undone.")
            .arg(filesToDelete.size()),
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::No)
        return;

    // Delete the files
    int deletedCount = 0;
    int failedCount = 0;

    for (const auto &file : filesToDelete)
    {
        QFile qfile(file.path);
        if (qfile.remove())
        {
            deletedCount++;
            // Remove from tree
            for (int i = 0; i < tree->topLevelItemCount(); ++i)
            {
                QTreeWidgetItem *groupItem = tree->topLevelItem(i);
                for (int j = 0; j < groupItem->childCount(); ++j)
                {
                    QTreeWidgetItem *fileItem = groupItem->child(j);
                    if (fileItem->text(0) == file.path)
                    {
                        delete fileItem;
                        break;
                    }
                }
            }
        }
        else
        {
            failedCount++;
            qDebug() << "Failed to delete:" << file.path << "Error:" << qfile.errorString();
        }
    }

    // Show result
    QString result;
    if (deletedCount > 0)
    {
        result += QString("Successfully deleted %1 duplicate file(s).\n").arg(deletedCount);
    }
    if (failedCount > 0)
    {
        result += QString("Failed to delete %1 file(s).\n").arg(failedCount);
    }

    QMessageBox::information(m_mainWindow, "Delete Complete", result);

    // Update the delete button state
    m_mainWindow->ui->deleteDuplicateFilesButton->setEnabled(false);
}
void FilesChecker::setupDuplicateFilesTree()
{
    if (!m_mainWindow || !m_mainWindow->ui)
        return;

    QTreeWidget *tree = m_mainWindow->ui->duplicateFilesTree;

    // Clear and setup columns
    tree->setColumnCount(5); // Action, File Name, Size, Modified, Path

    // Set clear column headers
    QStringList headers;
    headers << "Select" << "File Name" << "Size" << "Modified" << "Full Path";
    tree->setHeaderLabels(headers);

    // Set column widths
    tree->setColumnWidth(0, 80);  // Select column - wider for clear icons
    tree->setColumnWidth(1, 350); // File Name
    tree->setColumnWidth(2, 100); // Size
    tree->setColumnWidth(3, 150); // Modified Date
    tree->setColumnWidth(4, 400); // Full Path (hidden)

    // Hide full path column
    tree->setColumnHidden(4, true);

    // Improved styling
    tree->setStyleSheet(
        "QTreeWidget {"
        "    font-family: Segoe UI;"
        "    font-size: 9pt;"
        "    background-color: #fafafa;"
        "    alternate-background-color: #f8f9fa;"
        "}"
        "QTreeWidget::item {"
        "    padding: 6px 2px;"
        "    border-bottom: 1px solid #e9ecef;"
        "}"
        "QTreeWidget::item:selected {"
        "    background-color: #e3f2fd;"
        "    color: #1976d2;"
        "    border: 1px solid #bbdefb;"
        "}"
        "QTreeWidget::item:hover {"
        "    background-color: #f1f8ff;"
        "}"
        "QHeaderView::section {"
        "    background-color: #2c3e50;"
        "    color: white;"
        "    padding: 6px;"
        "    border: 1px solid #34495e;"
        "    font-weight: bold;"
        "}");

    // Enable alternating row colors
    tree->setAlternatingRowColors(true);

    // Better header properties
    tree->header()->setStretchLastSection(false);
    tree->header()->setSectionResizeMode(1, QHeaderView::Stretch); // File name stretches
    tree->header()->setDefaultAlignment(Qt::AlignLeft);
    tree->header()->setSectionsClickable(true);
}