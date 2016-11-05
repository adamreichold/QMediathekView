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

#include <QCryptographicHash>
#include <QDebug>
#include <QStandardPaths>
#include <QSqlError>
#include <QSqlQuery>

#include <QtConcurrentRun>

#include "settings.h"
#include "parser.h"

namespace QMediathekView
{

namespace
{

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

    template< typename Type >
    Type nextValue()
    {
        return m_query.value(m_valueIndex++).value< Type >();
    }

private:
    Q_DISABLE_COPY(Query)

    QSqlQuery m_query;
    int m_bindValueIndex;
    int m_valueIndex;

};

namespace Queries
{

#define DEFINE_QUERY(name, text) const auto name = QStringLiteral(text)

DEFINE_QUERY(createShows,
             "CREATE TABLE IF NOT EXISTS shows ("
             " id INTEGER PRIMARY KEY AUTOINCREMENT,"
             " key BLOB,"
             " channel TEXT,"
             " topic TEXT,"
             " title TEXT,"
             " date INTEGER,"
             " time INTEGER,"
             " duration INTEGER,"
             " description TEXT,"
             " website TEXT,"
             " url TEXT,"
             " urlSmallOffset INTEGER,"
             " urlSmallSuffix TEXT,"
             " urlLargeOffset INTEGER,"
             " urlLargeSuffix TEXT)");

DEFINE_QUERY(truncateShows, "DELETE FROM shows");

DEFINE_QUERY(createShowsByKey, "CREATE UNIQUE INDEX IF NOT EXISTS showsByKey ON shows (key)");

DEFINE_QUERY(createShowsByText,
             "CREATE VIRTUAL TABLE IF NOT EXISTS showsByText USING FTS5 ("
             " channel,"
             " topic,"
             " title,"
             " content=shows)");

DEFINE_QUERY(rebuildShowsByText, "INSERT INTO showsByText(showsByText) VALUES('rebuild')");

DEFINE_QUERY(deleteShow, "DELETE FROM shows WHERE key = ?");

DEFINE_QUERY(insertShow,
             "INSERT OR IGNORE INTO shows ("
             " key,"
             " channel, topic, title,"
             " date, time,"
             " duration,"
             " description, website,"
             " url,"
             " urlSmallOffset, urlSmallSuffix,"
             " urlLargeOffset, urlLargeSuffix)"
             " VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");

DEFINE_QUERY(selectShow,
             "SELECT"
             " channel, topic, title,"
             " date, time,"
             " duration,"
             " description, website,"
             " url,"
             " urlSmallOffset, urlSmallSuffix,"
             " urlLargeOffset, urlLargeSuffix"
             " FROM shows WHERE id = ?");

DEFINE_QUERY(selectChannels, "SELECT DISTINCT(channel) FROM showsByText");

DEFINE_QUERY(selectTopics, "SELECT DISTINCT(topic) FROM showsByText");

DEFINE_QUERY(selectTopicsByChannel, "SELECT DISTINCT(topic) FROM showsByText WHERE showsByText MATCH 'channel : \"' || ? || '\"'");

#undef DEFINE_QUERY

}

QByteArray keyOf(const Show& show)
{
    QCryptographicHash hash(QCryptographicHash::Md5);

    const auto addText = [&hash](const QString& text)
    {
        hash.addData(reinterpret_cast< const char* >(text.constData()), text.size() * sizeof(QChar));
    };

    addText(show.channel);
    addText(show.topic);
    addText(show.title);

    addText(show.url);

    return hash.result();
}

void bindTo(Query& query, const QByteArray& key, const Show& show)
{
    query << key
          << show.channel << show.topic << show.title
          << show.date.toJulianDay() << show.time.msecsSinceStartOfDay()
          << show.duration.msecsSinceStartOfDay()
          << show.description << show.website
          << show.url
          << show.urlSmallOffset << show.urlSmallSuffix
          << show.urlLargeOffset << show.urlLargeSuffix;
}

class FullUpdate : public Processor
{
public:
    FullUpdate(QSqlDatabase& database)
        : m_transaction(database)
        , m_insertShow(database)
    {
        Query(database).exec(Queries::truncateShows);

        m_insertShow.prepare(Queries::insertShow);
    }

    void operator()(const Show& show) override
    {
        const auto key = keyOf(show);

        bindTo(m_insertShow, key, show);

        m_insertShow.exec();
    }

    void commit()
    {
        m_transaction.commit();
    }

private:
    Transaction m_transaction;
    Query m_insertShow;

};

class PartialUpdate : public Processor
{
public:
    PartialUpdate(QSqlDatabase& database)
        : m_transaction(database)
        , m_deleteShow(database)
        , m_insertShow(database)
    {
        m_deleteShow.prepare(Queries::deleteShow);
        m_insertShow.prepare(Queries::insertShow);
    }

    void operator()(const Show& show) override
    {
        const auto key = keyOf(show);

        m_deleteShow << key;

        m_deleteShow.exec();

        bindTo(m_insertShow, key, show);

        m_insertShow.exec();
    }

    void commit()
    {
        m_transaction.commit();
    }

private:
    Transaction m_transaction;
    Query m_deleteShow;
    Query m_insertShow;

};

} // anonymous

