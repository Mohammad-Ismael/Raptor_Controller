#ifndef CLEANERITEM_H
#define CLEANERITEM_H

#include <QString>
#include <QStringList>

struct CleanerItem {
    QString name;
    QString description;
    QString path;
    QStringList filePatterns;
    qint64 size;
    bool isSafe;
    bool isSelected;
};

#endif // CLEANERITEM_H