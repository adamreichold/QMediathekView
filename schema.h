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

} // Mediathek

#endif // SCHEMA_H
