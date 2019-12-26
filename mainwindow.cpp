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

#include "mainwindow.h"

#include <functional>

#include <QClipboard>
#include <QComboBox>
#include <QDockWidget>
#include <QFormLayout>
#include <QGridLayout>
#include <QHeaderView>
#include <QLineEdit>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QShortcut>
#include <QStatusBar>
#include <QTableView>
#include <QTextEdit>
#include <QTimer>

#include "settings.h"
#include "model.h"
#include "miscellaneous.h"
#include "settingsdialog.h"
#include "application.h"

namespace QMediathekView
{

namespace
{

constexpr auto messageTimeout = 2 * 1000;
constexpr auto errorMessageTimeout = 10 * 1000;

constexpr auto searchTimeout = 200;

constexpr auto minimumChannelLength = 10;
constexpr auto minimumTopicLength = 30;

template< typename Action >
void forEachSelectedRow(const QAbstractItemView* view, Action action)
{
    const auto selectedRows = view->selectionModel()->selectedRows();

    for (const auto& index : selectedRows)
    {
        action(index);
    }
}

template< typename Getter >
void copyLinksOfSelectedRows(const QAbstractItemView* view, Getter getter)
{
    QStringList urls;

    forEachSelectedRow(view, [getter, &urls](const QModelIndex& index)
    {
        urls.append(getter(index));
    });

    QGuiApplication::clipboard()->setText(urls.join('\n'));
}

} // anonymous

MainWindow::MainWindow(Settings& settings, Model& model, Application& application, QWidget* parent)
    : QMainWindow(parent)
    , m_settings(settings)
    , m_model(model)
    , m_application(application)
{
    m_tableView = new QTableView(this);
    m_tableView->setModel(&m_model);
    setCentralWidget(m_tableView);

    m_tableView->setAlternatingRowColors(true);
    m_tableView->sortByColumn(0, Qt::AscendingOrder);
    m_tableView->setSortingEnabled(true);
    m_tableView->setTabKeyNavigation(false);
    m_tableView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableView->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_tableView->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_tableView->setContextMenuPolicy(Qt::CustomContextMenu);

    m_tableView->verticalHeader()->setVisible(false);
    m_tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_tableView->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);

    connect(m_tableView, &QTableView::doubleClicked, this, &MainWindow::doubleClicked);
    connect(m_tableView->selectionModel(), &QItemSelectionModel::currentChanged, this, &MainWindow::currentChanged);
    connect(m_tableView->horizontalHeader(), &QHeaderView::sortIndicatorChanged, this, &MainWindow::sortIndicatorChanged);
    connect(m_tableView, &QTableView::customContextMenuRequested, this, &MainWindow::customContextMenuRequested);

    const auto searchDock = new QDockWidget(tr("Search"), this);
    searchDock->setObjectName(QStringLiteral("searchDock"));
    searchDock->setFeatures(searchDock->features() & ~QDockWidget::DockWidgetClosable);
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

    m_tableView->horizontalHeader()->setMaximumSectionSize(qMax(m_channelBox->sizeHint().width(), m_topicBox->sizeHint().width()));

    connect(m_searchTimer, &QTimer::timeout, this, &MainWindow::timeout);

    constexpr auto startTimer = static_cast< void (QTimer::*)() >(&QTimer::start);
    connect(m_channelBox, &QComboBox::currentTextChanged, m_searchTimer, startTimer);
    connect(m_topicBox, &QComboBox::currentTextChanged, m_searchTimer, startTimer);
    connect(m_titleEdit, &QLineEdit::textChanged, m_searchTimer, startTimer);

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
    detailsDock->setFeatures(detailsDock->features() & ~QDockWidget::DockWidgetClosable);
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
    m_websiteLabel->setWordWrap(true);
    m_websiteLabel->setOpenExternalLinks(true);
    detailsLayout->addWidget(m_websiteLabel, 3, 1);

    m_playButton = new UrlButton(m_model, detailsWidget);
    m_playButton->setIcon(QIcon::fromTheme(QStringLiteral("media-playback-start")));
    detailsLayout->addWidget(m_playButton, 0, 0);

    connect(m_playButton, &UrlButton::clicked, this, &MainWindow::playClicked);
    connect(m_playButton, &UrlButton::defaultTriggered, this, &MainWindow::playDefaultTriggered);
    connect(m_playButton, &UrlButton::smallTriggered, this, &MainWindow::playSmallTriggered);
    connect(m_playButton, &UrlButton::largeTriggered, this, &MainWindow::playLargeTriggered);
    connect(m_tableView->selectionModel(), &QItemSelectionModel::currentChanged, m_playButton, &UrlButton::currentChanged);

    m_downloadButton = new UrlButton(m_model, detailsWidget);
    m_downloadButton->setIcon(QIcon::fromTheme(QStringLiteral("media-record")));
    detailsLayout->addWidget(m_downloadButton, 1, 0);

    connect(m_downloadButton, &UrlButton::clicked, this, &MainWindow::downloadClicked);
    connect(m_downloadButton, &UrlButton::defaultTriggered, this, &MainWindow::downloadDefaultTriggered);
    connect(m_downloadButton, &UrlButton::smallTriggered, this, &MainWindow::downloadSmallTriggered);
    connect(m_downloadButton, &UrlButton::largeTriggered, this, &MainWindow::downloadLargeTriggered);
    connect(m_tableView->selectionModel(), &QItemSelectionModel::currentChanged, m_downloadButton, &UrlButton::currentChanged);

