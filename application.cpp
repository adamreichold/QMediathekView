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

#include "application.h"

#include <QDesktopServices>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QMessageBox>
#include <QProcess>
#include <QProxyStyle>
#include <QTimer>
#include <QUrl>

#include "settings.h"
#include "database.h"
#include "model.h"
#include "mainwindow.h"
#include "downloaddialog.h"

namespace QMediathekView
{

namespace
{

const auto projectName = QStringLiteral("QMediathekView");

class ProxyStyle : public QProxyStyle
{
public:
    int styleHint(StyleHint hint, const QStyleOption* option, const QWidget* widget, QStyleHintReturn* returnData) const override
    {
        if (hint == QStyle::SH_ItemView_ActivateItemOnSingleClick)
        {
            return 0;
        }

        return QProxyStyle::styleHint(hint, option, widget, returnData);
    }
};

bool startDetached(QString command)
{
#if QT_VERSION < QT_VERSION_CHECK(5,15,0)

    return QProcess::startDetached(command);

#else

    auto arguments = QProcess::splitCommand(command);
    auto program = arguments.takeFirst();

    return QProcess::startDetached(program, arguments);

#endif // QT_VERSION
}

} // anonymous

Application::Application(int& argc, char** argv, bool headless)
    : QApplication(argc, argv)
    , m_settings(new Settings(this))
    , m_database(new Database(*m_settings, this))
    , m_model(new Model(*m_database, this))
    , m_networkManager(new QNetworkAccessManager(this))
    , m_mainWindow(!headless ? new MainWindow(*m_settings, *m_model, *this) : nullptr)
{
    setWindowIcon(QIcon::fromTheme(projectName));
    setStyle(new ProxyStyle);

    connect(m_database, &Database::updated, m_model, &Model::update);

    connect(m_database, &Database::updated, this, &Application::completedDatabaseUpdate);
    connect(m_database, &Database::failedToUpdate, this, &Application::failedToUpdateDatabase);

    if (m_mainWindow != nullptr)
    {
        connect(this, &Application::startedDatabaseUpdate, m_mainWindow, &MainWindow::showStartedDatabaseUpdate);
        connect(this, &Application::completedDatabaseUpdate, m_mainWindow, &MainWindow::showCompletedDatabaseUpdate);
        connect(this, &Application::failedToUpdateDatabase, m_mainWindow, &MainWindow::showDatabaseUpdateFailure);
    }
    else
    {
        connect(this, &Application::startedDatabaseUpdate, this, &Application::logStartedDatabaseUpdate);
        connect(this, &Application::completedDatabaseUpdate, this, &Application::logCompletedDatabaseUpdate);
        connect(this, &Application::failedToUpdateDatabase, this, &Application::logDatabaseUpdateFailure);
    }
}

Application::~Application() = default;

int Application::exec()
{
    QTimer::singleShot(0, this, &Application::checkUpdateDatabase);

    if (m_mainWindow != nullptr)
    {
        m_mainWindow->setAttribute(Qt::WA_DeleteOnClose);
        m_mainWindow->show();
    }

    return QApplication::exec();
}

void Application::playPreferred(const QModelIndex& index) const
{
    startPlay(preferredUrl(index));
}

void Application::playDefault(const QModelIndex& index) const
{
    startPlay(m_model->url(index));
}

void Application::playSmall(const QModelIndex& index) const
{
    startPlay(m_model->urlSmall(index));
}

void Application::playLarge(const QModelIndex& index) const
{
    startPlay(m_model->urlLarge(index));
}

void Application::downloadPreferred(const QModelIndex& index) const
{
    startDownload(m_model->title(index), preferredUrl(index));
}

void Application::downloadDefault(const QModelIndex& index) const
{
    startDownload(m_model->title(index), m_model->url(index));
}

void Application::downloadSmall(const QModelIndex& index) const
{
    startDownload(m_model->title(index), m_model->urlSmall(index));
}

void Application::downloadLarge(const QModelIndex& index) const
{
    startDownload(m_model->title(index), m_model->urlLarge(index));
}

void Application::checkUpdateDatabase()
{
    const auto updateAfter = m_settings->databaseUpdateAfterHours();
    const auto updatedOn = m_settings->databaseUpdatedOn();
    const auto updatedBefore = updatedOn.secsTo(QDateTime::currentDateTime()) / 60 / 60;

    if (!updatedOn.isValid() || updateAfter < updatedBefore)
    {
        updateDatabase();
    }
    else if (m_mainWindow == nullptr)
    {
        quit();
    }
}

void Application::updateDatabase()
{
    emit startedDatabaseUpdate();

    const auto updatedOn = m_settings->databaseUpdatedOn();
    const auto fullUpdateOn = QDateTime(QDate::currentDate(), QTime(9, 0));

    if (!updatedOn.isValid() || updatedOn < fullUpdateOn)
    {
        m_database->fullUpdate(m_settings->fullListUrl());
    }
    else
    {
        m_database->partialUpdate(m_settings->partialListUrl());
    }
}

QString Application::preferredUrl(const QModelIndex& index) const
{
    auto firstUrl = &Model::url;
    auto secondUrl = &Model::urlSmall;
    auto thirdUrl = &Model::urlLarge;

    switch (m_settings->preferredUrl())
    {
    default:
    case Url::Default:
        break;
    case Url::Small:
        firstUrl = &Model::urlSmall;
        secondUrl = &Model::url;
        thirdUrl = &Model::urlLarge;
        break;
    case Url::Large:
        firstUrl = &Model::urlLarge;
        secondUrl = &Model::url;
        thirdUrl = &Model::urlSmall;
        break;
    }

    auto url = (m_model->*firstUrl)(index);

    if (url.isEmpty())
    {
        url = (m_model->*secondUrl)(index);
    }

    if (url.isEmpty())
    {
        url = (m_model->*thirdUrl)(index);
    }

    return url;
}

void Application::startPlay(const QString& url) const
{
    const auto command = m_settings->playCommand();

    if (!command.isEmpty())
    {
        if (!startDetached(command.arg(url)))
        {
            QMessageBox::critical(m_mainWindow, tr("Critical"), tr("Failed to execute play command."));
        }
    }
    else
    {
        QDesktopServices::openUrl(url);
    }
}

void Application::startDownload(const QString& title, const QString& url) const
{
    const auto command = m_settings->downloadCommand();

    if (!command.isEmpty())
    {
        if (!startDetached(command.arg(url)))
        {
            QMessageBox::critical(m_mainWindow, tr("Critical"), tr("Failed to execute download command."));
        }
    }
    else
    {
        const auto dialog = new DownloadDialog(
            *m_settings, m_networkManager,
            title, url);

        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->show();
    }
}

void Application::logStartedDatabaseUpdate()
{
    qInfo() << tr("Started database update...");
}

void Application::logCompletedDatabaseUpdate()
{
    qInfo() << tr("Successfully updated database.");
    quit();
}

void Application::logDatabaseUpdateFailure(const QString& error)
{
    qWarning() << tr("Failed to update database: %1").arg(error);
    quit();
}

} // QMediathekView

int main(int argc, char** argv)
{
    using namespace QMediathekView;

    QApplication::setOrganizationName(projectName);
    QApplication::setApplicationName(projectName);

    bool headless = false;

    for (int argi = 1; argi < argc; ++argi)
    {
        const char* const arg = argv[argi];

        if (strcmp(arg, "--headless") == 0)
        {
            headless = true;
        }
    }

    return Application(argc, argv, headless).exec();
}
