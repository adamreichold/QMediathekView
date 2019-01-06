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

#include "settings.h"

#include <QProcess>
#include <QSettings>
#include <QStandardPaths>

namespace QMediathekView
{

namespace
{

namespace Keys
{

#define DEFINE_KEY(name) const auto name = QStringLiteral(#name)

DEFINE_KEY(userAgent);

DEFINE_KEY(fullListUrl);
DEFINE_KEY(partialListUrl);

DEFINE_KEY(fullListMirrors);
DEFINE_KEY(partialListMirrors);

DEFINE_KEY(mirrorsUpdateAfterDays);
DEFINE_KEY(databaseUpdateAfterHours);

DEFINE_KEY(mirrorsUpdatedOn);
DEFINE_KEY(databaseUpdatedOn);

DEFINE_KEY(playCommand);
DEFINE_KEY(downloadCommand);

DEFINE_KEY(downloadFolder);

DEFINE_KEY(preferredUrl);

DEFINE_KEY(mainWindowGeometry);
DEFINE_KEY(mainWindowState);

#undef DEFINE_KEY

} // Keys

namespace Defaults
{

const auto userAgent = QStringLiteral("QMediathekView");

const auto fullListUrl = QStringLiteral("http://zdfmediathk.sourceforge.net/akt.xml");
const auto partialListUrl = QStringLiteral("http://zdfmediathk.sourceforge.net/diff.xml");

constexpr auto mirrorListUpdateAfterDays = 3;
constexpr auto databaseUpdateAfterHours = 3;

constexpr auto preferredUrl = Url::Default;

} // Defaults

QString defaultPlayCommand()
{
    QString command("vlc");

    QProcess shell;
    shell.start("sh", QStringList() << "-c" << "grep '^Exec=' /usr/share/applications/`xdg-mime query default video/mp4` | cut -d'=' -f2 | cut -d' ' -f1", QIODevice::ReadOnly);
    shell.waitForFinished();

    if (shell.exitStatus() == QProcess::NormalExit)
    {
        const auto greppedCommand = QString::fromLocal8Bit(shell.readAll()).trimmed();

        if (!greppedCommand.isEmpty())
        {
            command = greppedCommand;
        }
    }

    return command.append(" %1");
}

QString defaultDownloadFolder()
{
    return QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
}

} // anonymous

Settings::Settings(QObject* parent) : QObject(parent),
    m_settings(new QSettings(this))
{
}

Settings::~Settings() = default;

QString Settings::userAgent() const
{
    return m_settings->value(Keys::userAgent, Defaults::userAgent).toString();
}

QString Settings::fullListUrl() const
{
    return m_settings->value(Keys::fullListUrl, Defaults::fullListUrl).toString();
}

QString Settings::partialListUrl() const
{
    return m_settings->value(Keys::partialListUrl, Defaults::partialListUrl).toString();
}

QStringList Settings::fullListMirrors() const
{
    return m_settings->value(Keys::fullListMirrors).toStringList();
}

void Settings::setFullListMirrors(const QStringList& mirrors)
{
    m_settings->setValue(Keys::fullListMirrors, mirrors);
}

QStringList Settings::partialListMirrors() const
{
    return m_settings->value(Keys::partialListMirrors).toStringList();
}

void Settings::setPartialListMirrors(const QStringList& mirrors)
{
    m_settings->setValue(Keys::partialListMirrors, mirrors);
}

int Settings::mirrorsUpdateAfterDays() const
{
    return m_settings->value(Keys::mirrorsUpdateAfterDays, Defaults::mirrorListUpdateAfterDays).toInt();
}

void Settings::setMirrorsUpdateAfterDays(int days)
{
    m_settings->setValue(Keys::mirrorsUpdateAfterDays, days);
}

int Settings::databaseUpdateAfterHours() const
{
    return m_settings->value(Keys::databaseUpdateAfterHours, Defaults::databaseUpdateAfterHours).toInt();
}

void Settings::setDatabaseUpdateAfterHours(int hours)
{
    m_settings->setValue(Keys::databaseUpdateAfterHours, hours);
}

QDateTime Settings::mirrorsUpdatedOn() const
{
    return m_settings->value(Keys::mirrorsUpdatedOn).toDateTime();
}

void Settings::setMirrorsUpdatedOn()
{
    m_settings->setValue(Keys::mirrorsUpdatedOn, QDateTime::currentDateTime());
}

void Settings::resetMirrorsUpdatedOn()
{
    m_settings->remove(Keys::mirrorsUpdatedOn);
}

QDateTime Settings::databaseUpdatedOn() const
{
    return m_settings->value(Keys::databaseUpdatedOn).toDateTime();
}

void Settings::setDatabaseUpdatedOn()
{
    m_settings->setValue(Keys::databaseUpdatedOn, QDateTime::currentDateTime());
}

void Settings::resetDatabaseUpdatedOn()
{
    m_settings->remove(Keys::databaseUpdatedOn);
}

QString Settings::playCommand() const
{
    return m_settings->value(Keys::playCommand, defaultPlayCommand()).toString();
}

void Settings::setPlayCommand(const QString& command)
{
    m_settings->setValue(Keys::playCommand, command);
}

QString Settings::downloadCommand() const
{
    return m_settings->value(Keys::downloadCommand).toString();
}

void Settings::setDownloadCommand(const QString& command)
{
    m_settings->setValue(Keys::downloadCommand, command);
}

QDir Settings::downloadFolder() const
{
    return QDir(m_settings->value(Keys::downloadFolder, defaultDownloadFolder()).toString());
}

void Settings::setDownloadFolder(const QDir& folder)
{
    m_settings->setValue(Keys::downloadFolder, folder.absolutePath());
}

Url Settings::preferredUrl() const
{
    return Url(m_settings->value(Keys::preferredUrl, int(Defaults::preferredUrl)).toInt());
}

void Settings::setPreferredUrl(const Url type)
{
    m_settings->setValue(Keys::preferredUrl, int(type));
}

QByteArray Settings::mainWindowGeometry() const
{
    return m_settings->value(Keys::mainWindowGeometry).toByteArray();
}

void Settings::setMainWindowGeometry(const QByteArray& geometry)
{
    m_settings->setValue(Keys::mainWindowGeometry, geometry);
}

QByteArray Settings::mainWindowState() const
{
    return m_settings->value(Keys::mainWindowState).toByteArray();
}

void Settings::setMainWindowState(const QByteArray& state)
{
    m_settings->setValue(Keys::mainWindowState, state);
}

} // QMediathekView
