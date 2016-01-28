#ifndef SETTINGS_H
#define SETTINGS_H

#include <QDateTime>
#include <QDir>
#include <QObject>
#include <QStringList>

#include "schema.h"

class QSettings;

namespace Mediathek
{

class Settings : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(Settings)

public:
    explicit Settings(QObject* parent = 0);
    ~Settings();

public:
    QString userAgent() const;
    void setUserAgent(const QString& userAgent);

    QStringList fullListMirrors() const;
    void setFullListMirrors(const QStringList& mirrors);

    QStringList partialListMirrors() const;
    void setPartialListMirrors(const QStringList& mirrors);

    int mirrorsUpdateAfterDays() const;
    void setMirrorsUpdateAfterDays(int days);

    int databaseUpdateAfterHours() const;
    void setDatabaseUpdateAfterHours(int hours);

    QDateTime mirrorsUpdatedOn() const;
    void setMirrorsUpdatedOn() const;

    QDateTime databaseUpdatedOn() const;
    void setDatabaseUpdatedOn() const;

    QString playCommand() const;
    void setPlayCommand(const QString& command);

    QDir downloadFolder() const;
    void setDownloadFolder(const QDir& folder);

    Url preferredUrl() const;
    void setPreferredUrl(const Url type);

    QByteArray mainWindowGeometry() const;
    void setMainWindowGeometry(const QByteArray& geometry);

    QByteArray mainWindowState() const;
    void setMainWindowState(const QByteArray& state);

private:
    QSettings* m_settings;

};

} // Mediathek

#endif // SETTINGS_H
