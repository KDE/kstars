/*
    SPDX-FileCopyrightText: 2003 Jason Harris <kstars@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "scriptargwidgets.h"
#include "ui_scriptbuilder.h"
#include "ui_scriptnamedialog.h"
#include "ui_optionstreeview.h"

#include <QDialog>
#include <QUrl>

#include <memory>

class QTreeWidget;
class QTreeWidgetItem;
class QTextStream;

class ScriptFunction;

class OptionsTreeViewWidget : public QFrame, public Ui::OptionsTreeView
{
        Q_OBJECT
    public:
        explicit OptionsTreeViewWidget(QWidget *p);
};

class OptionsTreeView : public QDialog
{
        Q_OBJECT
    public:
        explicit OptionsTreeView(QWidget *p);
        virtual ~OptionsTreeView() override = default;

        QTreeWidget *optionsList()
        {
            return otvw->OptionsList;
        }
        void resizeColumns();

    private:
        std::unique_ptr<OptionsTreeViewWidget> otvw;
};

class ScriptNameWidget : public QFrame, public Ui::ScriptNameDialog
{
        Q_OBJECT
    public:
        explicit ScriptNameWidget(QWidget *p);
};

class ScriptNameDialog : public QDialog
{
        Q_OBJECT
    public:
        explicit ScriptNameDialog(QWidget *p);
        ~ScriptNameDialog() override;
        QString scriptName() const
        {
            return snw->ScriptName->text();
        }
        QString authorName() const
        {
            return snw->AuthorName->text();
        }

    private slots:
        void slotEnableOkButton();

    private:
        ScriptNameWidget *snw { nullptr };
        QPushButton *okB { nullptr };
};

class ScriptBuilderUI : public QFrame, public Ui::ScriptBuilder
{
        Q_OBJECT
    public:
        explicit ScriptBuilderUI(QWidget *p);
};

/**
 * @class ScriptBuilder
 * A GUI tool for building behavioral DBus scripts for KStars.
 *
 * @author Jason Harris
 * @version 1.0
 */
class ScriptBuilder : public QDialog
{
        Q_OBJECT
    public:
        explicit ScriptBuilder(QWidget *parent);
        ~ScriptBuilder() override;

        bool unsavedChanges() const
        {
            return UnsavedChanges;
        }
        void setUnsavedChanges(bool b = true);
        void saveWarning();
        void readScript(QTextStream &istream);
        void writeScript(QTextStream &ostream);
        bool parseFunction(const QString &fn_name, const QString &fn_args);

    public slots:
        void slotAddFunction();
        void slotMoveFunctionUp();
        void slotMoveFunctionDown();
        void slotArgWidget();
        void slotShowDoc();

        void slotNew();
        void slotOpen();
        void slotSave();
        void slotSaveAs();
        void slotRunScript();
        void slotClose();

        void slotCopyFunction();
        void slotRemoveFunction();

        void slotFindCity();
        void slotFindObject();
        void slotShowOptions();
        void slotLookToward();
        void slotArgFindObject();
        void slotRa();
        void slotDec();
        void slotAz();
        void slotAlt();
        void slotChangeDate();
        void slotChangeTime();
        void slotWaitFor();
        void slotWaitForKey();
        void slotTracking();
        void slotViewOption();
        void slotChangeCity();
        void slotChangeProvince();
        void slotChangeCountry();
        void slotTimeScale();
        void slotZoom();
        void slotExportImage();
        void slotPrintImage();
        void slotChangeColor();
        void slotChangeColorName();
        void slotLoadColorScheme();

#if 0
        void slotINDIWaitCheck(bool toggleState);
        void slotINDIFindObject();
        void slotINDIStartDeviceName();
        void slotINDIStartDeviceMode();
        void slotINDISetDevice();
        void slotINDIShutdown();
        void slotINDISwitchDeviceConnection();
        void slotINDISetPortDevicePort();
        void slotINDISetTargetCoordDeviceRA();
        void slotINDISetTargetCoordDeviceDEC();
        void slotINDISetTargetNameTargetName();
        void slotINDISetActionName();
        void slotINDIWaitForActionName();
        void slotINDISetFocusSpeed();
        void slotINDIStartFocusDirection();
        void slotINDISetFocusTimeout();
        void slotINDISetGeoLocationDeviceLong();
        void slotINDISetGeoLocationDeviceLat();
        void slotINDIStartExposureTimeout();
        void slotINDISetUTC();
        void slotINDISetScopeAction();
        void slotINDISetFrameType();
        void slotINDISetCCDTemp();
        void slotINDISetFilterNum();
#endif

