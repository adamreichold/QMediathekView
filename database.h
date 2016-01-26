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

public:
    enum SortField
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
        const SortField sortField, const Qt::SortOrder sortOrder
    ) const;

    Show fetchShow(const quintptr id) const;

    QStringList channels() const;
    QStringList topics() const;
    QStringList topics(const QString& channel) const;

signals:
    void updated();
    void failedToUpdate(const QString& error);

public slots:
    void update(const QByteArray& data);

private:
    const Settings& m_settings;

    mutable QSqlDatabase m_database;

};

} // Mediathek

#endif // DATABASE_H
