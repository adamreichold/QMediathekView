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

#include <QObject>
#include <QSqlDatabase>
#include <QUrl>

#include "schema.h"

namespace Mediathek
{

class Settings;

class Database : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(Database)

public:
    Database(const Settings& settings, QObject* parent = 0);
    ~Database();

signals:
    void updated();
    void failedToUpdate(const QString& error);

public:
    void fullUpdate(const QByteArray& data);
    void partialUpdate(const QByteArray& data);

public:
    enum SortBy
    {
        SortByChannel,
        SortByTopic,
        SortByTitle,
        SortByDate,
        SortByTime,
        SortByDuration
    };

    QVector< quintptr > fetchId(
        const QString& channel, const QString& topic, const QString& title,
        const SortBy sortBy, const Qt::SortOrder sortOrder) const;

    std::unique_ptr< Show > fetchShow(const quintptr id) const;

public:
    QStringList channels() const;
    QStringList topics(const QString& channel) const;

private:
    const Settings& m_settings;

    mutable QSqlDatabase m_database;

};

} // Mediathek

#endif // DATABASE_H
