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

#include <QFuture>
#include <QObject>
#include <QSqlDatabase>

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
    template< typename Processor >
    void update(const QByteArray& data);

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

    QVector< quintptr > query(
        const QString& channel, const QString& topic, const QString& title,
        const SortColumn sortColumn, const Qt::SortOrder sortOrder) const;

public:
    std::unique_ptr< Show > show(const quintptr id) const;

    QStringList channels() const;
    QStringList topics(const QString& channel) const;

private:
    Settings& m_settings;

    mutable QSqlDatabase m_database;

    struct PreparedQueries;
    QScopedPointer< PreparedQueries > m_preparedQueries;

    QFuture< void > m_update;

};

} // QMediathekView

#endif // DATABASE_H
