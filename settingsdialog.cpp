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

#include "settingsdialog.h"

#include <QAction>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QLineEdit>
#include <QSpinBox>

#include "settings.h"

namespace QMediathekView
{

SettingsDialog::SettingsDialog(
    Settings& settings,
    QWidget* parent)
    : QDialog(parent)
    , m_settings(settings)
{
    auto layout = new QFormLayout(this);
    setLayout(layout);

    m_mirrorsUpdateAfterDaysBox = new QSpinBox(this);
    m_mirrorsUpdateAfterDaysBox->setValue(m_settings.mirrorsUpdateAfterDays());
    m_mirrorsUpdateAfterDaysBox->setRange(3, 30);
    m_mirrorsUpdateAfterDaysBox->setPrefix(tr("after "));
    m_mirrorsUpdateAfterDaysBox->setSuffix(tr(" days"));
    layout->addRow(tr("Mirrors update"), m_mirrorsUpdateAfterDaysBox);

    m_databaseUpdateAfterHoursBox = new QSpinBox(this);
    m_databaseUpdateAfterHoursBox->setValue(m_settings.databaseUpdateAfterHours());
    m_databaseUpdateAfterHoursBox->setRange(3, 30);
    m_databaseUpdateAfterHoursBox->setPrefix(tr("after "));
    m_databaseUpdateAfterHoursBox->setSuffix(tr(" hours"));
    layout->addRow(tr("Database update"), m_databaseUpdateAfterHoursBox);

    m_playCommandEdit = new QLineEdit(this);
    m_playCommandEdit->setText(m_settings.playCommand());
    layout->addRow(tr("Play command"), m_playCommandEdit);

    m_downloadCommandEdit = new QLineEdit(this);
    m_downloadCommandEdit->setText(m_settings.downloadCommand());
    layout->addRow(tr("Download command"), m_downloadCommandEdit);

    m_downloadFolderEdit = new QLineEdit(this);
    m_downloadFolderEdit->setText(m_settings.downloadFolder().absolutePath());
    layout->addRow(tr("Download folder"), m_downloadFolderEdit);

    const auto selectDownloadFolderAction = m_downloadFolderEdit->addAction(QIcon::fromTheme(QStringLiteral("document-open")), QLineEdit::TrailingPosition);
    connect(selectDownloadFolderAction, &QAction::triggered, this, &SettingsDialog::selectDownloadFolder);

    m_preferredUrlBox = new QComboBox(this);
    m_preferredUrlBox->addItem(tr("Default"), int(Url::Default));
    m_preferredUrlBox->addItem(tr("Small"), int(Url::Small));
    m_preferredUrlBox->addItem(tr("Large"), int(Url::Large));
    m_preferredUrlBox->setCurrentIndex(m_preferredUrlBox->findData(int(m_settings.preferredUrl())));
    layout->addRow(tr("Preferred URL"), m_preferredUrlBox);

    auto buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    layout->addWidget(buttonBox);

    connect(buttonBox, &QDialogButtonBox::accepted, this, &SettingsDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &SettingsDialog::reject);
}

SettingsDialog::~SettingsDialog() = default;

void SettingsDialog::accept()
{
    QDialog::accept();

    m_settings.setMirrorsUpdateAfterDays(m_mirrorsUpdateAfterDaysBox->value());
    m_settings.setDatabaseUpdateAfterHours(m_databaseUpdateAfterHoursBox->value());

    m_settings.setPlayCommand(m_playCommandEdit->text());
    m_settings.setDownloadCommand(m_downloadCommandEdit->text());

    m_settings.setDownloadFolder(m_downloadFolderEdit->text());

    m_settings.setPreferredUrl(Url(m_preferredUrlBox->currentData().toInt()));
}

void SettingsDialog::selectDownloadFolder()
{
    const auto downloadFolder = QFileDialog::getExistingDirectory(
                                    this, tr("Select download folder"),
                                    m_downloadFolderEdit->text());

    if (!downloadFolder.isNull())
    {
        m_downloadFolderEdit->setText(downloadFolder);
    }
}

} // QMediathekView