    const auto quitShortcut = new QShortcut(QKeySequence::Quit, this);
    connect(quitShortcut, &QShortcut::activated, this, &MainWindow::close);

    restoreGeometry(m_settings.mainWindowGeometry());
    restoreState(m_settings.mainWindowState());

    setWindowTitle(qApp->applicationName() + QStringLiteral("[*]"));
    statusBar()->showMessage(tr("Ready"), messageTimeout);
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    m_settings.setMainWindowGeometry(saveGeometry());
    m_settings.setMainWindowState(saveState());

    QMainWindow::closeEvent(event);
}

void MainWindow::showStartedMirrorsUpdate()
{
    statusBar()->showMessage(tr("Started mirror list update..."), messageTimeout);
}

void MainWindow::showCompletedMirrorsUpdate()
{
    statusBar()->showMessage(tr("Successfully updated mirror list."), messageTimeout);
}

void MainWindow::showMirrorsUpdateFailure(const QString& error)
{
    statusBar()->showMessage(tr("Failed to updated mirror list: %1").arg(error), errorMessageTimeout);
}

void MainWindow::showStartedDatabaseUpdate()
{
    setWindowModified(true);
    statusBar()->showMessage(tr("Started database update..."), messageTimeout);
}

void MainWindow::showCompletedDatabaseUpdate()
{
    setWindowModified(false);
    statusBar()->showMessage(tr("Successfully updated database."), messageTimeout);
}

void MainWindow::showDatabaseUpdateFailure(const QString& error)
{
    setWindowModified(false);
    statusBar()->showMessage(tr("Failed to updated database: %1").arg(error), errorMessageTimeout);
}

void MainWindow::resetFilterPressed()
{
    m_channelBox->clearEditText();
    m_topicBox->clearEditText();
    m_titleEdit->clear();
}

void MainWindow::updateDatabasePressed()
{
    m_application.updateDatabase();
}

void MainWindow::editSettingsPressed()
{
    SettingsDialog(m_settings, this).exec();
}

void MainWindow::playClicked()
{
    m_application.playPreferred(m_tableView->currentIndex());
}

void MainWindow::playDefaultTriggered()
{
    m_application.playDefault(m_tableView->currentIndex());
}

void MainWindow::playSmallTriggered()
{
    m_application.playSmall(m_tableView->currentIndex());
}

void MainWindow::playLargeTriggered()
{
    m_application.playLarge(m_tableView->currentIndex());
}

void MainWindow::downloadClicked()
{
    forEachSelectedRow(m_tableView, [this](const QModelIndex& index)
    {
        m_application.downloadPreferred(index);
    });
}

void MainWindow::downloadDefaultTriggered()
{
    forEachSelectedRow(m_tableView, [this](const QModelIndex& index)
    {
        m_application.downloadDefault(index);
    });
}

void MainWindow::downloadSmallTriggered()
{
    forEachSelectedRow(m_tableView, [this](const QModelIndex& index)
    {
        m_application.downloadSmall(index);
    });
}

void MainWindow::downloadLargeTriggered()
{
    forEachSelectedRow(m_tableView, [this](const QModelIndex& index)
    {
        m_application.downloadLarge(index);
    });
}

void MainWindow::timeout()
{
    m_searchTimer->stop();

    const auto channel = m_channelBox->currentText();
    const auto topic = m_topicBox->currentText();
    const auto title = m_titleEdit->text();

    m_model.filter(channel, topic, title);
}

void MainWindow::doubleClicked(const QModelIndex& index)
{
    m_application.playPreferred(index);
}

void MainWindow::currentChanged(const QModelIndex& current, const QModelIndex& /* previous */)
{
    m_descriptionEdit->setPlainText(m_model.description(current));
    m_websiteLabel->setText(QStringLiteral("<a href=\"%1\">%1</a>").arg(m_model.website(current)));
}

void MainWindow::sortIndicatorChanged(int logicalIndex, Qt::SortOrder /* order */)
{
    m_tableView->horizontalHeader()->setSortIndicatorShown(logicalIndex != 2);
}

void MainWindow::customContextMenuRequested(const QPoint& pos)
{
    using std::bind;
    using std::placeholders::_1;

    const auto index = m_tableView->indexAt(pos);

    if (!index.isValid())
    {
        return;
    }

    QMenu menu;

    connect(menu.addAction(tr("&Copy link")), &QAction::triggered, [this]()
    {
        copyLinksOfSelectedRows(m_tableView, bind(&Application::preferredUrl, &m_application, _1));
    });
    connect(menu.addAction(tr("Copy &default link")), &QAction::triggered, [this]()
    {
        copyLinksOfSelectedRows(m_tableView, bind(&Model::url, &m_model, _1));
    });
    connect(menu.addAction(tr("Copy &small link")), &QAction::triggered, [this]()
    {
        copyLinksOfSelectedRows(m_tableView, bind(&Model::urlSmall, &m_model, _1));
    });
    connect(menu.addAction(tr("Copy &large link")), &QAction::triggered, [this]()
    {
        copyLinksOfSelectedRows(m_tableView, bind(&Model::urlLarge, &m_model, _1));
    });

    menu.exec(m_tableView->viewport()->mapToGlobal(pos));
}

} // QMediathekView
