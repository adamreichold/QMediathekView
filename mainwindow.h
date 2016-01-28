#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class QComboBox;
class QLabel;
class QLineEdit;
class QTableView;
class QTextEdit;
class QTimer;

namespace Mediathek
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
    void showStartedMirrorListUpdate();
    void showCompletedMirrorListUpdate();
    void showMirrorListUpdateFailure(const QString& error);

    void showStartedDatabaseUpdate();
    void showCompletedDatabaseUpdate();
    void showDatabaseUpdateFailure(const QString& error);

    void applyFilter();
    void resetFilter();

private:
    void resetFilterPressed();
    void updateDatabasePressed();
    void editSettingsPressed();

    void playPressed();
    void downloadPressed();

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

} // Mediathek

#endif // MAINWINDOW_H
