#ifndef APPLICATION_H
#define APPLICATION_H

#include <QApplication>

class QNetworkAccessManager;
class QTimer;

namespace Mediathek
{

class Settings;
class Database;
class Model;
class MainWindow;

class Application : public QApplication
{
    Q_OBJECT
    Q_DISABLE_COPY(Application)

public:
    Application(int& argc, char** argv);
    ~Application();

public:
    int exec();

signals:
    void startedMirrorListUpdate();
    void completedMirrorListUpdate();
    void failedToUpdateMirrorList(const QString& error);

    void startedDatabaseUpdate();
    void completedDatabaseUpdate();
    void failedToUpdateDatabase(const QString& error);

public slots:
    void play(const QModelIndex& index);
    void download(const QModelIndex& index);

    void checkUpdateMirrorList();
    void checkUpdateDatabase();

private slots:
    void updateMirrorList();
    void updateDatabase();

private:
    Settings* m_settings;
    Database* m_database;
    Model* m_model;

    QNetworkAccessManager* m_networkManager;

    MainWindow* m_mainWindow;

    QTimer* m_updateTimer;

};

} // Mediathek

#endif // APPLICATION_H
