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

#include "database.h"

#include <QDebug>
#include <QStandardPaths>

#include "settings.h"

namespace
{

struct StringData
{
    const char* ptr;
    std::size_t len;

};

inline StringData fromBytes(const QByteArray& bytes)
{
    return { bytes.constData(), static_cast< std::size_t >(bytes.length()) };
}

inline QString toString(const StringData& data)
{
    return QString::fromUtf8(data.ptr, data.len);
}

struct ShowData
{
    StringData channel;
    StringData topic;
    StringData title;

    std::int64_t date;
    std::uint32_t time;

    std::uint32_t duration;

    StringData description;
    StringData website;

    StringData url;
    StringData url_small;
    StringData url_large;

};

}

extern "C"
{
    void append_integer(void* integers, std::int64_t data)
    {
        static_cast< QVector< quintptr >* >(integers)->append(data);
    }

    void append_string(void* strings, StringData data)
    {
        static_cast< QStringList* >(strings)->append(toString(data));
    }

    void fetch_show(void* show, const ShowData* data)
    {
        const auto show_ = static_cast< QMediathekView::Show* >(show);

        show_->channel = toString(data->channel);
        show_->topic = toString(data->topic);
        show_->title = toString(data->title);

        show_->date =  QDate::fromJulianDay(data->date);
        show_->time = QTime::fromMSecsSinceStartOfDay(data->time * 1000);

        show_->duration = QTime::fromMSecsSinceStartOfDay(data->duration * 1000);

        show_->description = toString(data->description);
        show_->website = toString(data->website);

        show_->url = toString(data->url);
        show_->urlSmall = toString(data->url_small);
        show_->urlLarge = toString(data->url_large);
    }

    Internals* internals_init(const char* path, bool* needs_update);
    void internals_drop(Internals* internals);

    struct Completion
    {
        void* context;
        void (*action)(void* context, const char* error);
    };

    void internals_full_update(
        Internals* internals,
        const char* url,
        Completion completion);
    void internals_partial_update(
        Internals* internals,
        const char* url,
        Completion completion);

    void internals_channels(
        Internals* internals,
        void* channels);
    void internals_topics(
        Internals* internals,
        StringData channel,
        void* topics);

    void internals_query(
        Internals* internals,
        StringData channel,
        StringData topic,
        StringData title,
        QMediathekView::Database::SortColumn sortColumn,
        QMediathekView::Database::SortOrder sortOrder,
        void* ids);

    void internals_fetch(
        Internals* internals,
        std::int64_t id,
        void* show);
}

namespace QMediathekView
{

Database::Database(Settings& settings, QObject* parent)
    : QObject(parent)
    , m_settings(settings)
{
    const auto path = QStandardPaths::writableLocation(QStandardPaths::DataLocation);
    bool needsUpdate = false;

    m_internals = internals_init(path.toLocal8Bit().constData(), &needsUpdate);

    if(needsUpdate)
    {
        m_settings.resetDatabaseUpdatedOn();
    }
}

Database::~Database()
{
    if(m_internals != nullptr)
    {
        internals_drop(m_internals);
    }
}

void Database::fullUpdate(const QString& url)
{
    if(m_internals != nullptr)
    {
        internals_full_update(
            m_internals,
            url.toUtf8().constData(),
            Completion { this, updateCompleted }
        );
    }
}

void Database::partialUpdate(const QString& url)
{
    if(m_internals != nullptr)
    {
        internals_partial_update(
            m_internals,
            url.toUtf8().constData(),
            Completion { this, updateCompleted }
        );
    }
}

void Database::updateCompleted(void* context, const char* error)
{
    Database* self = static_cast< Database* >(context);

    if (error != nullptr)
    {
        emit self->failedToUpdate(QString::fromUtf8(error));

        return;
    }

    self->m_settings.setDatabaseUpdatedOn();

    emit self->updated();
}

QVector< quintptr > Database::query(const QString& channel, const QString& topic, const QString& title, SortColumn sortColumn, SortOrder sortOrder) const
{
    QVector< quintptr > ids;

    if(m_internals != nullptr)
    {
        const auto channel_ = channel.toUtf8();
        const auto topic_ = topic.toUtf8();
        const auto title_ = title.toUtf8();

        internals_query(
            m_internals,
            fromBytes(channel_), fromBytes(topic_), fromBytes(title_),
            sortColumn, sortOrder,
            &ids);
    }

    return ids;
}

std::unique_ptr< Show > Database::show(const quintptr id) const
{
    std::unique_ptr< Show > show(new Show);

    if(m_internals != nullptr)
    {
        internals_fetch(m_internals, id, show.get());
    }

    return show;
}

QStringList Database::channels() const
{
    QStringList channels;
    channels.append(QString());

    if(m_internals != nullptr)
    {
        internals_channels(m_internals, &channels);
    }

    return channels;
}

QStringList Database::topics(const QString& channel) const
{
    QStringList topics;
    topics.append(QString());

    if(m_internals != nullptr)
    {
        const auto channel_ = channel.toUtf8();

        internals_topics(m_internals, fromBytes(channel_), &topics);
    }

    return topics;
}

} // QMediathekView
