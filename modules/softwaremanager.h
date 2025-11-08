#ifndef SOFTWAREUNINSTALLER_H
#define SOFTWAREUNINSTALLER_H

#include <QObject>

class MainWindow;

class SoftwareManager : public QObject
{
    Q_OBJECT

public:
    explicit SoftwareManager(MainWindow *mainWindow, QObject *parent = nullptr);
    
    void refreshSoftware();
    void searchSoftware(const QString &searchText);
    void onSoftwareSelectionChanged();
    void uninstallSoftware();
    void forceUninstallSoftware();
    void populateSoftwareTable();

private:
    MainWindow *m_mainWindow;
};

#endif // SOFTWAREUNINSTALLER_H