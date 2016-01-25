#include "model.h"

#include "database.h"

namespace Mediathek
{

Model::Model(Database& database, QObject* parent) : QAbstractTableModel(parent),
    m_database(database)
{
    reset();
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
    const auto section = index.column();

    switch (section)
    {
    case 0:
        return m_database.channel(id);
    case 1:
        return m_database.topic(id);
    case 2:
        return m_database.title(id);
    case 3:
        return m_database.date(id).toString(QStringLiteral("dd.MM.yy"));
    case 4:
        return m_database.time(id).toString(QStringLiteral("hh:mm"));
    case 5:
        return m_database.duration(id).toString(QStringLiteral("hh:mm:ss"));
    default:
        return {};
    }
}

void Model::filter(const QString& channel, const QString& topic, const QString& title)
{
    beginResetModel();

    m_channel = channel;
    m_topic = topic;
    m_title = title;

    update();

    endResetModel();
}

void Model::sort(int column, Qt::SortOrder order)
{
    if (column < 0 || column >= 6)
    {
        return;
    }

    beginResetModel();

    m_sortColumn = column;
    m_sortOrder = order;

    update();

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

    const auto fetch = qMin(256, m_id.size() - m_fetched);

    beginInsertRows({}, m_fetched, m_fetched + fetch - 1);

    m_fetched += fetch;

    endInsertRows();
}

QStringList Model::channels() const
{
    return m_database.channels();
}

QStringList Model::topics() const
{
    return m_database.topics();
}

QStringList Model::topics(const QString& channel) const
{
    return m_database.topics(channel);
}

QString Model::description(const QModelIndex& index) const
{
    if (!index.isValid())
    {
        return {};
    }

    return m_database.description(index.internalId());
}

QUrl Model::website(const QModelIndex& index) const
{
    if (!index.isValid())
    {
        return {};
    }

    return m_database.website(index.internalId());
}

void Model::reset()
{
    beginResetModel();

    m_channel.clear();
    m_topic.clear();
    m_title.clear();

    m_sortColumn = -1;
    m_sortOrder = Qt::AscendingOrder;

    m_id = m_database.id();
    m_fetched = 0;

    endResetModel();
}

void Model::update()
{
    Database::SortField sortField = Database::SortDefault;

    switch (m_sortColumn)
    {
    case 0:
        sortField = Database::SortByChannel;
        break;
    case 1:
        sortField = Database::SortByTopic;
        break;
    case 2:
        sortField = Database::SortByTitle;
        break;
    case 3:
        sortField = Database::SortByDate;
        break;
    case 4:
        sortField = Database::SortByTime;
        break;
    case 5:
        sortField = Database::SortByDuration;
        break;
    }

    m_id = m_database.id(
               m_channel, m_topic, m_title,
               sortField, m_sortOrder
           );
    m_fetched = 0;
}

} // Mediathek
