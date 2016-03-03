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

#include <fstream>

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/container/flat_set.hpp>
#include <boost/date_time/gregorian/formatters.hpp>
#include <boost/date_time/gregorian/greg_serialize.hpp>
#include <boost/date_time/posix_time/time_serialize.hpp>
#include <boost/dynamic_bitset.hpp>
#include <boost/serialization/utility.hpp>
#include <boost/serialization/vector.hpp>

#include <QDebug>
#include <QStandardPaths>

#include <QtConcurrentRun>

#include "settings.h"
#include "parser.h"

namespace QMediathekView
{

namespace
{

const std::locale locale;

void to_lower(std::string& string)
{
    boost::algorithm::to_lower(string, locale);
}

std::string to_lower_copy(const std::string& string)
{
    return boost::algorithm::to_lower_copy(string, locale);
}

class TextColumn
{
public:
    void insert(const std::string& value)
    {
        const auto pos = std::lower_bound(m_values.begin(), m_values.end(), value);
        const auto rank = std::distance(m_values.begin(), pos);
        if (pos != m_values.end() && *pos == value)
        {
            m_rank.push_back(rank);
        }
        else
        {
            m_values.insert(pos, value);
            m_lower.insert(m_lower.begin() + rank, to_lower_copy(value));
            for (auto& oldRank : m_rank)
            {
                if (oldRank >= rank)
                {
                    ++oldRank;
                }
            }
            m_rank.push_back(rank);
        }
    }

    void collect(const std::string& key, std::vector< quintptr >& id) const
    {
        assert(!key.empty());
        assert(to_lower_copy(key) == key);

        if (m_lower.size() < 4096)
        {
            boost::dynamic_bitset<> ranks(m_lower.size());

            for (std::size_t index = 0, count = m_lower.size(); index < count; ++index)
            {
                if (m_lower[index].find(key) != std::string::npos)
                {
                    ranks.set(index);
                }
            }

            for (std::size_t index = 0, count = m_rank.size(); index < count; ++index)
            {
                if (ranks.test(m_rank[index]))
                {
                    id.push_back(index);
                }
            }
        }
        else
        {
            boost::container::flat_set< std::uint32_t > ranks;

            for (std::size_t index = 0, count = m_lower.size(); index < count; ++index)
            {
                if (m_lower[index].find(key) != std::string::npos)
                {
                    ranks.insert(index);
                }
            }

            for (std::size_t index = 0, count = m_rank.size(); index < count; ++index)
            {
                if (ranks.find(m_rank[index]) != ranks.end())
                {
                    id.push_back(index);
                }
            }
        }
    }

    void filter(const std::string& key, std::vector< quintptr >& id) const
    {
        assert(to_lower_copy(key) == key);

        if (key.empty())
        {
            return;
        }

        id.erase(std::remove_if(id.begin(), id.end(), [&](const quintptr index)
        {
            return m_lower[m_rank[index]].find(key) == std::string::npos;
        }), id.end());
    }

    std::size_t size() const
    {
        return m_rank.size();
    }

    const std::string& operator[] (const std::size_t& index) const
    {
        return m_values[m_rank[index]];
    }

    const std::string& at(const std::size_t& index) const
    {
        return m_values[m_rank.at(index)];
    }

    const std::vector< std::string >& values() const
    {
        return m_values;
    }

    std::vector< std::string > values(const std::string& key) const
    {
        assert(to_lower_copy(key) == key);

        std::vector< std::string > values;

        if (key.empty())
        {
            values = m_values;
        }
        else
        {
            for (std::size_t index = 0, count = m_lower.size(); index < count; ++index)
            {
                if (m_lower[index].find(key) != std::string::npos)
                {
                    values.push_back(m_values[index]);
                }
            }
        }

        return values;
    }

    const std::string& lower(const std::size_t& index) const
    {
        return m_lower[m_rank[index]];
    }

    const std::uint32_t& rank(const std::size_t& index) const
    {
        return m_rank[index];
    }

