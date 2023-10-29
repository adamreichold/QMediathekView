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

#ifndef APPLICATION_H
#define APPLICATION_H

#include <QApplication>

class QNetworkAccessManager;

namespace QMediathekView
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
    Application(int& argc, char** argv, bool headless);
    ~Application();

signals:
    void startedDatabaseUpdate();
    void completedDatabaseUpdate();
    void failedToUpdateDatabase(const QString& error);

public:
    int exec();

    void playPreferred(const QModelIndex& index) const;
    void playDefault(const QModelIndex& index) const;
    void playSmall(const QModelIndex& index) const;
    void playLarge(const QModelIndex& index) const;

    void downloadPreferred(const QModelIndex& index) const;
    void downloadDefault(const QModelIndex& index) const;
    void downloadSmall(const QModelIndex& index) const;
    void downloadLarge(const QModelIndex& index) const;

    void checkUpdateDatabase();
    void updateDatabase();

    QString preferredUrl(const QModelIndex& index) const;

private:
    void startPlay(const QString& url) const;
    void startDownload(const QString& title, const QString& url) const;

    void logStartedDatabaseUpdate();
    void logCompletedDatabaseUpdate();
    void logDatabaseUpdateFailure(const QString& error);

private:
    Settings* m_settings;
    Database* m_database;
    Model* m_model;

    QNetworkAccessManager* m_networkManager;

    MainWindow* m_mainWindow;

};

} // QMediathekView

#endif // APPLICATION_H
