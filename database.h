#ifndef DATABASE_H
#define DATABASE_H

#include <QDate>
#include <QObject>
#include <QSqlDatabase>
#include <QTime>
#include <QUrl>

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

public:
    QVector< quintptr > id() const;

    enum SortField
    {
        SortDefault,
        SortByChannel,
        SortByTopic,
        SortByTitle,
        SortByDate,
        SortByTime,
        SortByDuration
    };

    QVector< quintptr > id(
        const QString& channel, const QString& topic, const QString& title,
        const SortField sortField, const Qt::SortOrder sortOrder
    ) const;

    QStringList channels() const;
    QStringList topics() const;
    QStringList topics(const QString& channel) const;

    QString channel(const quintptr id) const;
    QString topic(const quintptr id) const;
    QString title(const quintptr id) const;

    QDate date(const quintptr id) const;
    QTime time(const quintptr id) const;
    QTime duration(const quintptr id) const;

    QString description(const quintptr id) const;
    QUrl website(const quintptr id) const;

    enum UrlKind
    {
        UrlDefault,
        UrlSmall,
        UrlLarge
    };

    QUrl url(const quintptr id, const UrlKind kind) const;

signals:
    void updated();
    void failedToUpdate(const QString& error);

public slots:
    void update(const QByteArray& data);

private:
    const Settings& m_settings;

    mutable QSqlDatabase m_database;

};

} // Mediathek

#endif // DATABASE_H