    template< typename Archive >
    void serialize(Archive& archive, const unsigned int /* version */)
    {
        archive & m_values & m_lower & m_rank;
    }

private:
    std::vector< std::string > m_values;
    std::vector< std::string > m_lower;
    std::vector< std::uint32_t > m_rank;

};

template< typename Type >
void sort(
    const std::vector< Type >& column, const Database::SortOrder sortOrder,
    std::vector< quintptr >& id)
{
    switch (sortOrder)
    {
    default:
    case Database::SortAscending:
        std::sort(id.begin(), id.end(), [&](const std::size_t lhs, const std::size_t rhs)
        {
            return column[lhs] < column[rhs];
        });
        break;
    case Database::SortDescending:
        std::sort(id.begin(), id.end(), [&](const std::size_t lhs, const std::size_t rhs)
        {
            return column[rhs] < column[lhs];
        });
        break;
    }
}

void sort(
    const TextColumn& column, const Database::SortOrder sortOrder,
    const std::vector< boost::gregorian::date >& date, const std::vector< boost::posix_time::time_duration >& time,
    std::vector< quintptr >& id)
{
    switch (sortOrder)
    {
    default:
    case Database::SortAscending:
        std::sort(id.begin(), id.end(), [&](const std::size_t lhs, const std::size_t rhs)
        {
            return operator< (
                       std::tie(column.rank(lhs), date[rhs], time[rhs]),
                       std::tie(column.rank(rhs), date[lhs], time[lhs]));
        });
        break;
    case Database::SortDescending:
        std::sort(id.begin(), id.end(), [&](const std::size_t lhs, const std::size_t rhs)
        {
            return operator< (
                       std::tie(column.rank(rhs), date[rhs], time[rhs]),
                       std::tie(column.rank(lhs), date[lhs], time[lhs]));
        });
        break;
    }
}

QByteArray databasePath()
{
    const auto path = QStandardPaths::writableLocation(QStandardPaths::DataLocation);
    QDir().mkpath(path);
    return QFile::encodeName(QDir(path).filePath("database"));
}

} // anonymous

struct Database::Data
{
    TextColumn channel;
    TextColumn topic;
    TextColumn title;

    std::vector< boost::gregorian::date > date;
    std::vector< boost::posix_time::time_duration > time;

    std::vector< boost::posix_time::time_duration > duration;

    std::vector< std::string > description;
    std::vector< std::string > website;

    std::vector< std::string > url;
    std::vector< std::pair< unsigned short, std::string > > urlSmall;
    std::vector< std::pair< unsigned short, std::string > > urlLarge;

    void insert(const Show& show)
    {
        channel.insert(show.channel);
        topic.insert(show.topic);
        title.insert(show.title);

        date.push_back(show.date);
        time.push_back(show.time);

        duration.push_back(show.duration);

        description.push_back(show.description);
        website.push_back(show.website);

        url.push_back(show.url);
        urlSmall.push_back(std::make_pair(show.urlSmallOffset, show.urlSmallSuffix));
        urlLarge.push_back(std::make_pair(show.urlLargeOffset, show.urlLargeSuffix));
    }

    void update(const Show& show)
    {
        const auto key = std::tie(show.title, show.url, show.channel, show.topic);

        for (std::size_t index = 0, count = channel.size(); index < count; ++index)
        {
            if (key == std::tie(title[index], url[index], channel[index], topic[index]))
            {
                date[index] = show.date;
                time[index] = show.time;

                duration[index] = show.duration;

                description[index] = show.description;
                website[index] = show.website;

                urlSmall[index] = std::make_pair(show.urlSmallOffset, show.urlSmallSuffix);
                urlLarge[index] = std::make_pair(show.urlLargeOffset, show.urlLargeSuffix);

                return;
            }
        }

        insert(show);
    }

    Data (const Data& data)
        : channel(data.channel)
        , topic(data.topic)
        , title(data.title)
        , date(data.date)
        , time(data.time)
        , duration(data.duration)
        , description(data.description)
        , website(data.website)
        , url(data.url)
        , urlSmall(data.urlSmall)
        , urlLarge(data.urlLarge)
    {
    }

    Data() = default;
    Data(Data&&) = default;

    Data& operator= (const Data&) = delete;
    Data& operator= (Data&&) = default;

    template< typename Archive >
    void serialize(Archive& archive, const unsigned int /* version */)
    {
        // *INDENT-OFF*

        archive
        & channel & topic & title
        & date & time
        & duration
        & description & website
        & url & urlSmall & urlLarge;

        // *INDENT-ON*
    }

};

class Database::Transaction
{
public:
    Transaction()
        : m_data(std::make_shared< Data >())
    {
    }

    Transaction(const DataPtr& data)
        : m_data(std::make_shared< Data >(*data))
    {
    }

    bool load(const char* path)
    {
        try
        {
            std::ifstream file(path, std::ios::binary);
            boost::archive::binary_iarchive archive(file);
            archive >> *m_data;

            return true;
        }
        catch (std::exception& exception)
        {
            qDebug() << exception.what();

            return false;
        }
    }

    bool save(const char* path)
    {
        try
        {
            std::ofstream file(path, std::ios::binary);
            boost::archive::binary_oarchive archive(file);
            archive << *m_data;

            return true;
        }
        catch (std::exception& exception)
        {
            qDebug() << exception.what();

            return false;
        }
    }

