#include "mainwindow.h"

#include <QComboBox>
#include <QDesktopServices>
#include <QDockWidget>
#include <QFormLayout>
#include <QGridLayout>
#include <QHeaderView>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QShortcut>
#include <QStatusBar>
#include <QTableView>
#include <QTextEdit>
#include <QTimer>

#include "settings.h"
#include "model.h"
#include "settingsdialog.h"

namespace
{

using namespace Mediathek;

constexpr auto messageTimeout = 2 * 1000;
constexpr auto errorMessageTimeout = 10 * 1000;

constexpr auto searchTimeout = 200;

constexpr auto minimumChannelLength = 4;
constexpr auto minimumTopicLength = 12;

} // anonymous

namespace Mediathek
{

MainWindow::MainWindow(Settings& settings, Model& model, QWidget* parent)
    : QMainWindow(parent)
    , m_settings(settings)
    , m_model(model)
{
    m_tableView = new QTableView(this);
    m_tableView->setModel(&m_model);
    setCentralWidget(m_tableView);

    m_tableView->setAlternatingRowColors(true);
    m_tableView->sortByColumn(0, Qt::AscendingOrder);
    m_tableView->setSortingEnabled(true);
    m_tableView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableView->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_tableView->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);

    m_tableView->verticalHeader()->setVisible(false);
    m_tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_tableView->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);

    connect(m_tableView, &QTableView::activated, this, &MainWindow::activated);
    connect(m_tableView->selectionModel(), &QItemSelectionModel::currentChanged, this, &MainWindow::currentChanged);

    const auto searchDock = new QDockWidget(tr("Search"), this);
    searchDock->setObjectName(QStringLiteral("searchDock"));
    addDockWidget(Qt::BottomDockWidgetArea, searchDock);

    const auto searchWidget = new QWidget(searchDock);
    searchDock->setWidget(searchWidget);

    const auto searchLayout = new QFormLayout(searchWidget);
    searchWidget->setLayout(searchLayout);

    m_searchTimer = new QTimer(this);
    m_searchTimer->setInterval(searchTimeout);

    m_channelBox = new QComboBox(searchWidget);
    m_channelBox->setModel(m_model.channels());
    m_channelBox->setEditable(true);
    m_channelBox->setMinimumContentsLength(minimumChannelLength);
    m_channelBox->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLength);
    searchLayout->addRow(tr("Channel"), m_channelBox);

    m_topicBox = new QComboBox(searchWidget);
    m_topicBox->setModel(m_model.topics());
    m_topicBox->setEditable(true);
    m_topicBox->setMinimumContentsLength(minimumTopicLength);
    m_topicBox->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLength);
    searchLayout->addRow(tr("Topic"), m_topicBox);

    m_titleEdit = new QLineEdit(searchWidget);
    m_titleEdit->setFocus();
    searchLayout->addRow(tr("Title"), m_titleEdit);

    connect(m_searchTimer, &QTimer::timeout, this, &MainWindow::timeout);

    connect(m_channelBox, &QComboBox::currentTextChanged, [this]()
    {
        m_searchTimer->start();
    });
    connect(m_topicBox, &QComboBox::currentTextChanged, [this]()
    {
        m_searchTimer->start();
    });
    connect(m_titleEdit, &QLineEdit::textChanged, [this]()
    {
        m_searchTimer->start();
    });

    const auto buttonsWidget = new QWidget(searchWidget);
    searchLayout->addWidget(buttonsWidget);
    searchLayout->setAlignment(buttonsWidget, Qt::AlignRight);

    const auto buttonsLayout = new QBoxLayout(QBoxLayout::LeftToRight, buttonsWidget);
    buttonsWidget->setLayout(buttonsLayout);
    buttonsLayout->setSizeConstraint(QLayout::SetFixedSize);

    const auto resetFilterButton = new QPushButton(QIcon::fromTheme(QStringLiteral("edit-clear")), QString(), buttonsWidget);
    buttonsLayout->addWidget(resetFilterButton);

    const auto updateDatabaseButton = new QPushButton(QIcon::fromTheme(QStringLiteral("view-refresh")), QString(), buttonsWidget);
    buttonsLayout->addWidget(updateDatabaseButton);

    const auto editSettingsButton = new QPushButton(QIcon::fromTheme(QStringLiteral("preferences-system")), QString(), buttonsWidget);
    buttonsLayout->addWidget(editSettingsButton);

    connect(resetFilterButton, &QPushButton::pressed, this, &MainWindow::resetFilterPressed);
    connect(updateDatabaseButton, &QPushButton::pressed, this, &MainWindow::updateDatabasePressed);
    connect(editSettingsButton, &QPushButton::pressed, this, &MainWindow::editSettingsPressed);

    const auto detailsDock = new QDockWidget(tr("Details"), this);
    detailsDock->setObjectName(QStringLiteral("detailsDock"));
    addDockWidget(Qt::BottomDockWidgetArea, detailsDock);

    const auto detailsWidget = new QWidget(detailsDock);
    detailsDock->setWidget(detailsWidget);

    const auto detailsLayout = new QGridLayout(detailsWidget);
    detailsWidget->setLayout(detailsLayout);

    detailsLayout->setRowStretch(2, 1);
    detailsLayout->setColumnStretch(1, 1);

    m_descriptionEdit = new QTextEdit(detailsWidget);
    m_descriptionEdit->setReadOnly(true);
    m_descriptionEdit->setLineWrapMode(QTextEdit::WidgetWidth);
    detailsLayout->addWidget(m_descriptionEdit, 0, 1, 3, 1);

    m_websiteLabel = new QLabel(detailsWidget);
    detailsLayout->addWidget(m_websiteLabel, 3, 1);

    connect(m_websiteLabel, &QLabel::linkActivated, QDesktopServices::openUrl);

    const auto playButton = new QPushButton(QIcon::fromTheme(QStringLiteral("media-playback-start")), QString(), detailsWidget);
    detailsLayout->addWidget(playButton, 0, 0);

    const auto downloadButton = new QPushButton(QIcon::fromTheme(QStringLiteral("media-record")), QString(), detailsWidget);
    detailsLayout->addWidget(downloadButton, 1, 0);

    connect(playButton, &QPushButton::pressed, this, &MainWindow::playPressed);
    connect(downloadButton, &QPushButton::pressed, this, &MainWindow::downloadPressed);

    const auto quitShortcut = new QShortcut(QKeySequence::Quit, this);
    connect(quitShortcut, &QShortcut::activated, this, &MainWindow::close);

    restoreGeometry(m_settings.mainWindowGeometry());
    restoreState(m_settings.mainWindowState());

    statusBar()->showMessage(tr("Ready"), messageTimeout);
}

