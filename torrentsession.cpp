#include "torrentsession.h"

#include <libtorrent/alert_types.hpp>
#include <libtorrent/bencode.hpp>
#include <libtorrent/torrent_info.hpp>

namespace QMediathekView
{

TorrentSession::TorrentSession(QObject* parent)
    : QObject(parent)
    , m_session()
{
    qRegisterMetaType< libtorrent::torrent_handle >("libtorrent::torrent_handle");

    m_session.set_alert_mask(libtorrent::alert::error_notification | libtorrent::alert::status_notification);
    m_session.set_alert_dispatch([this](std::auto_ptr< libtorrent::alert > alert)
    {
        if(auto* addTorrentAlert = libtorrent::alert_cast< libtorrent::add_torrent_alert >(alert.get()))
        {
            if(addTorrentAlert->error)
            {
                emit failedToAddTorrent(QString::fromStdString(addTorrentAlert->error.message()));
            }
            else
            {
                emit torrentAdded(addTorrentAlert->handle);
            }
        }
        else if(auto* torrentFinishedAlert = libtorrent::alert_cast< libtorrent::torrent_finished_alert >(alert.get()))
        {
            emit torrentFinished(torrentFinishedAlert->handle);
        }
    });
}

void TorrentSession::addTorrent(const QByteArray& buffer, const QString& savePath)
{
    libtorrent::add_torrent_params params;
    params.ti = new libtorrent::torrent_info(buffer.data(), buffer.size());
    params.save_path = savePath.toStdString();

    m_session.async_add_torrent(params);
}

}
