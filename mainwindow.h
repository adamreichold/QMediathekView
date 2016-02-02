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

class MainWindow : public QMainWindow
{
    Q_OBJECT
    Q_DISABLE_COPY(MainWindow)

public:
    MainWindow(Settings& settings, Model& model, QWidget* parent = 0);
    ~MainWindow();

signals:
    void databaseUpdateRequested();

    void playRequested(const QModelIndex& index);
    void downloadRequested(const QModelIndex& index);

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

    void playPressed();
    void downloadPressed();

    void timeout();
    void activated(const QModelIndex& index);
    void currentChanged(const QModelIndex& current, const QModelIndex& previous);

private:
    Settings& m_settings;
    Model& m_model;

    QTableView* m_tableView;

    QTimer* m_searchTimer;

    QComboBox* m_channelBox;
    QComboBox* m_topicBox;
    QLineEdit* m_titleEdit;

    QTextEdit* m_descriptionEdit;
    QLabel* m_websiteLabel;

};

} // QMediathekView

#endif // MAINWINDOW_H
