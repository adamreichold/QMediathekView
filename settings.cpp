#include "settings.h"

#include <QSettings>

namespace
{

using namespace Mediathek;

namespace Keys
{

#define DEFINE_KEY(name) const auto name = QStringLiteral(#name)

DEFINE_KEY(userAgent);

DEFINE_KEY(mirrorList);

DEFINE_KEY(mirrorListUpdateAfterDays);
DEFINE_KEY(databaseUpdateAfterHours);

DEFINE_KEY(mirrorListUpdatedOn);
DEFINE_KEY(databaseUpdatedOn);

DEFINE_KEY(playCommand);
DEFINE_KEY(downloadFolder);

DEFINE_KEY(mainWindowGeometry);
DEFINE_KEY(mainWindowState);

#undef DEFINE_KEY

} // Keys

namespace Defaults
{

constexpr auto mirrorListUpdateAfterDays = 3;
constexpr auto databaseUpdateAfterHours = 3;

const auto playCommand = QStringLiteral("vlc %1");
const auto downloadFolder = QDir::homePath();

const auto userAgent = QStringLiteral("QMediathekView");

} // Defaults

} // anonymous

namespace Mediathek
{

Settings::Settings(QObject* parent) : QObject(parent),
    m_settings(new QSettings(this))
{
}

Settings::~Settings()
{
}

QString Settings::userAgent() const
{
    return m_settings->value(Keys::userAgent, Defaults::userAgent).toString();
}

void Settings::setUserAgent(const QString& userAgent)
{
    m_settings->setValue(Keys::userAgent, userAgent);
}

QStringList Settings::mirrorList() const
{
    return m_settings->value(Keys::mirrorList).toStringList();
}

void Settings::setMirrorList(const QStringList& mirrorList)
{
    m_settings->setValue(Keys::mirrorList, mirrorList);
}

int Settings::mirrorListUpdateAfterDays() const
{
    return m_settings->value(Keys::mirrorListUpdateAfterDays, Defaults::mirrorListUpdateAfterDays).toInt();
}

void Settings::setMirrorListUpdateAfterDays(int days)
{
    m_settings->setValue(Keys::mirrorListUpdateAfterDays, days);
}

int Settings::databaseUpdateAfterHours() const
{
    return m_settings->value(Keys::databaseUpdateAfterHours, Defaults::databaseUpdateAfterHours).toInt();
}

void Settings::setDatabaseUpdateAfterHours(int hours)
{
    m_settings->setValue(Keys::databaseUpdateAfterHours, hours);
}

QDateTime Settings::mirrorListUpdatedOn() const
{
    return m_settings->value(Keys::mirrorListUpdatedOn).toDateTime();
}

void Settings::setMirrorListUpdatedOn() const
{
    m_settings->setValue(Keys::mirrorListUpdatedOn, QDateTime::currentDateTime());
}

QDateTime Settings::databaseUpdatedOn() const
{
    return m_settings->value(Keys::databaseUpdatedOn).toDateTime();
}

void Settings::setDatabaseUpdatedOn() const
{
    m_settings->setValue(Keys::databaseUpdatedOn, QDateTime::currentDateTime());
}

QString Settings::playCommand() const
{
    return m_settings->value(Keys::playCommand, Defaults::playCommand).toString();
}

void Settings::setPlayCommand(const QString& command)
{
    m_settings->setValue(Keys::playCommand, command);
}

QDir Settings::downloadFolder() const
{
    return QDir(m_settings->value(Keys::downloadFolder, Defaults::downloadFolder).toString());
}

void Settings::setDownloadFolder(const QDir& folder)
{
    m_settings->setValue(Keys::downloadFolder, folder.absolutePath());
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

} // Mediathek
