#include "cleaneritem.h"

CleanerItem::CleanerItem()
    : m_size(0), m_selected(false), m_safe(true)
{
}

CleanerItem::CleanerItem(const QString &name, const QString &description, const QString &path, 
                         const QStringList &filePatterns, qint64 size, bool isSelected, bool isSafe)
    : m_name(name), m_description(description), m_path(path), m_filePatterns(filePatterns),
      m_size(size), m_selected(isSelected), m_safe(isSafe)
{
}

QString CleanerItem::name() const
{
    return m_name;
}

QString CleanerItem::description() const
{
    return m_description;
}

QString CleanerItem::path() const
{
    return m_path;
}

QStringList CleanerItem::filePatterns() const
{
    return m_filePatterns;
}

qint64 CleanerItem::size() const
{
    return m_size;
}

bool CleanerItem::isSelected() const
{
    return m_selected;
}

bool CleanerItem::isSafe() const
{
    return m_safe;
}

void CleanerItem::setName(const QString &name)
{
    m_name = name;
}

void CleanerItem::setDescription(const QString &description)
{
    m_description = description;
}

void CleanerItem::setPath(const QString &path)
{
    m_path = path;
}

void CleanerItem::setFilePatterns(const QStringList &filePatterns)
{
    m_filePatterns = filePatterns;
}

void CleanerItem::setSize(qint64 size)
{
    m_size = size;
}

void CleanerItem::setSelected(bool selected)
{
    m_selected = selected;
}

void CleanerItem::setSafe(bool safe)
{
    m_safe = safe;
}