    private:
        void initViewOptions();
        void warningMismatch(const QString &expected) const;

        ScriptBuilderUI *sb { nullptr };

        QList<ScriptFunction *> KStarsFunctionList;
        QList<ScriptFunction *> SimClockFunctionList;

#if 0
        QList<ScriptFunction *> INDIFunctionList;
#endif

        QList<ScriptFunction *> ScriptList;

        QWidget *argBlank { nullptr };
        ArgLookToward *argLookToward { nullptr };
        ArgFindObject *argFindObject { nullptr };
        ArgSetRaDec *argSetRaDec { nullptr };
        ArgSetAltAz *argSetAltAz { nullptr };
        ArgSetLocalTime *argSetLocalTime { nullptr };
        ArgWaitFor *argWaitFor { nullptr };
        ArgWaitForKey *argWaitForKey { nullptr };
        ArgSetTrack *argSetTracking { nullptr };
        ArgChangeViewOption *argChangeViewOption { nullptr };
        ArgSetGeoLocation *argSetGeoLocation { nullptr };
        ArgTimeScale *argTimeScale { nullptr };
        ArgZoom *argZoom { nullptr };
        ArgExportImage *argExportImage { nullptr };
        ArgPrintImage *argPrintImage { nullptr };
        ArgSetColor *argSetColor { nullptr };
        ArgLoadColorScheme *argLoadColorScheme { nullptr };

#if 0
        ArgStartINDI * argStartINDI;
        ArgSetDeviceINDI * argSetDeviceINDI;
        ArgShutdownINDI * argShutdownINDI;
        ArgSwitchINDI * argSwitchINDI;
        ArgSetPortINDI * argSetPortINDI;
        ArgSetTargetCoordINDI * argSetTargetCoordINDI;
        ArgSetTargetNameINDI * argSetTargetNameINDI;
        ArgSetActionINDI * argSetActionINDI;
        ArgSetActionINDI * argWaitForActionINDI;
        ArgSetFocusSpeedINDI * argSetFocusSpeedINDI;
        ArgStartFocusINDI * argStartFocusINDI;
        ArgSetFocusTimeoutINDI * argSetFocusTimeoutINDI;
        ArgSetGeoLocationINDI * argSetGeoLocationINDI;
        ArgStartExposureINDI * argStartExposureINDI;
        ArgSetUTCINDI * argSetUTCINDI;
        ArgSetScopeActionINDI * argSetScopeActionINDI;
        ArgSetFrameTypeINDI * argSetFrameTypeINDI;
        ArgSetCCDTempINDI * argSetCCDTempINDI;
        ArgSetFilterNumINDI * argSetFilterNumINDI;
#endif

        ScriptNameDialog *snd { nullptr };
        OptionsTreeView *otv { nullptr };

        QTreeWidgetItem *opsGUI { nullptr };
        QTreeWidgetItem *opsToolbar { nullptr };
        QTreeWidgetItem *opsShowObj { nullptr };
        QTreeWidgetItem *opsShowOther { nullptr };
        QTreeWidgetItem *opsCName { nullptr };
        QTreeWidgetItem *opsHide { nullptr };
        QTreeWidgetItem *opsSkymap { nullptr };
        QTreeWidgetItem *opsLimit { nullptr };

        bool UnsavedChanges { false };
        bool checkForChanges { false };
        QUrl currentFileURL;
        QString currentDir;
        QString currentScriptName, currentAuthor;
};
