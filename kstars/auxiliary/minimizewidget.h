/*
    SPDX-FileCopyrightText: 2025 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QWidget>
#include <QPushButton>
#include <QVBoxLayout>

/**
 * @brief MinimizeWidget provides functionality to have a minimal and full view of
 * some UI and toggle between them.
 *
 * @author Hy Murveit
 */

/*
 * Use this widget to switch between a full view and a minimized view of some controls
 * It automatically puts a little minimize/maximize button to the left, and alternates
 * between two child widgets. The first is the maximized UI, and the 2nd widget is the
 * minimized UI. It is your job to make sure this widget has two children.
 * Note that QtCreator doesn't display this well, unless the MinimizeWidget has an
 * (ignored) QVBoxLayout you won't see your two child widgets in QtCreator.
 * In a .ui file this might look like this:
 *
 *      <item>
 *       <widget class="MinimizeWidget" name="yourMinimizeWidget">
 *        <layout class="QVBoxLayout" name="unusedVerticalLayout">
 *         <item>
 *          <widget class="QFrame" name="MaximizedUI">
 *           ...
 *          </widget>
 *         </item>
 *         <item>
 *          <widget class="QFrame" name="MinimizedUI">
 *          ...
 *          </widget>
 *         </item>
 *        </layout>
 *       </widget>
 *      </item>
 *
 * If you make your widgets QWidgets, QtCreator tends to remove those, and that can be problematic,
 * so I'd recommend you use QFrames, and if you don't like the borders, go with
 *   <widget class="QFrame" name="altMaxFrame">
 *    <property name="frameShape">
 *     <enum>QFrame::Shape::NoFrame</enum>
 *    </property>
 * and perhaps set the various borders to 0 or a small number.
 * You must explicitly call setupUI(...) after the parent UI is setup (so it can see it's children.
 * You can get it to remember its minimized/maximized values using options, e.g.
 *
 *  ui->myMinimizeWidget->setupUI(Options::myUIisMinimized(), &Options::setMyUIisMinimized);
 */

class MinimizeWidget : public QWidget
{
        Q_OBJECT

        Q_PROPERTY(bool minimized READ isMinimized WRITE setMinimized NOTIFY changed)

    public:
        MinimizeWidget(QWidget *parent = nullptr);

        // Must be explicitly called
        // Can add an initial value and a function that sets the option value
        // when minimized/maximized by the user.
        void setupUI(bool initiallyMinimized = false, void(*setOption)(bool) = nullptr);

        bool isMinimized() const
        {
            return m_isMinimized;
        }
        void setMinimized(bool minimized);

    signals:
        void changed(bool minimized);

    protected:

    private slots:
        void maximize();
        void minimize();

    private:
        QPushButton *m_MaximizeButton { nullptr };
        QPushButton *m_MinimizeButton { nullptr };
        QVBoxLayout *m_mainLayout { nullptr };
        QWidget *m_MinimizedWidget { nullptr };
        QWidget *m_MaximizedWidget { nullptr };

        bool m_initialized { false };
        bool m_isMinimized { true };
};
