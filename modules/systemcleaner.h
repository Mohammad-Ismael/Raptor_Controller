#ifndef SYSTEMCLEANER_H
#define SYSTEMCLEANER_H

#include <QObject>
#include <QFutureWatcher>
#include <QVector>
#include <QListWidgetItem>
#include <QProgressDialog>

class MainWindow;

class SystemCleaner : public QObject
{
    Q_OBJECT

public:
    explicit SystemCleaner(MainWindow *mainWindow, QObject *parent = nullptr);
    ~SystemCleaner();

    void performQuickScan();
    void performQuickClean();
    void performSystemScan();
    void performSystemClean();

private slots:
    void onQuickScanFinished();
    void updateScanProgress(int value);
    void onSystemScanFinished();

private:
    QString getCategoryDescription(const QString& category);

    MainWindow *m_mainWindow;
    QFutureWatcher<QVector<double>> *m_scanWatcher;
    QFutureWatcher<QVector<double>> *m_systemScanWatcher;
    bool m_isScanning;
    bool m_isSystemScanning;

    static QVector<double> performScan();
    static QVector<double> performSystemScanInternal();
    void setupConnections();
    void updateSystemScanResults(const QVector<double>& results);
    void performSystemCleanInternal(QList<QListWidgetItem*> selectedItems, QProgressDialog* progress);
};

#endif // SYSTEMCLEANER_H