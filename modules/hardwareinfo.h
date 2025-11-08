#ifndef HARDWAREINFO_H
#define HARDWAREINFO_H

#include <QObject>

class MainWindow;

class HardwareInfo : public QObject
{
    Q_OBJECT

public:
    explicit HardwareInfo(MainWindow *mainWindow, QObject *parent = nullptr);
    
    void refreshHardware();
    void exportHardware();

private:
    MainWindow *m_mainWindow;
};

#endif // HARDWAREINFO_H