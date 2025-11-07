#include "mainwindow.h"
#include <QApplication>
#include "cleaneritem.h"  // Include the common header

// Register CleanerItem type for QVariant
Q_DECLARE_METATYPE(CleanerItem)

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    
    // Register the type
    qRegisterMetaType<CleanerItem>();
    
    MainWindow w;
    w.show();
    return a.exec();
}