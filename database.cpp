/*

Copyright 2016 Adam Reichold

This file is part of QMediathekView.

QMediathekView is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

QMediathekView is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with QMediathekView.  If not, see <http://www.gnu.org/licenses/>.

*/

#include "database.h"

#include <QDebug>
#include <QDir>
#include <QStandardPaths>
#include <QSqlError>
#include <QSqlQuery>
#include <QTime>

#include <QtConcurrentRun>

#include "settings.h"
#include "parser.h"

namespace
{

using namespace Mediathek;

const auto databaseType = QStringLiteral("QSQLITE");
const auto databaseName = QStringLiteral("database");

class Transaction
{
public:
    Transaction(QSqlDatabase& database)
        : m_database(database)
        , m_committed(false)
    {
        if (!m_database.transaction())
        {
            throw m_database.lastError();
        }
    }

    ~Transaction() noexcept
    {
        if (!m_committed)
        {
            m_database.rollback();
        }
    }

    void commit()
    {
        if (!m_database.commit())
        {
            throw m_database.lastError();
        }

        m_committed = true;
    }

private:
    Q_DISABLE_COPY(Transaction)

    QSqlDatabase& m_database;
    bool m_committed;

};

class Query
{
public:
    Query(QSqlDatabase& database)
        : m_query(database)
        , m_bindValueIndex(0)
        , m_valueIndex(0)
    {
    }

    void prepare(const QString& query)
    {
        if (!m_query.prepare(query))
        {
            throw m_query.lastError();
        }

        m_bindValueIndex = 0;
    }

    void exec()
    {
        if (!m_query.exec())
        {
            throw m_query.lastError();
        }

        m_bindValueIndex = 0;
    }

    void exec(const QString& query)
    {
        if (!m_query.exec(query))
        {
            throw m_query.lastError();
        }

        m_bindValueIndex = 0;
    }

    Query& operator <<(const QVariant& value)
    {
        m_query.bindValue(m_bindValueIndex++, value);

        return *this;
    }

    bool nextRecord()
    {
        if (!m_query.isActive())
        {
            throw m_query.lastError();
        }

        m_valueIndex = 0;

        return m_query.next();
    }

    template< typename T >
    T nextValue()
    {
        return m_query.value(m_valueIndex++).value< T >();
    }

private:
    Q_DISABLE_COPY(Query)

    QSqlQuery m_query;
    int m_bindValueIndex;
    int m_valueIndex;

};

} // anonymous

