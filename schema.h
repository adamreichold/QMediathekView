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

#ifndef SCHEMA_H
#define SCHEMA_H

#include <QDateTime>

namespace Mediathek
{

struct Show
{
    QString channel;
    QString topic;
    QString title;

    QDate date;
    QTime time;

    QTime duration;

    QString description;
    QString website;

    QString url;
    QString urlSmall;
    QString urlLarge;

};

enum class Url : int
{
    Default,
    Small,
    Large

};

} // Mediathek

#endif // SCHEMA_H
