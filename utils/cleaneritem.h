#ifndef CLEANERITEM_H
#define CLEANERITEM_H

#include <QString>
#include <QStringList>

class CleanerItem
{
public:
    CleanerItem();
    CleanerItem(const QString &name, const QString &description, const QString &path, 
                const QStringList &filePatterns, qint64 size, bool isSelected, bool isSafe);

    // Getters
    QString name() const;
    QString description() const;
    QString path() const;
    QStringList filePatterns() const;
    qint64 size() const;
    bool isSelected() const;
    bool isSafe() const;

    // Setters
    void setName(const QString &name);
    void setDescription(const QString &description);
    void setPath(const QString &path);
    void setFilePatterns(const QStringList &filePatterns);
    void setSize(qint64 size);
    void setSelected(bool selected);
    void setSafe(bool safe);

private:
    QString m_name;
    QString m_description;
    QString m_path;
    QStringList m_filePatterns;
    qint64 m_size;
    bool m_selected;
    bool m_safe;
};

#endif // CLEANERITEM_H