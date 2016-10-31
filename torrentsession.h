#ifndef TORRENTSESSION_H
#define TORRENTSESSION_H

#include <QObject>

#include <libtorrent/session.hpp>
#include <libtorrent/torrent_handle.hpp>

namespace libtorrent
{
class session;
}

namespace QMediathekView
{

class TorrentSession : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(TorrentSession)

public:
    explicit TorrentSession(QObject* parent = 0);

    void addTorrent(const QByteArray& buffer, const QString& savePath);

signals:
    void torrentAdded(libtorrent::torrent_handle handle);
    void torrentFinished(libtorrent::torrent_handle handle);
    void failedToAddTorrent(QString error);

private:
    libtorrent::session m_session;

};

}

#endif // TORRENTSESSION_H
