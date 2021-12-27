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

#ifndef DOWNLOADDIALOG_H
#define DOWNLOADDIALOG_H

#include <memory>

#include <QDialog>
#include <QUrl>

class QFile;
class QLineEdit;
class QNetworkAccessManager;
class QNetworkReply;
class QProgressBar;
class QPushButton;
class QRadioButton;

namespace QMediathekView
{

class Settings;
class Model;

class DownloadDialog : public QDialog
{
    Q_OBJECT
    Q_DISABLE_COPY(DownloadDialog)

public:
    DownloadDialog(
        const Settings& settings,
        QNetworkAccessManager* networkManager,
        const QString& title,
        const QUrl& url,
        QWidget* parent = 0);
    ~DownloadDialog();

private:
    void selectFilePath();

    void start();
    void cancel();

    void readyRead();
    void downloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void finished();

private:
    const Settings& m_settings;

    const QString m_title;
    const QUrl m_url;

    QNetworkAccessManager* m_networkManager;
    std::unique_ptr< QNetworkReply > m_networkReply;
    std::unique_ptr< QFile > m_file;

    QLineEdit* m_filePathEdit;

    QPushButton* m_startButton;
    QPushButton* m_cancelButton;

    QProgressBar* m_progressBar;

};

} // QMediathekView

#endif // DOWNLOADDIALOG_H