    DataPtr&& take()
    {
        return std::move(m_data);
    }

protected:
    DataPtr m_data;

};

struct Database::FullUpdate
    : public Database::Transaction
    , public Processor
{
    FullUpdate(const DataPtr& /* data */)
        : Transaction()
    {
    }

    void operator() (const Show& show) override
    {
        m_data->insert(show);
    }

};

struct Database::PartialUpdate
    : public Database::Transaction
    , public Processor
{
    PartialUpdate(const DataPtr& data)
        : Transaction(data)
    {
    }

    void operator() (const Show& show) override
    {
        m_data->update(show);
    }

};

Database::Database(Settings& settings, QObject* parent)
    : QObject(parent)
    , m_settings(settings)
    , m_data(std::make_shared< Data >())
{
    connect(&m_update, &Update::resultReadyAt, this, &Database::updateReady);

    Transaction transaction;

    if (transaction.load(databasePath().constData()))
    {
        m_data = transaction.take();
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

template< typename Transaction >
void Database::update(const QByteArray& data)
{
    if (m_update.isRunning())
    {
        return;
    }

    m_update.setFuture(QtConcurrent::run([this, data]()
    {
        Transaction transaction(m_data);

        if (!parse(data, transaction))
        {
            return DataPtr();
        }

        if (!transaction.save(databasePath().constData()))
        {
            return DataPtr();
        }

        return transaction.take();
    }));
}

void Database::updateReady(int index)
{
    const auto data = m_update.resultAt(index);

    if (!data)
    {
        emit failedToUpdate("Failed to parse or save data.");

        return;
    }

    m_data = data;

    m_settings.setDatabaseUpdatedOn();

    emit updated();
}

std::vector< quintptr > Database::query(
    std::string channel, std::string topic, std::string title,
    const SortColumn sortColumn, const SortOrder sortOrder) const
{
    std::vector< quintptr > id;

    to_lower(channel);
    to_lower(topic);
    to_lower(title);

    if (!channel.empty())
    {
        m_data->channel.collect(channel, id);
        m_data->topic.filter(topic, id);
        m_data->title.filter(title, id);
    }
    else if (!topic.empty())
    {
        m_data->topic.collect(topic, id);
        m_data->channel.filter(channel, id);
        m_data->title.filter(title, id);
    }
    else if (!title.empty())
    {
        m_data->title.collect(title, id);
        m_data->channel.filter(channel, id);
        m_data->topic.filter(topic, id);
    }
    else
    {
        id.resize(m_data->channel.size());
        std::iota(id.begin(), id.end(), std::size_t(0));
    }

    switch (sortColumn)
    {
    default:
    case SortChannel:
        sort(m_data->channel, sortOrder, m_data->date, m_data->time, id);
        break;
    case SortTopic:
        sort(m_data->topic, sortOrder, m_data->date, m_data->time, id);
        break;
    case SortTitle:
        sort(m_data->title, sortOrder, m_data->date, m_data->time, id);
        break;
    case SortDate:
        sort(m_data->date, sortOrder, id);
        break;
    case SortTime:
        sort(m_data->time, sortOrder, id);
        break;
    case SortDuration:
        sort(m_data->duration, sortOrder, id);
        break;
    }

    return id;
}

const std::string& Database::channel(const quintptr id) const
{
    return m_data->channel.at(id);
}

const std::string& Database::topic(const quintptr id) const
{
    return m_data->topic.at(id);
}

const std::string& Database::title(const quintptr id) const
{
    return m_data->title.at(id);
}

const boost::gregorian::date& Database::date(const quintptr id) const
{
    return m_data->date.at(id);
}

const boost::posix_time::time_duration& Database::time(const quintptr id) const
{
    return m_data->time.at(id);
}

const boost::posix_time::time_duration& Database::duration(const quintptr id) const
{
    return m_data->duration.at(id);
}

const std::string& Database::description(const quintptr id) const
{
    return m_data->description.at(id);
}

const std::string& Database::website(const quintptr id) const
{
    return m_data->website.at(id);
}

const std::string& Database::url(const quintptr id) const
{
    return m_data->url.at(id);
}

const std::string& Database::urlSmall(const quintptr id) const
{
    const auto& url = m_data->url.at(id);
    const auto& urlSmall = m_data->urlSmall.at(id);

    return url.substr(0, urlSmall.first).append(urlSmall.second);
}

const std::string& Database::urlLarge(const quintptr id) const
{
    const auto& url = m_data->url.at(id);
    const auto& urlLarge = m_data->urlLarge.at(id);

    return url.substr(0, urlLarge.first).append(urlLarge.second);
}

const std::vector< std::string >& Database::channels() const
{
    return m_data->channel.values();
}

std::vector< std::string > Database::topics(const std::string& channel) const
{
    return m_data->topic.values(to_lower_copy(channel));
}

} // QMediathekView
