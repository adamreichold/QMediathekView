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

#include "miscellaneous.h"

#include <QAction>

#include "model.h"

namespace QMediathekView
{

UrlButton::UrlButton(const Model& model, QWidget* parent)
    : QToolButton(parent)
    , m_model(model)
    , m_defaultAction(new QAction(tr("Default"), this))
    , m_smallAction(new QAction(tr("Small"), this))
    , m_largeAction(new QAction(tr("Large"), this))
{
    addActions(QList< QAction* >() << m_defaultAction << m_smallAction << m_largeAction);
    setPopupMode(QToolButton::DelayedPopup);
    setEnabled(false);

    connect(m_defaultAction, &QAction::triggered, this, &UrlButton::defaultTriggered);
    connect(m_smallAction, &QAction::triggered, this, &UrlButton::smallTriggered);
    connect(m_largeAction, &QAction::triggered, this, &UrlButton::largeTriggered);
}

void UrlButton::currentChanged(const QModelIndex& current, const QModelIndex& /* previous */)
{
    setEnabled(current.isValid());

    m_defaultAction->setDisabled(m_model.url(current).isEmpty());
    m_smallAction->setDisabled(m_model.urlSmall(current).isEmpty());
    m_largeAction->setDisabled(m_model.urlLarge(current).isEmpty());
}

} // QMediathekView
