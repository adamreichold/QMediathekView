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
#include <QTimer>
#include <QUrl>

#include <lzma.h>

#include "settings.h"
#include "database.h"
#include "model.h"
#include "torrentsession.h"
#include "mainwindow.h"
#include "downloaddialog.h"

namespace QMediathekView
{

namespace
{

const auto projectName = QStringLiteral("QMediathekView");

class Decompressor
{
public:
    Decompressor()
        : m_stream(LZMA_STREAM_INIT)
        , m_buffer(64 * 1024, Qt::Uninitialized)
        , m_data()
    {
        Q_UNUSED(lzma_stream_decoder(&m_stream, UINT64_MAX, LZMA_TELL_NO_CHECK));
    }

    void appendData(const QByteArray& data)
    {
        m_stream.next_in = reinterpret_cast< const std::uint8_t* >(data.constData());
        m_stream.avail_in = data.size();

        for (lzma_ret result = LZMA_OK; result == LZMA_OK;)
        {
            m_stream.next_out = reinterpret_cast< std::uint8_t* >(m_buffer.data());
            m_stream.avail_out = m_buffer.size();

            result = lzma_code(&m_stream, LZMA_RUN);

            m_data.append(m_buffer.constData(), m_buffer.size() - m_stream.avail_out);
        }
    }

    const QByteArray& data() const
    {
        return m_data;
    }

private:
    lzma_stream m_stream;
    QByteArray m_buffer;
    QByteArray m_data;

};

} // anonymous

Application::Application(int& argc, char** argv)
    : QApplication(argc, argv)
    , m_settings(new Settings(this))
    , m_database(new Database(*m_settings, this))
    , m_model(new Model(*m_database, this))
    , m_networkManager(new QNetworkAccessManager(this))
    , m_torrentSession(new TorrentSession(this))
    , m_mainWindow(new MainWindow(*m_settings, *m_model, *this))
{
    connect(m_database, &Database::updated, m_model, &Model::update);

    connect(m_database, &Database::updated, this, &Application::completedDatabaseUpdate);
    connect(m_database, &Database::failedToUpdate, this, &Application::failedToUpdateDatabase);

    connect(m_torrentSession, &TorrentSession::torrentAdded, this, &Application::databaseDownloadStarted);
    connect(m_torrentSession, &TorrentSession::torrentFinished, this, &Application::torrentFinished);
    connect(m_torrentSession, &TorrentSession::failedToAddTorrent, this, &Application::failedToUpdateDatabase);

    connect(this, &Application::startedDatabaseUpdate, m_mainWindow, &MainWindow::showStartedDatabaseUpdate);
    connect(this, &Application::completedDatabaseUpdate, m_mainWindow, &MainWindow::showCompletedDatabaseUpdate);
    connect(this, &Application::failedToUpdateDatabase, m_mainWindow, &MainWindow::showDatabaseUpdateFailure);

    connect(this, &Application::databaseDownloadStarted, m_mainWindow, &MainWindow::showDatabaseDownloadStarted);
    connect(this, &Application::databaseImportStarted, m_mainWindow, &MainWindow::showDatabaseImportStarted);
}

Application::~Application()
{
}

int Application::exec()
{
    QTimer::singleShot(0, this, &Application::checkUpdateDatabase);

    m_mainWindow->setAttribute(Qt::WA_DeleteOnClose);
    m_mainWindow->show();

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
}

void Application::updateDatabase()
{
    emit startedDatabaseUpdate();

    QNetworkRequest request(m_settings->torrentUrl());
    request.setHeader(QNetworkRequest::UserAgentHeader, m_settings->userAgent());

    const auto reply = m_networkManager->get(request);

    connect(reply, &QNetworkReply::finished, [this, reply]()
    {
        reply->deleteLater();

        if (reply->error())
        {
            emit failedToUpdateDatabase(reply->errorString());
            return;
        }

        try
        {
            m_torrentSession->addTorrent(reply->readAll(), QDir::tempPath());
        }
        catch (const std::exception& exception)
        {
            emit failedToUpdateDatabase(exception.what());
        }
    });
}

void Application::torrentFinished(const libtorrent::torrent_handle& handle)
{
    try
    {
        const auto filePath = QDir::temp().filePath(QString::fromStdString(handle.torrent_file()->file_at(0).path));

        QFile file(filePath);

        if (!file.open(QIODevice::ReadOnly))
        {
            emit failedToUpdateDatabase(tr("Failed to open database at '%1'.").arg(filePath));
            return;
        }

        Decompressor decompressor;

        forever
        {
            const auto buffer = file.read(8 * 1024);

            if (buffer.isEmpty())
            {
                break;
            }

            decompressor.appendData(buffer);
        }

        // TODO

        emit databaseImportStarted();
    }
    catch (const std::exception& exception)
    {
        emit failedToUpdateDatabase(exception.what());
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
        if (!QProcess::startDetached(command.arg(url)))
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
        if (!QProcess::startDetached(command.arg(url)))
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

} // QMediathekView

int main(int argc, char** argv)
{
    QApplication::setOrganizationName(QMediathekView::projectName);
    QApplication::setApplicationName(QMediathekView::projectName);

    return QMediathekView::Application(argc, argv).exec();
}
