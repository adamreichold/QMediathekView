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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class QComboBox;
class QLabel;
class QLineEdit;
class QTableView;
class QTextEdit;
class QTimer;

namespace QMediathekView
{

class Settings;
class Model;
class UrlButton;
class Application;

class MainWindow : public QMainWindow
{
    Q_OBJECT
    Q_DISABLE_COPY(MainWindow)

public:
    MainWindow(Settings& settings, Model& model, Application& application, QWidget* parent = 0);
    ~MainWindow();

public:
    void showStartedMirrorsUpdate();
    void showCompletedMirrorsUpdate();
    void showMirrorsUpdateFailure(const QString& error);

    void showStartedDatabaseUpdate();
    void showCompletedDatabaseUpdate();
    void showDatabaseUpdateFailure(const QString& error);

private:
    void resetFilterPressed();
    void updateDatabasePressed();
    void editSettingsPressed();

    void playClicked();
    void playDefaultTriggered();
    void playSmallTriggered();
    void playLargeTriggered();

    void downloadClicked();
    void downloadDefaultTriggered();
    void downloadSmallTriggered();
    void downloadLargeTriggered();

    void timeout();
    void doubleClicked(const QModelIndex& index);
    void currentChanged(const QModelIndex& current, const QModelIndex& previous);
    void customContextMenuRequested(const QPoint& pos);

private:
    Settings& m_settings;
    Model& m_model;
    Application& m_application;

    QTableView* m_tableView;

    QTimer* m_searchTimer;

    QComboBox* m_channelBox;
    QComboBox* m_topicBox;
    QLineEdit* m_titleEdit;

    QTextEdit* m_descriptionEdit;
    QLabel* m_websiteLabel;

    UrlButton* m_playButton;
    UrlButton* m_downloadButton;

};

} // QMediathekView

#endif // MAINWINDOW_H
