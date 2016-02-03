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

#include "model.h"

#include <QStringListModel>

#include "database.h"

namespace
{

constexpr auto cacheSize = 1024;
constexpr auto fetchSize = 256;

} // anonymous

namespace QMediathekView
{

Model::Model(Database& database, QObject* parent) : QAbstractTableModel(parent),
    m_database(database),
    m_cache(cacheSize),
    m_channels(new QStringListModel(this)),
    m_topics(new QStringListModel(this))
{
    update();
}

Model::~Model()
{
}

int Model::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
    {
        return 0;
    }

    return m_fetched;
}

int Model::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid())
    {
        return 0;
    }

    return 6;
}

QVariant Model::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole)
    {
        return {};
    }

    if (orientation != Qt::Horizontal)
    {
        return {};
    }

    switch (section)
    {
    case 0:
        return tr("Channel");
    case 1:
        return tr("Topic");
    case 2:
        return tr("Title");
    case 3:
        return tr("Date");
    case 4:
        return tr("Time");
    case 5:
        return tr("Duration");
    default:
        return {};
    }
}

QModelIndex Model::index(int row, int column, const QModelIndex& parent) const
{
    if (parent.isValid())
    {
        return {};
    }

    if (column < 0 || column >= 6)
    {
        return {};
    }

    if (row < 0 || row >= m_id.size())
    {
        return {};
    }

    return createIndex(row, column, m_id.at(row));
}

QVariant Model::data(const QModelIndex& index, int role) const
{
    if (role != Qt::DisplayRole)
    {
        return {};
    }

    if (!index.isValid())
    {
        return {};
    }

    const auto id = index.internalId();
    const auto column = index.column();

    switch (column)
    {
    case 0:
        return fetchShow< QString, &Show::channel >(id);
    case 1:
        return fetchShow< QString, &Show::topic >(id);
    case 2:
        return fetchShow< QString, &Show::title >(id);
    case 3:
        return fetchShow< QDate, &Show::date >(id).toString(tr("dd.MM.yy"));
    case 4:
        return fetchShow< QTime, &Show::time >(id).toString(tr("hh:mm"));
    case 5:
        return fetchShow< QTime, &Show::duration >(id).toString(tr("hh:mm:ss"));
    default:
        return {};
    }
}

void Model::filter(const QString& channel, const QString& topic, const QString& title)
{
    if (m_channel == channel && m_topic == topic && m_title == title)
    {
        return;
    }

    beginResetModel();

    if (m_channel != channel)
    {
        m_channel = channel;

        fetchTopics();
    }

    m_topic = topic;
    m_title = title;

    query();

    endResetModel();
}

void Model::sort(int column, Qt::SortOrder order)
{
    if (column < 0 || column >= 6)
    {
        return;
    }

    if (m_sortColumn == column && m_sortOrder == order)
    {
        return;
    }

    beginResetModel();

    m_sortColumn = column;
    m_sortOrder = order;

    query();

    endResetModel();
}

bool Model::canFetchMore(const QModelIndex& parent) const
{
    if (parent.isValid())
    {
        return false;
    }

    return m_id.size() > m_fetched;
}

void Model::fetchMore(const QModelIndex& parent)
{
    if (parent.isValid())
    {
        return;
    }

    const auto fetch = qMin(fetchSize, m_id.size() - m_fetched);

    beginInsertRows({}, m_fetched, m_fetched + fetch - 1);

    m_fetched += fetch;

    endInsertRows();
}

QAbstractItemModel* Model::channels() const
{
    return m_channels;
}

QAbstractItemModel* Model::topics() const
{
    return m_topics;
}

QString Model::title(const QModelIndex& index) const
{
    if (!index.isValid())
    {
        return {};
    }

    return fetchShow< QString, &Show::title >(index.internalId());
}

QString Model::description(const QModelIndex& index) const
{
    if (!index.isValid())
    {
        return {};
    }

    return fetchShow< QString, &Show::description >(index.internalId());
}

QString Model::website(const QModelIndex& index) const
{
    if (!index.isValid())
    {
        return {};
    }

    return fetchShow< QString, &Show::website >(index.internalId());
}

QString Model::url(const QModelIndex& index) const
{
    if (!index.isValid())
    {
        return {};
    }

    return fetchShow< QString, &Show::url >(index.internalId());
}

QString Model::urlSmall(const QModelIndex& index) const
{
    if (!index.isValid())
    {
        return {};
    }

    return fetchShow< QString, &Show::urlSmall >(index.internalId());
}

QString Model::urlLarge(const QModelIndex& index) const
{
    if (!index.isValid())
    {
        return {};
    }

    return fetchShow< QString, &Show::urlLarge >(index.internalId());
}

void Model::update()
{
    beginResetModel();

    query();
    fetchChannels();
    fetchTopics();

    endResetModel();
}

void Model::query()
{
    Database::SortColumn sortColumn;

    switch (m_sortColumn)
    {
    default:
    case 0:
        sortColumn = Database::SortChannel;
        break;
    case 1:
        sortColumn = Database::SortTopic;
        break;
    case 2:
        sortColumn = Database::SortTitle;
        break;
    case 3:
        sortColumn = Database::SortDate;
        break;
    case 4:
        sortColumn = Database::SortTime;
        break;
    case 5:
        sortColumn = Database::SortDuration;
        break;
    }

    m_id = m_database.query(
               m_channel, m_topic, m_title,
               sortColumn, m_sortOrder);
    m_fetched = 0;
}

template< typename Type, Type Show::* Field >
Type Model::fetchShow(const quintptr id) const
{
    if (const auto show = m_cache.object(id))
    {
        return show->*Field;
    }

    auto show = m_database.show(id);

    const auto value = show.get()->*Field;

    m_cache.insert(id, show.release());

    return value;
}

void Model::fetchChannels()
{
    auto channels = m_database.channels();
    channels.prepend(QString());

    if (m_channels->stringList() != channels)
    {
        m_channels->setStringList(channels);
    }
}

void Model::fetchTopics()
{
    auto topics = m_database.topics(m_channel);
    topics.prepend(QString());

    if (m_topics->stringList() != topics)
    {
        m_topics->setStringList(topics);
    }
}

} // QMediathekView
