#include "application.h"

#include <memory>
#include <random>

#include <QDomDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
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

using namespace Mediathek;

const auto updateInterval = 60 * 60 * 1000;
const auto mirrorListUrl = QStringLiteral("http://zdfmediathk.sourceforge.net/akt.xml");

namespace Tags
{

const auto root = QStringLiteral("Mediathek");
const auto server = QStringLiteral("Server");
const auto url = QStringLiteral("URL");

} // Tags

const QString& randomItem(const QStringList& list)
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

namespace Mediathek
{

Application::Application(int& argc, char** argv)
    : QApplication(argc, argv)
    , m_settings(new Settings(this))
    , m_database(new Database(*m_settings, this))
    , m_model(new Model(*m_database, this))
    , m_networkManager(new QNetworkAccessManager(this))
    , m_mainWindow(new MainWindow(*m_settings, *m_model))
    , m_updateTimer(new QTimer(this))
{
    connect(m_database, &Database::updated, this, &Application::completedDatabaseUpdate);
    connect(m_database, &Database::failedToUpdate, this, &Application::failedToUpdateDatabase);

    connect(m_database, &Database::updated, m_model, &Model::reset);
    connect(m_database, &Database::updated, m_mainWindow, &MainWindow::resetFilter);

    connect(this, &Application::startedMirrorListUpdate, m_mainWindow, &MainWindow::showStartedMirrorListUpdate);
    connect(this, &Application::completedMirrorListUpdate, m_mainWindow, &MainWindow::showCompletedMirrorListUpdate);
    connect(this, &Application::failedToUpdateMirrorList, m_mainWindow, &MainWindow::showMirrorListUpdateFailure);

    connect(this, &Application::startedDatabaseUpdate, m_mainWindow, &MainWindow::showStartedDatabaseUpdate);
    connect(this, &Application::completedDatabaseUpdate, m_mainWindow, &MainWindow::showCompletedDatabaseUpdate);
    connect(this, &Application::failedToUpdateDatabase, m_mainWindow, &MainWindow::showDatabaseUpdateFailure);

    connect(m_mainWindow, &MainWindow::databaseUpdateRequested, this, &Application::updateDatabase);

    connect(m_mainWindow, &MainWindow::playRequested, this, &Application::play);
    connect(m_mainWindow, &MainWindow::downloadRequested, this, &Application::download);

    connect(m_updateTimer, &QTimer::timeout, this, &Application::checkUpdateMirrorList, Qt::QueuedConnection);
    connect(this, &Application::completedMirrorListUpdate, this, &Application::checkUpdateDatabase, Qt::QueuedConnection);
}

Application::~Application()
{
}

int Application::exec()
{
    m_updateTimer->start(updateInterval);

    QTimer::singleShot(0, this, &Application::checkUpdateMirrorList);

    m_mainWindow->setAttribute(Qt::WA_DeleteOnClose);
    m_mainWindow->show();

    return QApplication::exec();
}

void Application::play(const quintptr id)
{
    auto url = m_database->url(id, Database::UrlLarge);

    if (url.isEmpty())
    {
        url = m_database->url(id, Database::UrlDefault);
    }

    if (url.isEmpty())
    {
        url = m_database->url(id, Database::UrlSmall);
    }

    QProcess::startDetached(m_settings->playCommand().arg(url.toString()));
}

void Application::download(const quintptr id)
{
    const auto dialog = new DownloadDialog(
        *m_settings,
        m_database->title(id),
        m_database->url(id, Database::UrlDefault),
        m_database->url(id, Database::UrlLarge),
        m_database->url(id, Database::UrlSmall),
        m_networkManager
    );

    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->show();
}

void Application::checkUpdateMirrorList()
{
    const auto updateAfter = m_settings->mirrorListUpdateAfterDays();
    const auto updatedOn = m_settings->mirrorListUpdatedOn();
    const auto updatedBefore = updatedOn.daysTo(QDateTime::currentDateTime());

    if (!updatedOn.isValid() || updateAfter < updatedBefore)
    {
        updateMirrorList();
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

void Application::updateMirrorList()
{
    emit startedMirrorListUpdate();

    QNetworkRequest request(mirrorListUrl);
    request.setHeader(QNetworkRequest::UserAgentHeader, m_settings->userAgent());

    const auto reply = m_networkManager->get(request);

    connect(reply, &QNetworkReply::finished, [this, reply]()
    {
        reply->deleteLater();

        if (reply->error())
        {
            emit failedToUpdateMirrorList(reply->errorString());
            return;
        }

        QDomDocument document;
        document.setContent(reply);

        const auto root = document.documentElement();
        if (root.tagName() != Tags::root)
        {
            emit failedToUpdateMirrorList(tr("Received a malformed mirror list."));
            return;
        }

        QStringList mirrorList;

        for (
            auto server = root.firstChildElement(Tags::server);
            !server.isNull();
            server = server.nextSiblingElement(Tags::server)
        )
        {
            const auto url = server.firstChildElement(Tags::url).text();

            if (!url.isEmpty())
            {
                mirrorList.append(url);
            }
        }

        if (mirrorList.isEmpty())
        {
            emit failedToUpdateMirrorList(tr("Received an empty mirror list."));
            return;
        }

        m_settings->setMirrorList(mirrorList);
        m_settings->setMirrorListUpdatedOn();

        emit completedMirrorListUpdate();
    });
}

void Application::updateDatabase()
{
    emit startedDatabaseUpdate();

    QNetworkRequest request(randomItem(m_settings->mirrorList()));
    request.setHeader(QNetworkRequest::UserAgentHeader, m_settings->userAgent());

    const auto reply = m_networkManager->get(request);

    const auto decompressor = std::make_shared< Decompressor >();

    connect(reply, &QNetworkReply::readyRead, [this, reply, decompressor]()
    {
        if (reply->error())
        {
            return;
        }

        decompressor->appendData(reply->readAll());
    });

    connect(reply, &QNetworkReply::finished, [this, reply, decompressor]()
    {
        reply->deleteLater();

        if (reply->error())
        {
            emit failedToUpdateDatabase(reply->errorString());
            return;
        }

        decompressor->appendData(reply->readAll());

        m_database->update(decompressor->data());
    });
}

} // Mediathek
