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

#include <memory>
#include <random>

#include <QDomDocument>
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
#include "mainwindow.h"
#include "downloaddialog.h"

namespace
{

using namespace QMediathekView;

const auto projectName = QStringLiteral("QMediathekView");

namespace Tags
{

const auto root = QStringLiteral("Mediathek");
const auto server = QStringLiteral("Server");
const auto url = QStringLiteral("URL");

} // Tags

QString randomItem(const QStringList& list)
{
    std::random_device device;
    std::default_random_engine generator(device());
    std::uniform_int_distribution<> distribution(0, list.size() - 1);

    return list.at(distribution(generator));
}

class Decompressor
{
public:
    Decompressor()
        : m_stream(LZMA_STREAM_INIT)
    {
        Q_UNUSED(lzma_stream_decoder(&m_stream, UINT64_MAX, LZMA_TELL_NO_CHECK));
    }

    void appendData(const QByteArray& data)
    {
        m_stream.next_in = reinterpret_cast< const std::uint8_t* >(data.constData());
        m_stream.avail_in = data.size();

        for (lzma_ret result = LZMA_OK; result == LZMA_OK;)
        {
            m_stream.next_out = m_buffer;
            m_stream.avail_out = sizeof(m_buffer);

            result = lzma_code(&m_stream, LZMA_RUN);

            m_data.append(reinterpret_cast< const char* >(m_buffer), sizeof(m_buffer) - m_stream.avail_out);
        }
    }

    const QByteArray& data() const
    {
        return m_data;
    }

private:
    lzma_stream m_stream;
    std::uint8_t m_buffer[64 * 1024];
    QByteArray m_data;

};

} // anonymous

namespace QMediathekView
{

Application::Application(int& argc, char** argv)
    : QApplication(argc, argv)
    , m_settings(new Settings(this))
    , m_database(new Database(*m_settings, this))
    , m_model(new Model(*m_database, this))
    , m_networkManager(new QNetworkAccessManager(this))
    , m_mainWindow(new MainWindow(*m_settings, *m_model))
{
    connect(m_database, &Database::updated, m_model, &Model::update);

    connect(m_database, &Database::updated, this, &Application::completedDatabaseUpdate);
    connect(m_database, &Database::failedToUpdate, this, &Application::failedToUpdateDatabase);

    connect(this, &Application::startedMirrorsUpdate, m_mainWindow, &MainWindow::showStartedMirrorsUpdate);
    connect(this, &Application::completedMirrorsUpdate, m_mainWindow, &MainWindow::showCompletedMirrorsUpdate);
    connect(this, &Application::failedToUpdateMirrors, m_mainWindow, &MainWindow::showMirrorsUpdateFailure);

    connect(this, &Application::startedDatabaseUpdate, m_mainWindow, &MainWindow::showStartedDatabaseUpdate);
    connect(this, &Application::completedDatabaseUpdate, m_mainWindow, &MainWindow::showCompletedDatabaseUpdate);
    connect(this, &Application::failedToUpdateDatabase, m_mainWindow, &MainWindow::showDatabaseUpdateFailure);

    connect(m_mainWindow, &MainWindow::databaseUpdateRequested, this, &Application::updateDatabase);

    connect(m_mainWindow, &MainWindow::playRequested, this, &Application::play);
    connect(m_mainWindow, &MainWindow::downloadRequested, this, &Application::download);
}

Application::~Application()
{
}

int Application::exec()
{
    QTimer::singleShot(0, this, &Application::checkUpdateMirrors);

    m_mainWindow->setAttribute(Qt::WA_DeleteOnClose);
    m_mainWindow->show();

    return QApplication::exec();
}

void Application::play(const QModelIndex& index)
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

    if (!QProcess::startDetached(m_settings->playCommand().arg(url)))
    {
        QMessageBox::critical(m_mainWindow, tr("Critical"), tr("Failed to execute play command."));
    }
}

