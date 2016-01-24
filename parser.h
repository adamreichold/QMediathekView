#ifndef PARSER_H
#define PARSER_H

#include <functional>

#include <QDate>
#include <QTime>

namespace Mediathek
{

struct Entry
{
    QString channel, topic, title;
    QDate date;
    QTime time;
    QTime duration;
    QString description, website;
    QString url, urlSmall, urlLarge;
};

using Inserter = std::function< void(const Entry&) >;

bool parse(const QByteArray& data, const Inserter& inserter);

} // Mediatehk

#endif // PARSER_H
