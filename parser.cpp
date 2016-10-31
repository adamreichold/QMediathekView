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

#include "parser.h"

#include <boost/tokenizer.hpp>

#include <QIODevice>

namespace QMediathekView
{

namespace
{

void assignIfNotEmpty(QString& value, const QByteArray& newValue)
{
    if(!newValue.isEmpty())
    {
        value = newValue;
    }
}

} // anonymous

bool parse(QIODevice& source, Processor& processor)
{
    try
    {
        boost::tokenizer< boost::escaped_list_separator< char >, QByteArray::const_iterator, QByteArray > tokenizer{QByteArray{}};
        std::vector< QByteArray > token;

        enum Column
        {
            Channel = 0,
            ChannelUrlPrefix,
            ChannelUrlWebPrefix,
            Topic,
            TopicUrlPrefix,
            TopicUrlWebPrefix,
            Title,
            Date,
            Time,
            Duration,
            Description,
            UrlWeb,
            UrlPrefix,
            UrlLarge,
            UrlMedium,
            UrlSmall
        };

        Show show;

        QString channelUrlPrefix;
        QString channelUrlWebPrefix;
        QString topicUrlPrefix;
        QString topicUrlWebPrefix;

        forever
        {
            const auto buffer = source.readLine();

            if (buffer.isEmpty())
            {
                break;
            }

            tokenizer.assign(buffer.begin(), buffer.end());
            token.assign(tokenizer.begin(), tokenizer.end());

            assignIfNotEmpty(show.channel, token.at(Channel));
            assignIfNotEmpty(channelUrlPrefix, token.at(ChannelUrlPrefix));
            assignIfNotEmpty(channelUrlWebPrefix, token.at(ChannelUrlWebPrefix));

            assignIfNotEmpty(show.topic, token.at(Topic));
            assignIfNotEmpty(topicUrlPrefix, token.at(TopicUrlPrefix));
            assignIfNotEmpty(topicUrlWebPrefix, token.at(TopicUrlWebPrefix));

            assignIfNotEmpty(show.title, token.at(Title));

            show.date = QDate::fromString(QString::fromUtf8(token.at(Date)), "dd.MM.yyyy");
            show.time = QTime::fromString(QString::fromUtf8(token.at(Time)), "hh:mm");

            show.duration = QTime::fromString(QString::fromUtf8(token.at(Duration)), "hh:mm");

            show.description = QString::fromUtf8(token.at(Description));
            show.website = channelUrlWebPrefix + topicUrlWebPrefix + QString::fromUtf8(token.at(UrlWeb));

            const auto urlPrefix = QString::fromUtf8(token.at(UrlPrefix));
            show.urlLarge = channelUrlPrefix + topicUrlPrefix + urlPrefix + QString::fromUtf8(token.at(UrlLarge));
            show.url = channelUrlPrefix + topicUrlPrefix + urlPrefix + QString::fromUtf8(token.at(UrlMedium));
            show.urlSmall = channelUrlPrefix + topicUrlPrefix + urlPrefix + QString::fromUtf8(token.at(UrlSmall));

            processor(show);
        }

        return true;
    }
    catch(const std::exception&)
    {
        return false;
    }
}

} // QMediathekView
