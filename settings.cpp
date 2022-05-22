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

#include <QSettings>

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

DEFINE_KEY(databaseUpdateAfterHours);
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

const auto fullListUrl = QStringLiteral("https://liste.mediathekview.de/Filmliste-akt.xz");
const auto partialListUrl = QStringLiteral("https://liste.mediathekview.de/Filmliste-diff.xz");

constexpr auto databaseUpdateAfterHours = 3;

const auto playCommand = QStringLiteral("vlc %1");

const auto downloadFolder = QDir::homePath();

constexpr auto preferredUrl = Url::Default;

} // Defaults

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

int Settings::databaseUpdateAfterHours() const
{
    return m_settings->value(Keys::databaseUpdateAfterHours, Defaults::databaseUpdateAfterHours).toInt();
}

void Settings::setDatabaseUpdateAfterHours(int hours)
{
    m_settings->setValue(Keys::databaseUpdateAfterHours, hours);
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
    return m_settings->value(Keys::playCommand, Defaults::playCommand).toString();
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
    return QDir(m_settings->value(Keys::downloadFolder, Defaults::downloadFolder).toString());
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
