#ifndef SYSTEMCLEANER_H
#define SYSTEMCLEANER_H

#include <QObject>
#include <QFutureWatcher>
#include <QVector>

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
    void performBrowserScan();
    void performBrowserClean();

private slots:
    void onQuickScanFinished();
    void updateScanProgress(int value);

private:
    MainWindow *m_mainWindow;
    QFutureWatcher<QVector<double>> *m_scanWatcher;
    bool m_isScanning;

    static QVector<double> performScan();
    void setupConnections();
};

#endif // SYSTEMCLEANER_H