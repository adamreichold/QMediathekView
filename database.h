#ifndef DATABASE_H
#define DATABASE_H

#include <QDate>
#include <QObject>
#include <QSqlDatabase>
#include <QTime>
#include <QUrl>

#include "schema.h"

namespace Mediathek
{

class Settings;

class Database : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(Database)

public:
    Database(const Settings& settings, QObject* parent = 0);
    ~Database();

signals:
    void updated();
    void failedToUpdate(const QString& error);

public:
    void fullUpdate(const QByteArray& data);
    void partialUpdate(const QByteArray& data);

public:
    enum SortBy
    {
        SortByChannel,
        SortByTopic,
        SortByTitle,
        SortByDate,
        SortByTime,
        SortByDuration
    };

    QVector< quintptr > fetchId(
        const QString& channel, const QString& topic, const QString& title,
        const SortBy sortBy, const Qt::SortOrder sortOrder
    ) const;

    Show fetchShow(const quintptr id) const;

public:
    QStringList channels() const;
    QStringList topics(const QString& channel) const;

private:
    const Settings& m_settings;

    mutable QSqlDatabase m_database;

};

} // Mediathek

#endif // DATABASE_H
