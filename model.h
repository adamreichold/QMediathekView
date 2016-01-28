#ifndef MODEL_H
#define MODEL_H

#include <QAbstractTableModel>
#include <QCache>

class QStringListModel;

#include "schema.h"

namespace Mediathek
{

class Database;

class Model : public QAbstractTableModel
{
    Q_OBJECT
    Q_DISABLE_COPY(Model)

public:
    Model(Database& database, QObject* parent = 0);
    ~Model();

public:
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    int rowCount(const QModelIndex& parent) const override;
    int columnCount(const QModelIndex& parent) const override;

    QModelIndex index(int row, int column, const QModelIndex& parent) const override;
    QVariant data(const QModelIndex& index, int role) const override;

    void filter(const QString& channel, const QString& topic, const QString& title);
    void sort(int column, Qt::SortOrder order) override;

protected:
    bool canFetchMore(const QModelIndex& parent) const override;
    void fetchMore(const QModelIndex& parent) override;

public:
    QAbstractItemModel* channels() const;
    QAbstractItemModel* topics() const;

public:
    QString title(const QModelIndex& index) const;

    QString description(const QModelIndex& index) const;
    QString website(const QModelIndex& index) const;

    QString url(const QModelIndex& index) const;
    QString urlSmall(const QModelIndex& index) const;
    QString urlLarge(const QModelIndex& index) const;

public:
    void update();

private:
    const Database& m_database;

    QString m_channel;
    QString m_topic;
    QString m_title;

    int m_sortColumn = -1;
    Qt::SortOrder m_sortOrder = Qt::AscendingOrder;

    QVector< quintptr > m_id;
    int m_fetched = 0;
    mutable QCache< quintptr, Show > m_cache;

    void fetchId();
    Show fetchShow(const quintptr id) const;

    QStringListModel* m_channels;
    QStringListModel* m_topics;

    void fetchChannels();
    void fetchTopics();

};

} // Mediathek

#endif // MODEL_H
