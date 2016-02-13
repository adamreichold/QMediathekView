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

#ifndef MODEL_H
#define MODEL_H

#include <QAbstractTableModel>
#include <QCache>

class QStringListModel;

#include "schema.h"

namespace QMediathekView
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
    int columnCount(const QModelIndex& parent) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    int rowCount(const QModelIndex& parent) const override;
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

    int m_sortColumn = 0;
    Qt::SortOrder m_sortOrder = Qt::AscendingOrder;

    QVector< quintptr > m_id;
    int m_fetched = 0;

    void query();

    mutable QCache< quintptr, Show > m_cache;

    template< typename Member >
    using ResultOf = typename std::decay< typename std::result_of< Member(Show) >::type >::type;

    template< typename Member >
    ResultOf< Member > fetchShow(const quintptr id, Member member) const;

    QStringListModel* m_channels;
    QStringListModel* m_topics;

    void fetchChannels();
    void fetchTopics();

};

} // QMediathekView

#endif // MODEL_H
