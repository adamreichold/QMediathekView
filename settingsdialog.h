#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>

class QComboBox;
class QLineEdit;
class QSpinBox;

namespace Mediathek
{

class Settings;

class SettingsDialog : public QDialog
{
    Q_OBJECT
    Q_DISABLE_COPY(SettingsDialog)

public:
    SettingsDialog(Settings& settings, QWidget* parent = 0);
    ~SettingsDialog();

public slots:
    void accept();

private slots:
    void selectDownloadFolder();

private:
    Settings& m_settings;

    QSpinBox* m_mirrorListUpdateAfterDaysBox;
    QSpinBox* m_databaseUpdateAfterHoursBox;

    QLineEdit* m_playCommandEdit;
    QLineEdit* m_downloadFolderEdit;

    QComboBox* m_preferredUrlBox;

};

} // Mediathek

#endif // SETTINGSDIALOG_H
