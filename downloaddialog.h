#ifndef DOWNLOADDIALOG_H
#define DOWNLOADDIALOG_H

#include <QDialog>
#include <QUrl>

class QFile;
class QLineEdit;
class QNetworkAccessManager;
class QNetworkReply;
class QProgressBar;
class QPushButton;
class QRadioButton;

namespace Mediathek
{

class Settings;

class DownloadDialog : public QDialog
{
    Q_OBJECT
    Q_DISABLE_COPY(DownloadDialog)

public:
    DownloadDialog(
        const Settings& settings,
        const QString& title,
        const QUrl& url,
        const QUrl& urlLarge,
        const QUrl& urlSmall,
        QNetworkAccessManager* networkManager,
        QWidget* parent = 0
    );
    ~DownloadDialog();

private slots:
    void selectFilePath();

    void start();
    void cancel();

    void readyRead();
    void finished();

    void downloadProgress(qint64 bytesReceived, qint64 bytesTotal);

private:
    const Settings& m_settings;

    const QString m_title;
    const QUrl m_url;
    const QUrl m_urlLarge;
    const QUrl m_urlSmall;

    const QUrl& selectedUrl() const;

    QNetworkAccessManager* m_networkManager;
    QNetworkReply* m_networkReply;
    QFile* m_file;

    QLineEdit* m_filePathEdit;

    QRadioButton* m_defaultButton;
    QRadioButton* m_largeButton;
    QRadioButton* m_smallButton;

    QPushButton* m_startButton;
    QPushButton* m_cancelButton;

    QProgressBar* m_progressBar;

};

} // Mediathek

#endif // DOWNLOADDIALOG_H
