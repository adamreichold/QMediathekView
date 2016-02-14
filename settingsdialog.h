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

#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>

class QComboBox;
class QLineEdit;
class QSpinBox;

namespace QMediathekView
{

class Settings;

class SettingsDialog : public QDialog
{
    Q_OBJECT
    Q_DISABLE_COPY(SettingsDialog)

public:
    SettingsDialog(Settings& settings, QWidget* parent = 0);
    ~SettingsDialog();

public:
    void accept();

private:
    void selectDownloadFolder();

private:
    Settings& m_settings;

    QSpinBox* m_mirrorsUpdateAfterDaysBox;
    QSpinBox* m_databaseUpdateAfterHoursBox;

    QLineEdit* m_playCommandEdit;
    QLineEdit* m_downloadCommandEdit;

    QLineEdit* m_downloadFolderEdit;

    QComboBox* m_preferredUrlBox;

};

} // QMediathekView

#endif // SETTINGSDIALOG_H
