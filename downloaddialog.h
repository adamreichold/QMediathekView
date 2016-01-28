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
class Model;

class DownloadDialog : public QDialog
{
    Q_OBJECT
    Q_DISABLE_COPY(DownloadDialog)

public:
    DownloadDialog(
        const Settings& settings,
        const Model& model,
        const QModelIndex& index,
        QNetworkAccessManager* networkManager,
        QWidget* parent = 0
    );
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

    const QString m_url;
    const QString m_urlLarge;
    const QString m_urlSmall;

    QUrl selectedUrl() const;

    QNetworkAccessManager* m_networkManager;
    QNetworkReply* m_networkReply;
    QFile* m_file;

    QLineEdit* m_filePathEdit;

    QRadioButton* m_defaultButton;
    QRadioButton* m_smallButton;
    QRadioButton* m_largeButton;

    QPushButton* m_startButton;
    QPushButton* m_cancelButton;

    QProgressBar* m_progressBar;

};

} // Mediathek

#endif // DOWNLOADDIALOG_H
