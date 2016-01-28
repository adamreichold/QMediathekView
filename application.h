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

signals:
    void startedMirrorsUpdate();
    void completedMirrorsUpdate();
    void failedToUpdateMirrors(const QString& error);

    void startedDatabaseUpdate();
    void completedDatabaseUpdate();
    void failedToUpdateDatabase(const QString& error);

public:
    int exec();

    void play(const QModelIndex& index);
    void download(const QModelIndex& index);

    void checkUpdateMirrors();
    void checkUpdateDatabase();

private:
    void updateMirrors();
    void updateDatabase();

    template< typename Consumer >
    void downloadMirrors(const QString& url, const Consumer& consumer);

    template< typename Consumer >
    void downloadDatabase(const QString& url, const Consumer& consumer);

private:
    Settings* m_settings;
    Database* m_database;
    Model* m_model;

    QNetworkAccessManager* m_networkManager;

    MainWindow* m_mainWindow;

};

} // Mediathek

#endif // APPLICATION_H
