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
    class Value
    {
    private:
        friend class Query;

        explicit Value(const QVariant& value) :
            m_value(value)
        {
        }

    public:
        template< typename T >
        operator T() const
        {
            return m_value.value< T >();
        }

    private:
        const QVariant m_value;

    };

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

    Value nextValue()
    {
        return Value(m_query.value(m_valueIndex++));
    }

private:
    Q_DISABLE_COPY(Query)

    QSqlQuery m_query;
    int m_bindValueIndex;
    int m_valueIndex;

};

template< typename Field >
Field queryField(QSqlDatabase& database, const QString& field, const quintptr id)
{
    try
    {
        Query query(database);

        query.exec(QStringLiteral("SELECT %1 FROM shows WHERE rowid = %2").arg(field).arg(id));

        if (query.nextRecord())
        {
            return query.nextValue();
        }
    }
    catch (QSqlError& error)
    {
        qDebug() << error;
    }

    return {};
}

template< typename Field >
QList< Field > queryFieldRange(QSqlDatabase& database, const QString& field)
{
    QList< Field > range;

    try
    {
        Query query(database);

        query.exec(QStringLiteral("SELECT DISTINCT(%1) FROM shows").arg(field));

        while (query.nextRecord())
        {
            range.push_back(query.nextValue());
        }
    }
    catch (QSqlError& error)
    {
        qDebug() << error;
    }

    return range;
}

namespace Fields
{

#define DEFINE_FIELD(name) const auto name = QStringLiteral(#name)

DEFINE_FIELD(channel);
DEFINE_FIELD(topic);
DEFINE_FIELD(title);

DEFINE_FIELD(date);
DEFINE_FIELD(time);
DEFINE_FIELD(duration);

DEFINE_FIELD(description);
DEFINE_FIELD(website);

DEFINE_FIELD(url);
DEFINE_FIELD(urlSmall);
DEFINE_FIELD(urlLarge);

#undef DEFINE_FIELD

}

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
                       " channel TEXT NOCASE,"
                       " topic TEXT NOCASE,"
                       " title TEXT NOCASE,"
                       " date INTEGER,"
                       " time INTEGER,"
                       " duration INTEGER,"
                       " description TEXT,"
                       " website TEXT,"
                       " url TEXT,"
                       " urlSmall TEXT,"
                       " urlLarge TEXT)"
                   ));

        query.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS showsByChannel ON shows (channel)"));
        query.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS showsByTopic ON shows (topic)"));
        query.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS showsByTitle ON shows (title)"));
        query.exec(QStringLiteral("CREATE INDEX IF NOT EXISTS showsByDateAndTime ON shows (date DESC, time DESC)"));
    }
    catch (QSqlError& error)
    {
        qDebug() << error;
    }
}

Database::~Database()
{
}

QVector< quintptr > Database::id(
    const QString& channel, const QString& topic, const QString& title,
    const SortField sortField, const Qt::SortOrder sortOrder
) const
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

    switch (sortField)
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

        query.prepare(QStringLiteral(
                          "SELECT rowid FROM shows"
                          " WHERE %1 AND %2 AND %3"
                          " ORDER BY %4"
                      )
                      .arg(channelFilterClause)
                      .arg(topicFilterClause)
                      .arg(titleFilterCaluse)
                      .arg(sortClause)
                     );

        query << channel << topic << title;

        query.exec();

        while (query.nextRecord())
        {
            id.push_back(query.nextValue());
        }
    }
    catch (QSqlError& error)
    {
        qDebug() << error;
    }

    return id;
}

QStringList Database::channels() const
{
    return queryFieldRange< QString >(m_database, Fields::channel);
}

QStringList Database::topics() const
{
    return queryFieldRange< QString >(m_database, Fields::topic);
}

QStringList Database::topics(const QString& channel) const
{
    QStringList topics;

    try
    {
        Query query(m_database);

        query.prepare(QStringLiteral(
                          "SELECT DISTINCT(topic) FROM shows"
                          " WHERE channel = ?"
                      ));

        query << channel;

        query.exec();

        while (query.nextRecord())
        {
            topics.push_back(query.nextValue());
        }
    }
    catch (QSqlError& error)
    {
        qDebug() << error;
    }

    return topics;
}

QString Database::channel(const quintptr id) const
{
    return queryField< QString >(m_database, Fields::channel, id);
}

QString Database::topic(const quintptr id) const
{
    return queryField< QString >(m_database, Fields::topic, id);
}

QString Database::title(const quintptr id) const
{
    return queryField< QString >(m_database, Fields::title, id);
}

QDate Database::date(const quintptr id) const
{
    return QDate::fromJulianDay(queryField< qint64 >(m_database, Fields::date, id));
}

QTime Database::time(const quintptr id) const
{
    return QTime::fromMSecsSinceStartOfDay(queryField< int >(m_database, Fields::time, id));
}

QTime Database::duration(const quintptr id) const
{
    return QTime::fromMSecsSinceStartOfDay(queryField< int >(m_database, Fields::duration, id));
}

QString Database::description(const quintptr id) const
{
    return queryField< QString >(m_database, Fields::description, id);
}

QUrl Database::website(const quintptr id) const
{
    return queryField< QString >(m_database, Fields::website, id);
}

QUrl Database::url(const quintptr id, const Database::UrlKind kind) const
{
    switch (kind)
    {
    default:
    case UrlDefault:
        return queryField< QString >(m_database, Fields::url, id);
    case UrlSmall:
        return queryField< QString >(m_database, Fields::urlSmall, id);
    case UrlLarge:
        return queryField< QString >(m_database, Fields::urlLarge, id);
    }
}

void Database::update(const QByteArray& data)
{
    QtConcurrent::run([this, data]()
    {
        try
        {
            Transaction transaction(m_database);
            Query query(m_database);

            query.exec(QStringLiteral("DELETE FROM shows"));

            query.prepare(QStringLiteral(
                              "INSERT INTO shows ("
                              " channel, topic, title,"
                              " date, time, duration,"
                              " description, website,"
                              " url, urlSmall, urlLarge)"
                              " VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"
                          ));

            const auto inserter = [&query](const Show& show)
            {
                query << show.channel << show.topic << show.title
                      << show.date.toJulianDay() << show.time.msecsSinceStartOfDay() << show.duration.msecsSinceStartOfDay()
                      << show.description << show.website
                      << show.url << show.urlSmall << show.urlLarge;

                query.exec();
            };

            if (!parse(data, inserter))
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

} // Mediathek
