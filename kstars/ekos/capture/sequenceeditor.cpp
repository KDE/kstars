/*
    SPDX-FileCopyrightText: 2024 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "sequenceeditor.h"

#include "capture.h"
#include <kstars_debug.h>

namespace Ekos
{

SequenceEditor::SequenceEditor(QWidget * parent) : QDialog(parent)
{
#ifdef Q_OS_OSX
    setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
#endif
    setupUi(this);

    m_capture.reset(new Capture(true));
    sequenceEditorLayout->insertWidget(0, m_capture.get());
}
void SequenceEditor::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    m_capture->onStandAloneShow(event);
}
}