struct Database::PreparedQueries
{
    PreparedQueries(QSqlDatabase& database)
        : selectShow(database)
        , selectChannels(database)
        , selectTopics(database)
        , selectTopicsByChannel(database)
    {
        selectShow.prepare(Queries::selectShow);

        selectChannels.prepare(Queries::selectChannels);
        selectTopics.prepare(Queries::selectTopics);
        selectTopicsByChannel.prepare(Queries::selectTopicsByChannel);
    }

    Query selectShow;

    Query selectChannels;
    Query selectTopics;
    Query selectTopicsByChannel;
};

Database::Database(Settings& settings, QObject* parent)
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

        query.exec(Queries::createShows);
        query.exec(Queries::createShowsByKey);
        query.exec(Queries::createShowsByText);

        m_preparedQueries.reset(new PreparedQueries(m_database));
    }
    catch (QSqlError& error)
    {
        qDebug() << error;
    }
}

Database::~Database()
{
    m_update.waitForFinished();
}

void Database::fullUpdate(const QByteArray& data)
{
    update< FullUpdate >(data);
}

void Database::partialUpdate(const QByteArray& data)
{
    update< PartialUpdate >(data);
}

template< typename Processor >
void Database::update(const QByteArray& data)
{
    if (m_update.isRunning())
    {
        return;
    }

    m_update = QtConcurrent::run([this, data]()
    {
        try
        {
            Processor processor(m_database);

            if (!parse(data, processor))
            {
                emit failedToUpdate(tr("Could not parse data."));
                return;
            }

            processor.commit();

            Query(m_database).exec(Queries::rebuildShowsByText);

            m_settings.setDatabaseUpdatedOn();

            emit updated();
        }
        catch (QSqlError& error)
        {
            qDebug() << error;
        }
    });
}

QVector< quintptr > Database::query(
    const QString& channel, const QString& topic, const QString& title,
    const SortColumn sortColumn, const Qt::SortOrder sortOrder) const
{
    QVector< quintptr > id;

    QString sortOrderClause;

    switch (sortOrder)
    {
    default:
    case Qt::AscendingOrder:
        sortOrderClause = QStringLiteral("ASC");
        break;
    case Qt::DescendingOrder:
        sortOrderClause = QStringLiteral("DESC");
        break;
    }

    QString sortClause;

    switch (sortColumn)
    {
    default:
    case SortChannel:
        sortClause = QStringLiteral("shows.channel %1, shows.date DESC, shows.time DESC").arg(sortOrderClause);
        break;
    case SortTopic:
        sortClause = QStringLiteral("shows.topic %1, shows.date DESC, shows.time DESC").arg(sortOrderClause);
        break;
    case SortTitle:
        sortClause = QStringLiteral("shows.title %1, shows.date DESC, shows.time DESC").arg(sortOrderClause);
        break;
    case SortDate:
        sortClause = QStringLiteral("shows.date %1").arg(sortOrderClause);
        break;
    case SortTime:
        sortClause = QStringLiteral("shows.time %1").arg(sortOrderClause);
        break;
    case SortDuration:
        sortClause = QStringLiteral("shows.duration %1").arg(sortOrderClause);
        break;
    }

    QStringList filterClauses;

    if(!channel.isEmpty())
    {
        filterClauses.append("channel : \"' || ? || '\"");
    }

    if(!topic.isEmpty())
    {
        filterClauses.append("topic : \"' || ? || '\"");
    }

    if(!title.isEmpty())
    {
        filterClauses.append("title : \"' || ? || '\"");
    }

    const auto filterClause = filterClauses.isEmpty() ? QStringLiteral("1")
                              : QStringLiteral("showsByText MATCH '%1'").arg(filterClauses.join(QStringLiteral(" AND ")));

    try
    {
        Query query(m_database);

        if(channel.isEmpty() && topic.isEmpty() && title.isEmpty())
        {
            query.prepare(QStringLiteral("SELECT id FROM shows ORDER BY %1").arg(sortClause));
        }
        else
        {
            query.prepare(QStringLiteral("SELECT id"
                                         " FROM showsByText"
                                         " JOIN shows"
                                         " ON showsByText.rowid = shows.id"
                                         " WHERE %1 ORDER BY %4")
                          .arg(filterClause).arg(sortClause));

            if(!channel.isEmpty())
            {
                query << channel;
            }

            if(!topic.isEmpty())
            {
                query << topic;
            }

            if(!title.isEmpty())
            {
                query << title;
            }
        }

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

std::unique_ptr< Show > Database::show(const quintptr id) const
{
    std::unique_ptr< Show > show(new Show);

    try
    {
        Query& query = m_preparedQueries->selectShow << id;

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

            show->urlSmallOffset = query.nextValue< unsigned short >();
            show->urlSmallSuffix = query.nextValue< QString >();

            show->urlLargeOffset = query.nextValue< unsigned short >();
            show->urlLargeSuffix = query.nextValue< QString >();
        }
    }
    catch (QSqlError& error)
    {
        qDebug() << error;
    }

    return show;
}

QStringList Database::channels() const
{
    QStringList channels;

    try
    {
        Query& query = m_preparedQueries->selectChannels;

        query.exec();

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

    try
    {
        Query& query = channel.isEmpty()
                       ? m_preparedQueries->selectTopics
                       : m_preparedQueries->selectTopicsByChannel << channel;

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

} // QMediathekView