MainWindow::~MainWindow()
{
    m_settings.setMainWindowGeometry(saveGeometry());
    m_settings.setMainWindowState(saveState());
}

void MainWindow::showStartedMirrorListUpdate()
{
    statusBar()->showMessage(tr("Started mirror list update..."), messageTimeout);
}

void MainWindow::showCompletedMirrorListUpdate()
{
    statusBar()->showMessage(tr("Successfully updated mirror list."), messageTimeout);
}

void MainWindow::showMirrorListUpdateFailure(const QString& error)
{
    statusBar()->showMessage(tr("Failed to updated mirror list: %1").arg(error), errorMessageTimeout);
}

void MainWindow::showStartedDatabaseUpdate()
{
    statusBar()->showMessage(tr("Started database update..."), messageTimeout);
}

void MainWindow::showCompletedDatabaseUpdate()
{
    statusBar()->showMessage(tr("Successfully updated database."), messageTimeout);
}

void MainWindow::showDatabaseUpdateFailure(const QString& error)
{
    statusBar()->showMessage(tr("Failed to updated database: %1").arg(error), errorMessageTimeout);
}

void MainWindow::resetFilterPressed()
{
    m_channelBox->clearEditText();
    m_topicBox->clearEditText();
    m_titleEdit->clear();;
}

void MainWindow::updateDatabasePressed()
{
    emit databaseUpdateRequested();
}

void MainWindow::editSettingsPressed()
{
    SettingsDialog(m_settings, this).exec();
}

void MainWindow::playPressed()
{
    emit playRequested(m_tableView->currentIndex());
}

void MainWindow::downloadPressed()
{
    emit downloadRequested(m_tableView->currentIndex());
}

void MainWindow::timeout()
{
    m_searchTimer->stop();

    const auto channel = m_channelBox->currentText();
    const auto topic = m_topicBox->currentText();
    const auto title = m_titleEdit->text();

    m_model.filter(channel, topic, title);
}

void MainWindow::activated(const QModelIndex& index)
{
    emit playRequested(index);
}

void MainWindow::currentChanged(const QModelIndex& current, const QModelIndex& /* previous */)
{
    m_descriptionEdit->setPlainText(m_model.description(current));
    m_websiteLabel->setText(QStringLiteral("<a href=\"%1\">%1</a>").arg(m_model.website(current)));
}

} // Mediathek
