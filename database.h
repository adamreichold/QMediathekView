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

#ifndef DATABASE_H
#define DATABASE_H

#include <memory>

#include <QFutureWatcher>
#include <QObject>

#include "schema.h"

namespace QMediathekView
{

class Settings;

class Database : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(Database)

public:
    Database(Settings& settings, QObject* parent = 0);
    ~Database();

signals:
    void updated();
    void failedToUpdate(const QString& error);

public:
    void fullUpdate(const QByteArray& data);
    void partialUpdate(const QByteArray& data);

private:
    template< typename Transaction >
    void update(const QByteArray& data);

    void updateReady(int index);

public:
    enum SortColumn
    {
        SortChannel,
        SortTopic,
        SortTitle,
        SortDate,
        SortTime,
        SortDuration
    };

    enum SortOrder
    {
        SortAscending,
        SortDescending
    };

    std::vector< quintptr > query(
        std::string channel, std::string topic, std::string title,
        const SortColumn sortColumn, const SortOrder sortOrder) const;

public:
    std::shared_ptr< const Show > show(const quintptr id) const;

    std::vector< std::string > channels() const;
    std::vector< std::string > topics(std::string channel) const;

private:
    Settings& m_settings;

    struct Data;
    using DataPtr = std::shared_ptr< Data >;
    mutable DataPtr m_data;

    class Transaction;

    using Update = QFutureWatcher< DataPtr >;
    Update m_update;

    class FullUpdate;
    class PartialUpdate;

};

} // QMediathekView

#endif // DATABASE_H
