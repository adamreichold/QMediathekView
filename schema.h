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

#include <boost/date_time/gregorian/gregorian_types.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>

namespace QMediathekView
{

struct Show
{
    std::string channel;
    std::string topic;
    std::string title;

    boost::gregorian::date date;
    boost::posix_time::time_duration time;

    boost::posix_time::time_duration duration;

    std::string description;
    std::string website;

    std::string url;

    unsigned short urlSmallOffset = 0;
    std::string urlSmallSuffix;

    std::string urlSmall() const
    {
        return url.substr(0, urlSmallOffset) + urlSmallSuffix;
    }

    unsigned short urlLargeOffset = 0;
    std::string urlLargeSuffix;

    std::string urlLarge() const
    {
        return url.substr(0, urlLargeOffset) + urlLargeSuffix;
    }

};

enum class Url : int
{
    Default,
    Small,
    Large

};

} // QMediathekView

#endif // SCHEMA_H