namespace Mediathek
{

Database::Database(const Settings& settings, QObject* parent)
    : QObject(parent)
    , m_settings(settings)
    , m_database(QSqlDatabase::addDatabase(databaseType))
{
    const auto path = QStandardPaths::writableLocation(QStandardPaths::DataLocation);
    QDir().mkpath(path);
    m_database.setDatabaseName(QDir(path).filePath(databaseName));

    if (!m_database.open())
    {
        qDebug() << m_database.lastError();
        return;
    }

    try
    {
        Query query(m_database);

        query.exec(QStringLiteral(
                       "CREATE TABLE IF NOT EXISTS shows ("
                       " id INTEGER PRIMARY KEY AUTOINCREMENT,"
                       " channel TEXT,"
                       " topic TEXT,"
                       " title TEXT,"
                       " date INTEGER,"
                       " time INTEGER,"
                       " duration INTEGER,"
                       " description TEXT,"
                       " website TEXT,"
                       " url TEXT,"
                       " urlSmall TEXT,"
                       " urlLarge TEXT)"));

        query.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS showsByChannel ON shows (channel COLLATE NOCASE)"));
        query.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS showsByTopic ON shows (topic COLLATE NOCASE)"));
        query.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS showsByTitle ON shows (title COLLATE NOCASE)"));

        query.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS showsByDateAndTime ON shows (date DESC, time DESC)"));

        query.exec(QStringLiteral("CREATE UNIQUE INDEX IF NOT EXISTS showsByChannelTopicTitleAndUrl ON shows (channel, topic, title, url)"));
    }
    catch (QSqlError& error)
    {
        qDebug() << error;
    }
}

Database::~Database()
{
}

void Database::fullUpdate(const QByteArray& data)
{
    QtConcurrent::run([this, data]()
    {
        try
        {
            Transaction transaction(m_database);

            Query deleteAllShows(m_database);
            deleteAllShows.exec(QStringLiteral("DELETE FROM shows"));

            Query insertShow(m_database);
            insertShow.prepare(QStringLiteral(
                                   "INSERT INTO shows ("
                                   " channel, topic, title,"
                                   " date, time,"
                                   " duration,"
                                   " description, website,"
                                   " url, urlSmall, urlLarge)"
                                   " VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));

            const auto processor = [&insertShow](const Show& show)
            {
                insertShow << show.channel << show.topic << show.title
                           << show.date.toJulianDay() << show.time.msecsSinceStartOfDay()
                           << show.duration.msecsSinceStartOfDay()
                           << show.description << show.website
                           << show.url << show.urlSmall << show.urlLarge;

                insertShow.exec();
            };

            if (!parse(data, processor))
            {
                emit failedToUpdate(tr("Could not parse data."));
                return;
            }

            transaction.commit();

            m_settings.setDatabaseUpdatedOn();

            emit updated();
        }
        catch (QSqlError& error)
        {
            qDebug() << error;
        }
    });
}


void Database::partialUpdate(const QByteArray& data)
{
    QtConcurrent::run([this, data]()
    {
        try
        {
            Transaction transaction(m_database);

            Query deleteShow(m_database);
            deleteShow.prepare(QStringLiteral(
                                   "DELETE FROM shows"
                                   " WHERE channel = ?"
                                   " AND topic = ?"
                                   " AND title = ?"
                                   " AND url = ?"));

            Query insertShow(m_database);
            insertShow.prepare(QStringLiteral(
                                   "INSERT INTO shows ("
                                   " channel, topic, title,"
                                   " date, time,"
                                   " duration,"
                                   " description, website,"
                                   " url, urlSmall, urlLarge)"
                                   " VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));

            const auto processor = [&deleteShow, &insertShow](const Show& show)
            {
                deleteShow << show.channel << show.topic << show.title << show.url;

                deleteShow.exec();

                insertShow << show.channel << show.topic << show.title
                           << show.date.toJulianDay() << show.time.msecsSinceStartOfDay()
                           << show.duration.msecsSinceStartOfDay()
                           << show.description << show.website
                           << show.url << show.urlSmall << show.urlLarge;

                insertShow.exec();
            };

            if (!parse(data, processor))
            {
                emit failedToUpdate(tr("Could not parse data."));
                return;
            }

            transaction.commit();

            m_settings.setDatabaseUpdatedOn();

            emit updated();
        }
        catch (QSqlError& error)
        {
            qDebug() << error;
        }
    });
}

QVector< quintptr > Database::fetchId(
    const QString& channel, const QString& topic, const QString& title,
    const SortBy sortBy, const Qt::SortOrder sortOrder) const
{
    QVector< quintptr > id;

    QString sortOrderClause;

    switch (sortOrder)
    {
    default:
    case Qt::AscendingOrder:
        break;
    case Qt::DescendingOrder:
        sortOrderClause = QStringLiteral("DESC");
        break;
    }

    QString sortClause;

    switch (sortBy)
    {
    default:
    case SortByChannel:
        sortClause = QStringLiteral("channel %1, date DESC, time DESC").arg(sortOrderClause);
        break;
    case SortByTopic:
        sortClause = QStringLiteral("topic %1, date DESC, time DESC").arg(sortOrderClause);
        break;
    case SortByTitle:
        sortClause = QStringLiteral("title %1, date DESC, time DESC").arg(sortOrderClause);
        break;
    case SortByDate:
        sortClause = QStringLiteral("date %1").arg(sortOrderClause);
        break;
    case SortByTime:
        sortClause = QStringLiteral("time %1").arg(sortOrderClause);
        break;
    case SortByDuration:
        sortClause = QStringLiteral("duration %1").arg(sortOrderClause);
        break;
    }

    const auto channelFilterClause = channel.isEmpty() ? QStringLiteral("ifnull(1, ?)")
                                     : QStringLiteral("channel LIKE ('%' || ? || '%')");

    const auto topicFilterClause = topic.isEmpty() ? QStringLiteral("ifnull(1, ?)")
                                   : QStringLiteral("topic LIKE ('%' || ? || '%')");

    const auto titleFilterCaluse = title.isEmpty() ? QStringLiteral("ifnull(1, ?)")
                                   : QStringLiteral("title LIKE ('%' || ? || '%')");

    try
    {
        Query query(m_database);

        query.prepare(QStringLiteral("SELECT id FROM shows WHERE %1 AND %2 AND %3 ORDER BY %4")
                      .arg(channelFilterClause)
                      .arg(topicFilterClause)
                      .arg(titleFilterCaluse)
                      .arg(sortClause));

        query << channel << topic << title;

        query.exec();

        while (query.nextRecord())
        {
            id.append(query.nextValue< quintptr >());
        }
    }
    catch (QSqlError& error)
    {
        qDebug() << error;
    }

    return id;
}

std::unique_ptr< Show > Database::fetchShow(const quintptr id) const
{
    std::unique_ptr< Show > show(new Show);

    try
    {
        Query query(m_database);

        query.prepare(QStringLiteral(
                          "SELECT"
                          " channel, topic, title,"
                          " date, time,"
                          " duration,"
                          " description, website,"
                          " url, urlSmall, urlLarge"
                          " FROM shows WHERE id = ?"));

        query << id;

        query.exec();

        if (query.nextRecord())
        {
            show->channel = query.nextValue< QString >();
            show->topic = query.nextValue< QString >();
            show->title = query.nextValue< QString >();

            show->date =  QDate::fromJulianDay(query.nextValue< qint64 >());
            show->time = QTime::fromMSecsSinceStartOfDay(query.nextValue< int >());

            show->duration = QTime::fromMSecsSinceStartOfDay(query.nextValue< int >());

            show->description = query.nextValue< QString >();
            show->website = query.nextValue< QString >();

            show->url = query.nextValue< QString >();
            show->urlSmall = query.nextValue< QString >();
            show->urlLarge = query.nextValue< QString >();
        }
    }
    catch (QSqlError& error)
    {
        qDebug() << error;
    }

    return std::move(show);
}

QStringList Database::channels() const
{
    QStringList channels;

    try
    {
        Query query(m_database);

        query.exec(QStringLiteral("SELECT DISTINCT(channel) FROM shows"));

        while (query.nextRecord())
        {
            channels.append(query.nextValue< QString >());
        }
    }
    catch (QSqlError& error)
    {
        qDebug() << error;
    }

    return channels;
}

QStringList Database::topics(const QString& channel) const
{
    QStringList topics;

    const auto filterClause = channel.isEmpty() ? QStringLiteral("ifnull(1, ?)")
                              : QStringLiteral("channel LIKE ('%' || ? || '%')");

    try
    {
        Query query(m_database);

        query.prepare(QStringLiteral("SELECT DISTINCT(topic) FROM shows WHERE %1").arg(filterClause));

        query << channel;

        query.exec();

        while (query.nextRecord())
        {
            topics.append(query.nextValue< QString >());
        }
    }
    catch (QSqlError& error)
    {
        qDebug() << error;
    }

    return topics;
}

} // Mediathek
