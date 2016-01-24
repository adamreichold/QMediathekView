#ifndef SETTINGS_H
#define SETTINGS_H

#include <QDateTime>
#include <QDir>
#include <QObject>
#include <QStringList>

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

    QStringList mirrorList() const;
    void setMirrorList(const QStringList& mirrorList);

    int mirrorListUpdateAfterDays() const;
    void setMirrorListUpdateAfterDays(int days);

    int databaseUpdateAfterHours() const;
    void setDatabaseUpdateAfterHours(int hours);

    QDateTime mirrorListUpdatedOn() const;
    void setMirrorListUpdatedOn() const;

    QDateTime databaseUpdatedOn() const;
    void setDatabaseUpdatedOn() const;

    QString playCommand() const;
    void setPlayCommand(const QString& command);

    QDir downloadFolder() const;
    void setDownloadFolder(const QDir& folder);

    QByteArray mainWindowGeometry() const;
    void setMainWindowGeometry(const QByteArray& geometry);

    QByteArray mainWindowState() const;
    void setMainWindowState(const QByteArray& state);

private:
    QSettings* m_settings;

};

} // Mediathek

#endif // SETTINGS_H
