#include "settingsdialog.h"

#include <QAction>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QLineEdit>
#include <QSpinBox>

#include "settings.h"

namespace Mediathek
{

SettingsDialog::SettingsDialog(
    Settings& settings,
    QWidget* parent)
    : QDialog(parent)
    , m_settings(settings)
{
    auto layout = new QFormLayout(this);
    setLayout(layout);

    m_mirrorListUpdateAfterDaysBox = new QSpinBox(this);
    m_mirrorListUpdateAfterDaysBox->setValue(m_settings.mirrorListUpdateAfterDays());
    m_mirrorListUpdateAfterDaysBox->setRange(3, 30);
    m_mirrorListUpdateAfterDaysBox->setPrefix(tr("after "));
    m_mirrorListUpdateAfterDaysBox->setSuffix(tr(" days"));
    layout->addRow(tr("Mirror list update"), m_mirrorListUpdateAfterDaysBox);

    m_databaseUpdateAfterHoursBox = new QSpinBox(this);
    m_databaseUpdateAfterHoursBox->setValue(m_settings.databaseUpdateAfterHours());
    m_databaseUpdateAfterHoursBox->setRange(3, 30);
    m_databaseUpdateAfterHoursBox->setPrefix(tr("after "));
    m_databaseUpdateAfterHoursBox->setSuffix(tr(" hours"));
    layout->addRow(tr("Database update"), m_databaseUpdateAfterHoursBox);

    m_playCommandEdit = new QLineEdit(this);
    m_playCommandEdit->setText(m_settings.playCommand());
    layout->addRow(tr("Play command"), m_playCommandEdit);

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

SettingsDialog::~SettingsDialog()
{
}

void SettingsDialog::accept()
{
    QDialog::accept();

    m_settings.setMirrorListUpdateAfterDays(m_mirrorListUpdateAfterDaysBox->value());
    m_settings.setDatabaseUpdateAfterHours(m_databaseUpdateAfterHoursBox->value());

    m_settings.setPlayCommand(m_playCommandEdit->text());
    m_settings.setDownloadFolder(m_downloadFolderEdit->text());

    m_settings.setPreferredUrl(Url(m_preferredUrlBox->currentData().toInt()));
}

void SettingsDialog::selectDownloadFolder()
{
    const auto downloadFolder = QFileDialog::getExistingDirectory(
                                    this, tr("Select download folder"),
                                    m_downloadFolderEdit->text()
                                );

    if (!downloadFolder.isNull())
    {
        m_downloadFolderEdit->setText(downloadFolder);
    }
}

} // Mediathek