void Application::download(const QModelIndex& index)
{
    const auto dialog = new DownloadDialog(
        *m_settings,
        *m_model,
        index,
        m_networkManager);

    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

void Application::checkUpdateMirrors()
{
    const auto updateAfter = m_settings->mirrorsUpdateAfterDays();
    const auto updatedOn = m_settings->mirrorsUpdatedOn();
    const auto updatedBefore = updatedOn.daysTo(QDateTime::currentDateTime());

    if (!updatedOn.isValid() || updateAfter < updatedBefore)
    {
        updateMirrors();
    }
    else
    {
        checkUpdateDatabase();
    }
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

void Application::updateMirrors()
{
    emit startedMirrorsUpdate();

    downloadMirrors(m_settings->fullListUrl(), [this](const QStringList& mirrors)
    {
        m_settings->setFullListMirrors(mirrors);

        downloadMirrors(m_settings->partialListUrl(), [this](const QStringList& mirrors)
        {
            m_settings->setPartialListMirrors(mirrors);
            m_settings->setMirrorsUpdatedOn();

            emit completedMirrorsUpdate();

            QTimer::singleShot(0, this, &Application::checkUpdateDatabase);
        });
    });
}

void Application::updateDatabase()
{
    emit startedDatabaseUpdate();

    const auto updatedOn = m_settings->databaseUpdatedOn();
    const auto fullUpdateOn = QDateTime(QDate::currentDate(), QTime(9, 0));

    if (!updatedOn.isValid() || updatedOn < fullUpdateOn)
    {
        const auto url = randomItem(m_settings->fullListMirrors());

        downloadDatabase(url, [this](const QByteArray& data)
        {
            m_database->fullUpdate(data);
        });
    }
    else
    {
        const auto url = randomItem(m_settings->partialListMirrors());

        downloadDatabase(url, [this](const QByteArray& data)
        {
            m_database->partialUpdate(data);
        });
    }
}

template< typename Consumer >
void Application::downloadMirrors(const QString& url, const Consumer& consumer)
{
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, m_settings->userAgent());

    const auto reply = m_networkManager->get(request);

    connect(reply, &QNetworkReply::finished, [this, consumer, reply]()
    {
        reply->deleteLater();

        if (reply->error())
        {
            emit failedToUpdateMirrors(reply->errorString());
            return;
        }

        QStringList mirrors;

        {
            QDomDocument document;
            document.setContent(reply);

            const auto root = document.documentElement();
            if (root.tagName() != Tags::root)
            {
                emit failedToUpdateMirrors(tr("Received a malformed mirror list."));
                return;
            }

            auto server = root.firstChildElement(Tags::server);

            while (!server.isNull())
            {
                const auto url = server.firstChildElement(Tags::url).text();

                if (!url.isEmpty())
                {
                    mirrors.append(url);
                }

                server = server.nextSiblingElement(Tags::server);
            }
        }

        if (mirrors.isEmpty())
        {
            emit failedToUpdateMirrors(tr("Received an empty mirror list."));
            return;
        }

        consumer(mirrors);
    });
}

template< typename Consumer >
void Application::downloadDatabase(const QString& url, const Consumer& consumer)
{
    const auto decompressor = std::make_shared< Decompressor >();

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, m_settings->userAgent());

    const auto reply = m_networkManager->get(request);

    connect(reply, &QNetworkReply::readyRead, [this, reply, decompressor]()
    {
        if (reply->error())
        {
            return;
        }

        decompressor->appendData(reply->readAll());
    });

    connect(reply, &QNetworkReply::finished, [this, consumer, reply, decompressor]()
    {
        reply->deleteLater();

        if (reply->error())
        {
            emit failedToUpdateDatabase(reply->errorString());
            return;
        }

        decompressor->appendData(reply->readAll());

        consumer(decompressor->data());
    });
}

} // QMediathekView

int main(int argc, char** argv)
{
    QApplication::setOrganizationName(projectName);
    QApplication::setApplicationName(projectName);

    return QMediathekView::Application(argc, argv).exec();
}
