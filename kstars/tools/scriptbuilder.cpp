/*
    SPDX-FileCopyrightText: 2003 Jason Harris <kstars@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "scriptbuilder.h"

#include "kspaths.h"
#include "scriptfunction.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "skymap.h"
#include "ksnotification.h"
#include "kstarsdatetime.h"
#include "dialogs/finddialog.h"
#include "dialogs/locationdialog.h"
#include "widgets/dmsbox.h"
#include "widgets/timespinbox.h"
#include "widgets/timestepbox.h"

#include <KLocalizedString>
#include <KMessageBox>
#include <KIO/StoredTransferJob>
#include <KIO/CopyJob>
#include <KJob>

#include <QApplication>
#include <QFontMetrics>
#include <QTreeWidget>
#include <QTextStream>
#include <QFileDialog>
#include <QStandardPaths>
#include <QDebug>

#include <sys/stat.h>

OptionsTreeViewWidget::OptionsTreeViewWidget(QWidget *p) : QFrame(p)
{
    setupUi(this);
}

OptionsTreeView::OptionsTreeView(QWidget *p) : QDialog(p)
{
    otvw.reset(new OptionsTreeViewWidget(this));

    QVBoxLayout *mainLayout = new QVBoxLayout;

    mainLayout->addWidget(otvw.get());
    setLayout(mainLayout);

    setWindowTitle(i18nc("@title:window", "Options"));

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    mainLayout->addWidget(buttonBox);
    connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

    setModal(false);
}

void OptionsTreeView::resizeColumns()
{
    //Size each column to the maximum width of items in that column
    int maxwidth[3]  = { 0, 0, 0 };
    QFontMetrics qfm = optionsList()->fontMetrics();

    for (int i = 0; i < optionsList()->topLevelItemCount(); ++i)
    {
        QTreeWidgetItem *topitem = optionsList()->topLevelItem(i);
        topitem->setExpanded(true);

        for (int j = 0; j < topitem->childCount(); ++j)
        {
            QTreeWidgetItem *child = topitem->child(j);

            for (int icol = 0; icol < 3; ++icol)
            {
                child->setExpanded(true);

#if QT_VERSION >= QT_VERSION_CHECK(5,11,0)
                int w = qfm.horizontalAdvance(child->text(icol)) + 4;
#else
                int w = qfm.width(child->text(icol)) + 4;
#endif

                if (icol == 0)
                {
                    w += 2 * optionsList()->indentation();
                }

                if (w > maxwidth[icol])
                {
                    maxwidth[icol] = w;
                }
            }
        }
    }

    for (int icol = 0; icol < 3; ++icol)
    {
        //DEBUG
        qDebug() << QString("max width of column %1: %2").arg(icol).arg(maxwidth[icol]);

        optionsList()->setColumnWidth(icol, maxwidth[icol]);
    }
}

ScriptNameWidget::ScriptNameWidget(QWidget *p) : QFrame(p)
{
    setupUi(this);
}

ScriptNameDialog::ScriptNameDialog(QWidget *p) : QDialog(p)
{
#ifdef Q_OS_OSX
    setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
#endif
    snw = new ScriptNameWidget(this);

    QVBoxLayout *mainLayout = new QVBoxLayout;

    mainLayout->addWidget(snw);
    setLayout(mainLayout);

    setWindowTitle(i18nc("@title:window", "Script Data"));

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    mainLayout->addWidget(buttonBox);
    connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

    okB = buttonBox->button(QDialogButtonBox::Ok);

    connect(snw->ScriptName, SIGNAL(textChanged(QString)), this, SLOT(slotEnableOkButton()));
}

ScriptNameDialog::~ScriptNameDialog()
{
    delete snw;
}

void ScriptNameDialog::slotEnableOkButton()
{
    okB->setEnabled(!snw->ScriptName->text().isEmpty());
}

ScriptBuilderUI::ScriptBuilderUI(QWidget *p) : QFrame(p)
{
    setupUi(this);
}

ScriptBuilder::ScriptBuilder(QWidget *parent)
    : QDialog(parent), UnsavedChanges(false), checkForChanges(true), currentFileURL(), currentDir(QDir::homePath()),
      currentScriptName(), currentAuthor()
{
#ifdef Q_OS_OSX
    setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
#endif
    sb = new ScriptBuilderUI(this);

    QVBoxLayout *mainLayout = new QVBoxLayout;

    mainLayout->addWidget(sb);
    setLayout(mainLayout);

    setWindowTitle(i18nc("@title:window", "Script Builder"));

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close);
    mainLayout->addWidget(buttonBox);
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(slotClose()));

    sb->FuncDoc->setTextInteractionFlags(Qt::NoTextInteraction);

    //Initialize function templates and descriptions
    KStarsFunctionList.append(new ScriptFunction("lookTowards",
                              i18n("Point the display at the specified location. %1 can be the name "
                                   "of an object, a cardinal point on the compass, or 'zenith'.",
                                   QString("dir")),
                              false, "QString", "dir"));
    KStarsFunctionList.append(new ScriptFunction(
                                  "addLabel", i18n("Add a name label to the object named %1.", QString("name")), false, "QString", "name"));
    KStarsFunctionList.append(
        new ScriptFunction("removeLabel", i18n("Remove the name label from the object named %1.", QString("name")),
                           false, "QString", "name"));
    KStarsFunctionList.append(new ScriptFunction(
                                  "addTrail", i18n("Add a trail to the solar system body named %1.", QString("name")), false, "QString", "name"));
    KStarsFunctionList.append(new ScriptFunction(
                                  "removeTrail", i18n("Remove the trail from the solar system body named %1.", QString("name")), false, "QString",
                                  "name"));
    KStarsFunctionList.append(new ScriptFunction("setRaDec",
                              i18n("Point the display at the specified RA/Dec coordinates.  RA is "
                                   "expressed in Hours; Dec is expressed in Degrees."),
                              false, "double", "ra", "double", "dec"));
    KStarsFunctionList.append(new ScriptFunction(
                                  "setAltAz",
                                  i18n("Point the display at the specified Alt/Az coordinates.  Alt and Az are expressed in Degrees."), false,
                                  "double", "alt", "double", "az"));
    KStarsFunctionList.append(new ScriptFunction("zoomIn", i18n("Increase the display Zoom Level."), false));
    KStarsFunctionList.append(new ScriptFunction("zoomOut", i18n("Decrease the display Zoom Level."), false));
    KStarsFunctionList.append(
        new ScriptFunction("defaultZoom", i18n("Set the display Zoom Level to its default value."), false));
    KStarsFunctionList.append(
        new ScriptFunction("zoom", i18n("Set the display Zoom Level manually."), false, "double", "z"));
    KStarsFunctionList.append(
        new ScriptFunction("setLocalTime", i18n("Set the system clock to the specified Local Time."), false, "int",
                           "year", "int", "month", "int", "day", "int", "hour", "int", "minute", "int", "second"));
    KStarsFunctionList.append(new ScriptFunction(
                                  "waitFor", i18n("Pause script execution for specified number of seconds."), false, "double", "sec"));
    KStarsFunctionList.append(new ScriptFunction("waitForKey",
                              i18n("Halt script execution until the specified key is pressed.  Only "
                                   "single-key strokes are possible; use 'space' for the spacebar."),
                              false, "QString", "key"));
    KStarsFunctionList.append(new ScriptFunction(
                                  "setTracking", i18n("Set whether the display is tracking the current location."), false, "bool", "track"));
    KStarsFunctionList.append(new ScriptFunction(
                                  "changeViewOption", i18n("Change view option named %1 to value %2.", QString("opName"), QString("opValue")),
                                  false, "QString", "opName", "QString", "opValue"));
    KStarsFunctionList.append(new ScriptFunction(
                                  "setGeoLocation", i18n("Set the geographic location to the city specified by city, province and country."),
                                  false, "QString", "cityName", "QString", "provinceName", "QString", "countryName"));
    KStarsFunctionList.append(new ScriptFunction(
                                  "setColor", i18n("Set the color named %1 to the value %2.", QString("colorName"), QString("value")), false,
                                  "QString", "colorName", "QString", "value"));
    KStarsFunctionList.append(new ScriptFunction("loadColorScheme", i18n("Load the color scheme specified by name."),
                              false, "QString", "name"));
    KStarsFunctionList.append(
        new ScriptFunction("exportImage", i18n("Export the sky image to the file, with specified width and height."),
                           false, "QString", "fileName", "int", "width", "int", "height"));
    KStarsFunctionList.append(
        new ScriptFunction("printImage",
                           i18n("Print the sky image to a printer or file.  If %1 is true, it will show the print "
                                "dialog.  If %2 is true, it will use the Star Chart color scheme for printing.",
                                QString("usePrintDialog"), QString("useChartColors")),
                           false, "bool", "usePrintDialog", "bool", "useChartColors"));
    SimClockFunctionList.append(new ScriptFunction("stop", i18n("Halt the simulation clock."), true));
    SimClockFunctionList.append(new ScriptFunction("start", i18n("Start the simulation clock."), true));
    SimClockFunctionList.append(new ScriptFunction("setClockScale",
                                i18n("Set the timescale of the simulation clock to specified scale. "
                                     " 1.0 means real-time; 2.0 means twice real-time; etc."),
                                true, "double", "scale"));

    // JM: We're using QTreeWdiget for Qt4 now
    QTreeWidgetItem *kstars_tree   = new QTreeWidgetItem(sb->FunctionTree, QStringList("KStars"));
    QTreeWidgetItem *simclock_tree = new QTreeWidgetItem(sb->FunctionTree, QStringList("SimClock"));

    for (auto &item : KStarsFunctionList)
        new QTreeWidgetItem(kstars_tree, QStringList(item->prototype()));

    for (auto &item : SimClockFunctionList)
        new QTreeWidgetItem(simclock_tree, QStringList(item->prototype()));

    kstars_tree->sortChildren(0, Qt::AscendingOrder);
    simclock_tree->sortChildren(0, Qt::AscendingOrder);

    sb->FunctionTree->setColumnCount(1);
    sb->FunctionTree->setHeaderLabels(QStringList(i18n("Functions")));
    sb->FunctionTree->setSortingEnabled(false);

    //Add icons to Push Buttons
    sb->NewButton->setIcon(QIcon::fromTheme("document-new"));
    sb->OpenButton->setIcon(QIcon::fromTheme("document-open"));
    sb->SaveButton->setIcon(QIcon::fromTheme("document-save"));
    sb->SaveAsButton->setIcon(
        QIcon::fromTheme("document-save-as"));
    sb->RunButton->setIcon(QIcon::fromTheme("system-run"));
    sb->CopyButton->setIcon(QIcon::fromTheme("view-refresh"));
    sb->AddButton->setIcon(QIcon::fromTheme("go-previous"));
    sb->RemoveButton->setIcon(QIcon::fromTheme("go-next"));
    sb->UpButton->setIcon(QIcon::fromTheme("go-up"));
    sb->DownButton->setIcon(QIcon::fromTheme("go-down"));

    sb->NewButton->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    sb->OpenButton->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    sb->SaveButton->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    sb->SaveAsButton->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    sb->RunButton->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    sb->CopyButton->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    sb->AddButton->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    sb->RemoveButton->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    sb->UpButton->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    sb->DownButton->setAttribute(Qt::WA_LayoutUsesWidgetRect);

    //Prepare the widget stack
    argBlank            = new QWidget();
    argLookToward       = new ArgLookToward(sb->ArgStack);
    argFindObject       = new ArgFindObject(sb->ArgStack); //shared by Add/RemoveLabel and Add/RemoveTrail
    argSetRaDec         = new ArgSetRaDec(sb->ArgStack);
    argSetAltAz         = new ArgSetAltAz(sb->ArgStack);
    argSetLocalTime     = new ArgSetLocalTime(sb->ArgStack);
    argWaitFor          = new ArgWaitFor(sb->ArgStack);
    argWaitForKey       = new ArgWaitForKey(sb->ArgStack);
    argSetTracking      = new ArgSetTrack(sb->ArgStack);
    argChangeViewOption = new ArgChangeViewOption(sb->ArgStack);
    argSetGeoLocation   = new ArgSetGeoLocation(sb->ArgStack);
    argTimeScale        = new ArgTimeScale(sb->ArgStack);
    argZoom             = new ArgZoom(sb->ArgStack);
    argExportImage      = new ArgExportImage(sb->ArgStack);
    argPrintImage       = new ArgPrintImage(sb->ArgStack);
    argSetColor         = new ArgSetColor(sb->ArgStack);
    argLoadColorScheme  = new ArgLoadColorScheme(sb->ArgStack);

    sb->ArgStack->addWidget(argBlank);
    sb->ArgStack->addWidget(argLookToward);
    sb->ArgStack->addWidget(argFindObject);
    sb->ArgStack->addWidget(argSetRaDec);
    sb->ArgStack->addWidget(argSetAltAz);
    sb->ArgStack->addWidget(argSetLocalTime);
    sb->ArgStack->addWidget(argWaitFor);
    sb->ArgStack->addWidget(argWaitForKey);
    sb->ArgStack->addWidget(argSetTracking);
    sb->ArgStack->addWidget(argChangeViewOption);
    sb->ArgStack->addWidget(argSetGeoLocation);
    sb->ArgStack->addWidget(argTimeScale);
    sb->ArgStack->addWidget(argZoom);
    sb->ArgStack->addWidget(argExportImage);
    sb->ArgStack->addWidget(argPrintImage);
    sb->ArgStack->addWidget(argSetColor);
    sb->ArgStack->addWidget(argLoadColorScheme);

    sb->ArgStack->setCurrentIndex(0);

    snd = new ScriptNameDialog(KStars::Instance());
    otv = new OptionsTreeView(KStars::Instance());

    otv->resize(sb->width(), 0.5 * sb->height());

    initViewOptions();
    otv->resizeColumns();

    //connect widgets in ScriptBuilderUI
    connect(sb->FunctionTree, SIGNAL(itemDoubleClicked(QTreeWidgetItem*, int)), this, SLOT(slotAddFunction()));
    connect(sb->FunctionTree, SIGNAL(itemClicked(QTreeWidgetItem*, int)), this, SLOT(slotShowDoc()));
    connect(sb->UpButton, SIGNAL(clicked()), this, SLOT(slotMoveFunctionUp()));
    connect(sb->ScriptListBox, SIGNAL(itemClicked(QListWidgetItem*)), this, SLOT(slotArgWidget()));
    connect(sb->DownButton, SIGNAL(clicked()), this, SLOT(slotMoveFunctionDown()));
    connect(sb->CopyButton, SIGNAL(clicked()), this, SLOT(slotCopyFunction()));
    connect(sb->RemoveButton, SIGNAL(clicked()), this, SLOT(slotRemoveFunction()));
    connect(sb->NewButton, SIGNAL(clicked()), this, SLOT(slotNew()));
    connect(sb->OpenButton, SIGNAL(clicked()), this, SLOT(slotOpen()));
    connect(sb->SaveButton, SIGNAL(clicked()), this, SLOT(slotSave()));
    connect(sb->SaveAsButton, SIGNAL(clicked()), this, SLOT(slotSaveAs()));
    connect(sb->AddButton, SIGNAL(clicked()), this, SLOT(slotAddFunction()));
    connect(sb->RunButton, SIGNAL(clicked()), this, SLOT(slotRunScript()));

    //Connections for Arg Widgets
    connect(argSetGeoLocation->FindCityButton, SIGNAL(clicked()), this, SLOT(slotFindCity()));
    connect(argLookToward->FindButton, SIGNAL(clicked()), this, SLOT(slotFindObject()));
    connect(argChangeViewOption->TreeButton, SIGNAL(clicked()), this, SLOT(slotShowOptions()));
    connect(argFindObject->FindButton, SIGNAL(clicked()), this, SLOT(slotFindObject()));

    connect(argLookToward->FocusEdit, SIGNAL(editTextChanged(QString)), this, SLOT(slotLookToward()));
    connect(argFindObject->NameEdit, SIGNAL(textChanged(QString)), this, SLOT(slotArgFindObject()));
    connect(argSetRaDec->RABox, SIGNAL(textChanged(QString)), this, SLOT(slotRa()));
    connect(argSetRaDec->DecBox, SIGNAL(textChanged(QString)), this, SLOT(slotDec()));
    connect(argSetAltAz->AltBox, SIGNAL(textChanged(QString)), this, SLOT(slotAlt()));
    connect(argSetAltAz->AzBox, SIGNAL(textChanged(QString)), this, SLOT(slotAz()));
    connect(argSetLocalTime->DateWidget, SIGNAL(dateChanged(QDate)), this, SLOT(slotChangeDate()));
    connect(argSetLocalTime->TimeBox, SIGNAL(timeChanged(QTime)), this, SLOT(slotChangeTime()));
    connect(argWaitFor->DelayBox, SIGNAL(valueChanged(int)), this, SLOT(slotWaitFor()));
    connect(argWaitForKey->WaitKeyEdit, SIGNAL(textChanged(QString)), this, SLOT(slotWaitForKey()));
    connect(argSetTracking->CheckTrack, SIGNAL(stateChanged(int)), this, SLOT(slotTracking()));
    connect(argChangeViewOption->OptionName, SIGNAL(activated(QString)), this, SLOT(slotViewOption()));
    connect(argChangeViewOption->OptionValue, SIGNAL(textChanged(QString)), this, SLOT(slotViewOption()));
    connect(argSetGeoLocation->CityName, SIGNAL(textChanged(QString)), this, SLOT(slotChangeCity()));
    connect(argSetGeoLocation->ProvinceName, SIGNAL(textChanged(QString)), this, SLOT(slotChangeProvince()));
    connect(argSetGeoLocation->CountryName, SIGNAL(textChanged(QString)), this, SLOT(slotChangeCountry()));
    connect(argTimeScale->TimeScale, SIGNAL(scaleChanged(float)), this, SLOT(slotTimeScale()));
    connect(argZoom->ZoomBox, SIGNAL(textChanged(QString)), this, SLOT(slotZoom()));
    connect(argExportImage->ExportFileName, SIGNAL(textChanged(QString)), this, SLOT(slotExportImage()));
    connect(argExportImage->ExportWidth, SIGNAL(valueChanged(int)), this, SLOT(slotExportImage()));
    connect(argExportImage->ExportHeight, SIGNAL(valueChanged(int)), this, SLOT(slotExportImage()));
    connect(argPrintImage->UsePrintDialog, SIGNAL(toggled(bool)), this, SLOT(slotPrintImage()));
    connect(argPrintImage->UseChartColors, SIGNAL(toggled(bool)), this, SLOT(slotPrintImage()));
    connect(argSetColor->ColorName, SIGNAL(activated(QString)), this, SLOT(slotChangeColorName()));
    connect(argSetColor->ColorValue, SIGNAL(changed(QColor)), this, SLOT(slotChangeColor()));
    connect(argLoadColorScheme->SchemeList, SIGNAL(itemClicked(QListWidgetItem*)), this, SLOT(slotLoadColorScheme()));

    //disable some buttons
    sb->CopyButton->setEnabled(false);
    sb->AddButton->setEnabled(false);
    sb->RemoveButton->setEnabled(false);
    sb->UpButton->setEnabled(false);
    sb->DownButton->setEnabled(false);
    sb->SaveButton->setEnabled(false);
    sb->SaveAsButton->setEnabled(false);
    sb->RunButton->setEnabled(false);
}

ScriptBuilder::~ScriptBuilder()
{
    while (!KStarsFunctionList.isEmpty())
        delete KStarsFunctionList.takeFirst();

    while (!SimClockFunctionList.isEmpty())
        delete SimClockFunctionList.takeFirst();

    while (!ScriptList.isEmpty())
        delete ScriptList.takeFirst();
}

void ScriptBuilder::initViewOptions()
{
    otv->optionsList()->setRootIsDecorated(true);
    QStringList fields;

    //InfoBoxes
    opsGUI = new QTreeWidgetItem(otv->optionsList(), QStringList(i18n("InfoBoxes")));
    fields << "ShowInfoBoxes" << i18n("Toggle display of all InfoBoxes") << i18n("bool");
    new QTreeWidgetItem(opsGUI, fields);
    fields.clear();
    fields << "ShowTimeBox" << i18n("Toggle display of Time InfoBox") << i18n("bool");
    new QTreeWidgetItem(opsGUI, fields);
    fields.clear();
    fields << "ShowGeoBox" << i18n("Toggle display of Geographic InfoBox") << i18n("bool");
    new QTreeWidgetItem(opsGUI, fields);
    fields.clear();
    fields << "ShowFocusBox" << i18n("Toggle display of Focus InfoBox") << i18n("bool");
    new QTreeWidgetItem(opsGUI, fields);
    fields.clear();
    fields << "ShadeTimeBox" << i18n("(un)Shade Time InfoBox") << i18n("bool");
    new QTreeWidgetItem(opsGUI, fields);
    fields.clear();
    fields << "ShadeGeoBox" << i18n("(un)Shade Geographic InfoBox") << i18n("bool");
    new QTreeWidgetItem(opsGUI, fields);
    fields.clear();
    fields << "ShadeFocusBox" << i18n("(un)Shade Focus InfoBox") << i18n("bool");
    new QTreeWidgetItem(opsGUI, fields);
    fields.clear();

    argChangeViewOption->OptionName->addItem("ShowInfoBoxes");
    argChangeViewOption->OptionName->addItem("ShowTimeBox");
    argChangeViewOption->OptionName->addItem("ShowGeoBox");
    argChangeViewOption->OptionName->addItem("ShowFocusBox");
    argChangeViewOption->OptionName->addItem("ShadeTimeBox");
    argChangeViewOption->OptionName->addItem("ShadeGeoBox");
    argChangeViewOption->OptionName->addItem("ShadeFocusBox");

    //Toolbars
    opsToolbar = new QTreeWidgetItem(otv->optionsList(), QStringList(i18n("Toolbars")));
    fields << "ShowMainToolBar" << i18n("Toggle display of main toolbar") << i18n("bool");
    new QTreeWidgetItem(opsToolbar, fields);
    fields.clear();
    fields << "ShowViewToolBar" << i18n("Toggle display of view toolbar") << i18n("bool");
    new QTreeWidgetItem(opsToolbar, fields);
    fields.clear();

    argChangeViewOption->OptionName->addItem("ShowMainToolBar");
    argChangeViewOption->OptionName->addItem("ShowViewToolBar");

    //Show Objects
    opsShowObj = new QTreeWidgetItem(otv->optionsList(), QStringList(i18n("Show Objects")));
    fields << "ShowStars" << i18n("Toggle display of Stars") << i18n("bool");
    new QTreeWidgetItem(opsShowObj, fields);
    fields.clear();
    fields << "ShowDeepSky" << i18n("Toggle display of all deep-sky objects") << i18n("bool");
    new QTreeWidgetItem(opsShowObj, fields);
    fields.clear();
    new QTreeWidgetItem(opsShowObj, fields);
    fields.clear();
    new QTreeWidgetItem(opsShowObj, fields);
    fields.clear();
    fields << "ShowSolarSystem" << i18n("Toggle display of all solar system bodies") << i18n("bool");
    new QTreeWidgetItem(opsShowObj, fields);
    fields.clear();
    fields << "ShowSun" << i18n("Toggle display of Sun") << i18n("bool");
    new QTreeWidgetItem(opsShowObj, fields);
    fields.clear();
    fields << "ShowMoon" << i18n("Toggle display of Moon") << i18n("bool");
    new QTreeWidgetItem(opsShowObj, fields);
    fields.clear();
    fields << "ShowMercury" << i18n("Toggle display of Mercury") << i18n("bool");
    new QTreeWidgetItem(opsShowObj, fields);
    fields.clear();
    fields << "ShowVenus" << i18n("Toggle display of Venus") << i18n("bool");
    new QTreeWidgetItem(opsShowObj, fields);
    fields.clear();
    fields << "ShowMars" << i18n("Toggle display of Mars") << i18n("bool");
    new QTreeWidgetItem(opsShowObj, fields);
    fields.clear();
    fields << "ShowJupiter" << i18n("Toggle display of Jupiter") << i18n("bool");
    new QTreeWidgetItem(opsShowObj, fields);
    fields.clear();
    fields << "ShowSaturn" << i18n("Toggle display of Saturn") << i18n("bool");
    new QTreeWidgetItem(opsShowObj, fields);
    fields.clear();
    fields << "ShowUranus" << i18n("Toggle display of Uranus") << i18n("bool");
    new QTreeWidgetItem(opsShowObj, fields);
    fields.clear();
    fields << "ShowNeptune" << i18n("Toggle display of Neptune") << i18n("bool");
    new QTreeWidgetItem(opsShowObj, fields);
    //fields.clear();
    //fields << "ShowPluto" << i18n( "Toggle display of Pluto" ) << i18n( "bool" );
    //new QTreeWidgetItem( opsShowObj, fields );
    fields.clear();
    fields << "ShowAsteroids" << i18n("Toggle display of Asteroids") << i18n("bool");
    new QTreeWidgetItem(opsShowObj, fields);
    fields.clear();
    fields << "ShowComets" << i18n("Toggle display of Comets") << i18n("bool");
    new QTreeWidgetItem(opsShowObj, fields);
    fields.clear();

    argChangeViewOption->OptionName->addItem("ShowStars");
    argChangeViewOption->OptionName->addItem("ShowDeepSky");
    argChangeViewOption->OptionName->addItem("ShowSolarSystem");
    argChangeViewOption->OptionName->addItem("ShowSun");
    argChangeViewOption->OptionName->addItem("ShowMoon");
    argChangeViewOption->OptionName->addItem("ShowMercury");
    argChangeViewOption->OptionName->addItem("ShowVenus");
    argChangeViewOption->OptionName->addItem("ShowMars");
    argChangeViewOption->OptionName->addItem("ShowJupiter");
    argChangeViewOption->OptionName->addItem("ShowSaturn");
    argChangeViewOption->OptionName->addItem("ShowUranus");
    argChangeViewOption->OptionName->addItem("ShowNeptune");
    //argChangeViewOption->OptionName->addItem( "ShowPluto" );
    argChangeViewOption->OptionName->addItem("ShowAsteroids");
    argChangeViewOption->OptionName->addItem("ShowComets");

    opsShowOther = new QTreeWidgetItem(otv->optionsList(), QStringList(i18n("Show Other")));
    fields << "ShowCLines" << i18n("Toggle display of constellation lines") << i18n("bool");
    new QTreeWidgetItem(opsShowOther, fields);
    fields.clear();
    fields << "ShowCBounds" << i18n("Toggle display of constellation boundaries") << i18n("bool");
    new QTreeWidgetItem(opsShowOther, fields);
    fields.clear();
    fields << "ShowCNames" << i18n("Toggle display of constellation names") << i18n("bool");
    new QTreeWidgetItem(opsShowOther, fields);
    fields.clear();
    fields << "ShowMilkyWay" << i18n("Toggle display of Milky Way") << i18n("bool");
    new QTreeWidgetItem(opsShowOther, fields);
    fields.clear();
    fields << "ShowGrid" << i18n("Toggle display of the coordinate grid") << i18n("bool");
    new QTreeWidgetItem(opsShowOther, fields);
    fields.clear();
    fields << "ShowEquator" << i18n("Toggle display of the celestial equator") << i18n("bool");
    new QTreeWidgetItem(opsShowOther, fields);
    fields.clear();
    fields << "ShowEcliptic" << i18n("Toggle display of the ecliptic") << i18n("bool");
    new QTreeWidgetItem(opsShowOther, fields);
    fields.clear();
    fields << "ShowHorizon" << i18n("Toggle display of the horizon line") << i18n("bool");
    new QTreeWidgetItem(opsShowOther, fields);
    fields.clear();
    fields << "ShowGround" << i18n("Toggle display of the opaque ground") << i18n("bool");
    new QTreeWidgetItem(opsShowOther, fields);
    fields.clear();
    fields << "ShowStarNames" << i18n("Toggle display of star name labels") << i18n("bool");
    new QTreeWidgetItem(opsShowOther, fields);
    fields.clear();
    fields << "ShowStarMagnitudes" << i18n("Toggle display of star magnitude labels") << i18n("bool");
    new QTreeWidgetItem(opsShowOther, fields);
    fields.clear();
    fields << "ShowAsteroidNames" << i18n("Toggle display of asteroid name labels") << i18n("bool");
    new QTreeWidgetItem(opsShowOther, fields);
    fields.clear();
    fields << "ShowCometNames" << i18n("Toggle display of comet name labels") << i18n("bool");
    new QTreeWidgetItem(opsShowOther, fields);
    fields.clear();
    fields << "ShowPlanetNames" << i18n("Toggle display of planet name labels") << i18n("bool");
    new QTreeWidgetItem(opsShowOther, fields);
    fields.clear();
    fields << "ShowPlanetImages" << i18n("Toggle display of planet images") << i18n("bool");
    new QTreeWidgetItem(opsShowOther, fields);
    fields.clear();

    argChangeViewOption->OptionName->addItem("ShowCLines");
    argChangeViewOption->OptionName->addItem("ShowCBounds");
    argChangeViewOption->OptionName->addItem("ShowCNames");
    argChangeViewOption->OptionName->addItem("ShowMilkyWay");
    argChangeViewOption->OptionName->addItem("ShowGrid");
    argChangeViewOption->OptionName->addItem("ShowEquator");
    argChangeViewOption->OptionName->addItem("ShowEcliptic");
    argChangeViewOption->OptionName->addItem("ShowHorizon");
    argChangeViewOption->OptionName->addItem("ShowGround");
    argChangeViewOption->OptionName->addItem("ShowStarNames");
    argChangeViewOption->OptionName->addItem("ShowStarMagnitudes");
    argChangeViewOption->OptionName->addItem("ShowAsteroidNames");
    argChangeViewOption->OptionName->addItem("ShowCometNames");
    argChangeViewOption->OptionName->addItem("ShowPlanetNames");
    argChangeViewOption->OptionName->addItem("ShowPlanetImages");

    opsCName = new QTreeWidgetItem(otv->optionsList(), QStringList(i18n("Constellation Names")));
    fields << "UseLatinConstellNames" << i18n("Show Latin constellation names") << i18n("bool");
    new QTreeWidgetItem(opsCName, fields);
    fields.clear();
    fields << "UseLocalConstellNames" << i18n("Show constellation names in local language") << i18n("bool");
    new QTreeWidgetItem(opsCName, fields);
    fields.clear();
    fields << "UseAbbrevConstellNames" << i18n("Show IAU-standard constellation abbreviations") << i18n("bool");
    new QTreeWidgetItem(opsCName, fields);
    fields.clear();

    argChangeViewOption->OptionName->addItem("UseLatinConstellNames");
    argChangeViewOption->OptionName->addItem("UseLocalConstellNames");
    argChangeViewOption->OptionName->addItem("UseAbbrevConstellNames");

    opsHide = new QTreeWidgetItem(otv->optionsList(), QStringList(i18n("Hide Items")));
    fields << "HideOnSlew" << i18n("Toggle whether objects hidden while slewing display") << i18n("bool");
    new QTreeWidgetItem(opsHide, fields);
    fields.clear();
    fields << "SlewTimeScale" << i18n("Timestep threshold (in seconds) for hiding objects") << i18n("double");
    new QTreeWidgetItem(opsHide, fields);
    fields.clear();
    fields << "HideStars" << i18n("Hide faint stars while slewing?") << i18n("bool");
    new QTreeWidgetItem(opsHide, fields);
    fields.clear();
    fields << "HidePlanets" << i18n("Hide solar system bodies while slewing?") << i18n("bool");
    new QTreeWidgetItem(opsHide, fields);
    fields.clear();
    fields << "HideMilkyWay" << i18n("Hide Milky Way while slewing?") << i18n("bool");
    new QTreeWidgetItem(opsHide, fields);
    fields.clear();
    fields << "HideCNames" << i18n("Hide constellation names while slewing?") << i18n("bool");
    new QTreeWidgetItem(opsHide, fields);
    fields.clear();
    fields << "HideCLines" << i18n("Hide constellation lines while slewing?") << i18n("bool");
    new QTreeWidgetItem(opsHide, fields);
    fields.clear();
    fields << "HideCBounds" << i18n("Hide constellation boundaries while slewing?") << i18n("bool");
    new QTreeWidgetItem(opsHide, fields);
    fields.clear();
    fields << "HideGrid" << i18n("Hide coordinate grid while slewing?") << i18n("bool");
    new QTreeWidgetItem(opsHide, fields);
    fields.clear();

    argChangeViewOption->OptionName->addItem("HideOnSlew");
    argChangeViewOption->OptionName->addItem("SlewTimeScale");
    argChangeViewOption->OptionName->addItem("HideStars");
    argChangeViewOption->OptionName->addItem("HidePlanets");
    argChangeViewOption->OptionName->addItem("HideMilkyWay");
    argChangeViewOption->OptionName->addItem("HideCNames");
    argChangeViewOption->OptionName->addItem("HideCLines");
    argChangeViewOption->OptionName->addItem("HideCBounds");
    argChangeViewOption->OptionName->addItem("HideGrid");

    opsSkymap = new QTreeWidgetItem(otv->optionsList(), QStringList(i18n("Skymap Options")));
    fields << "UseAltAz" << i18n("Use Horizontal coordinates? (otherwise, use Equatorial)") << i18n("bool");
    new QTreeWidgetItem(opsSkymap, fields);
    fields.clear();
    fields << "ZoomFactor" << i18n("Set the Zoom Factor") << i18n("double");
    new QTreeWidgetItem(opsSkymap, fields);
    fields.clear();
    fields << "FOVName" << i18n("Select angular size for the FOV symbol (in arcmin)") << i18n("double");
    new QTreeWidgetItem(opsSkymap, fields);
    fields.clear();
    fields << "FOVShape" << i18n("Select shape for the FOV symbol (0=Square, 1=Circle, 2=Crosshairs, 4=Bullseye)")
           << i18n("int");
    new QTreeWidgetItem(opsSkymap, fields);
    fields.clear();
    fields << "FOVColor" << i18n("Select color for the FOV symbol") << i18n("string");
    new QTreeWidgetItem(opsSkymap, fields);
    fields.clear();
    fields << "UseAnimatedSlewing" << i18n("Use animated slewing? (otherwise, \"snap\" to new focus)") << i18n("bool");
    new QTreeWidgetItem(opsSkymap, fields);
    fields.clear();
    fields << "UseRefraction" << i18n("Correct for atmospheric refraction?") << i18n("bool");
    new QTreeWidgetItem(opsSkymap, fields);
    fields.clear();
    fields << "UseAutoLabel" << i18n("Automatically attach name label to centered object?") << i18n("bool");
    new QTreeWidgetItem(opsSkymap, fields);
    fields.clear();
    fields << "UseHoverLabel" << i18n("Attach temporary name label when hovering mouse over an object?")
           << i18n("bool");
    new QTreeWidgetItem(opsSkymap, fields);
    fields.clear();
    fields << "UseAutoTrail" << i18n("Automatically add trail to centered solar system body?") << i18n("bool");
    new QTreeWidgetItem(opsSkymap, fields);
    fields.clear();
    fields << "FadePlanetTrails" << i18n("Planet trails fade to sky color? (otherwise color is constant)")
           << i18n("bool");
    new QTreeWidgetItem(opsSkymap, fields);
    fields.clear();

    argChangeViewOption->OptionName->addItem("UseAltAz");
    argChangeViewOption->OptionName->addItem("ZoomFactor");
    argChangeViewOption->OptionName->addItem("FOVName");
    argChangeViewOption->OptionName->addItem("FOVSize");
    argChangeViewOption->OptionName->addItem("FOVShape");
    argChangeViewOption->OptionName->addItem("FOVColor");
    argChangeViewOption->OptionName->addItem("UseRefraction");
    argChangeViewOption->OptionName->addItem("UseAutoLabel");
    argChangeViewOption->OptionName->addItem("UseHoverLabel");
    argChangeViewOption->OptionName->addItem("UseAutoTrail");
    argChangeViewOption->OptionName->addItem("AnimateSlewing");
    argChangeViewOption->OptionName->addItem("FadePlanetTrails");

    opsLimit = new QTreeWidgetItem(otv->optionsList(), QStringList(i18n("Limits")));
    /*
    fields << "magLimitDrawStar" << i18n( "magnitude of faintest star drawn on map when zoomed in" ) << i18n( "double" );
    new QTreeWidgetItem( opsLimit, fields );
    fields.clear();
    fields << "magLimitDrawStarZoomOut" << i18n( "magnitude of faintest star drawn on map when zoomed out" ) << i18n( "double" );
    new QTreeWidgetItem( opsLimit, fields );
    fields.clear();
    */

    // TODO: We have disabled the following two features. Enable them when feasible...
    /*
    fields << "magLimitDrawDeepSky" << i18n( "magnitude of faintest nonstellar object drawn on map when zoomed in" ) << i18n( "double" );
    new QTreeWidgetItem( opsLimit, fields );
    fields.clear();
    fields << "magLimitDrawDeepSkyZoomOut" << i18n( "magnitude of faintest nonstellar object drawn on map when zoomed out" ) << i18n( "double" );
    new QTreeWidgetItem( opsLimit, fields );
    fields.clear();
    */

    //FIXME: This description is incorrect! Fix after strings freeze
    fields << "starLabelDensity" << i18n("magnitude of faintest star labeled on map") << i18n("double");
    new QTreeWidgetItem(opsLimit, fields);
    fields.clear();
    fields << "magLimitHideStar" << i18n("magnitude of brightest star hidden while slewing") << i18n("double");
    new QTreeWidgetItem(opsLimit, fields);
    fields.clear();
    fields << "magLimitAsteroid" << i18n("magnitude of faintest asteroid drawn on map") << i18n("double");
    new QTreeWidgetItem(opsLimit, fields);
    fields.clear();
    //FIXME: This description is incorrect! Fix after strings freeze
    fields << "asteroidLabelDensity" << i18n("magnitude of faintest asteroid labeled on map") << i18n("double");
    new QTreeWidgetItem(opsLimit, fields);
    fields.clear();
    fields << "maxRadCometName" << i18n("comets nearer to the Sun than this (in AU) are labeled on map")
           << i18n("double");
    new QTreeWidgetItem(opsLimit, fields);
    fields.clear();

    //    argChangeViewOption->OptionName->addItem( "magLimitDrawStar" );
    //    argChangeViewOption->OptionName->addItem( "magLimitDrawStarZoomOut" );
    argChangeViewOption->OptionName->addItem("magLimitDrawDeepSky");
    argChangeViewOption->OptionName->addItem("magLimitDrawDeepSkyZoomOut");
    argChangeViewOption->OptionName->addItem("starLabelDensity");
    argChangeViewOption->OptionName->addItem("magLimitHideStar");
    argChangeViewOption->OptionName->addItem("magLimitAsteroid");
    argChangeViewOption->OptionName->addItem("asteroidLabelDensity");
    argChangeViewOption->OptionName->addItem("maxRadCometName");

    //init the list of color names and values
    for (unsigned int i = 0; i < KStarsData::Instance()->colorScheme()->numberOfColors(); ++i)
    {
        argSetColor->ColorName->addItem(KStarsData::Instance()->colorScheme()->nameAt(i));
    }

    //init list of color scheme names
    argLoadColorScheme->SchemeList->addItem(i18nc("use default color scheme", "Default Colors"));
    argLoadColorScheme->SchemeList->addItem(i18nc("use 'star chart' color scheme", "Star Chart"));
    argLoadColorScheme->SchemeList->addItem(i18nc("use 'night vision' color scheme", "Night Vision"));
    argLoadColorScheme->SchemeList->addItem(i18nc("use 'moonless night' color scheme", "Moonless Night"));

    QFile file;
    QString line;
    //determine filename in local user KDE directory tree.
    file.setFileName(KSPaths::locate(QStandardPaths::AppDataLocation, "colors.dat"));
    if (file.open(QIODevice::ReadOnly))
    {
        QTextStream stream(&file);

        while (!stream.atEnd())
        {
            line = stream.readLine();
            argLoadColorScheme->SchemeList->addItem(line.left(line.indexOf(':')));
        }
        file.close();
    }
}

//Slots defined in ScriptBuilderUI
void ScriptBuilder::slotNew()
{
    saveWarning();
    if (!UnsavedChanges)
    {
        ScriptList.clear();
        sb->ScriptListBox->clear();
        sb->ArgStack->setCurrentWidget(argBlank);

        sb->CopyButton->setEnabled(false);
        sb->RemoveButton->setEnabled(false);
        sb->RunButton->setEnabled(false);
        sb->SaveAsButton->setEnabled(false);

        currentFileURL.clear();
        currentScriptName.clear();
    }
}

void ScriptBuilder::slotOpen()
{
    saveWarning();

    QString fname;
    QTemporaryFile tmpfile;
    tmpfile.open();

    if (!UnsavedChanges)
    {
        currentFileURL = QFileDialog::getOpenFileUrl(
                             KStars::Instance(), QString(), QUrl(currentDir),
                             "*.kstars|" + i18nc("Filter by file type: KStars Scripts.", "KStars Scripts (*.kstars)"));

        if (currentFileURL.isValid())
        {
            currentDir = currentFileURL.toLocalFile();

            ScriptList.clear();
            sb->ScriptListBox->clear();
            sb->ArgStack->setCurrentWidget(argBlank);

            if (currentFileURL.isLocalFile())
            {
                fname = currentFileURL.toLocalFile();
            }
            else
            {
                fname = tmpfile.fileName();
                if (KIO::copy(currentFileURL, QUrl(fname))->exec() == false)
                    //if ( ! KIO::NetAccess::download( currentFileURL, fname, (QWidget*) 0 ) )
                    KSNotification::sorry(i18n("Could not download remote file."), i18n("Download Error"));
            }

            QFile f(fname);
            if (!f.open(QIODevice::ReadOnly))
            {
                KSNotification::sorry(i18n("Could not open file %1.", f.fileName()), i18n("Could Not Open File"));
                currentFileURL.clear();
                return;
            }

            QTextStream istream(&f);
            readScript(istream);

            f.close();
        }
        else if (!currentFileURL.url().isEmpty())
        {
            KSNotification::sorry(i18n("Invalid URL: %1", currentFileURL.url()), i18n("Invalid URL"));
            currentFileURL.clear();
        }
    }
}

void ScriptBuilder::slotSave()
{
    QString fname;
    QTemporaryFile tmpfile;
    tmpfile.open();

    if (currentScriptName.isEmpty())
    {
        //Get Script Name and Author info
        if (snd->exec() == QDialog::Accepted)
        {
            currentScriptName = snd->scriptName();
            currentAuthor     = snd->authorName();
        }
        else
        {
            return;
        }
    }

    bool newFilename = false;
    if (currentFileURL.isEmpty())
    {
        currentFileURL = QFileDialog::getSaveFileUrl(
                             KStars::Instance(), QString(), QUrl(currentDir),
                             "*.kstars|" + i18nc("Filter by file type: KStars Scripts.", "KStars Scripts (*.kstars)"));
        newFilename = true;
    }

    if (currentFileURL.isValid())
    {
        currentDir = currentFileURL.toLocalFile();

        if (currentFileURL.isLocalFile())
        {
            fname = currentFileURL.toLocalFile();

            //Warn user if file exists
            if (newFilename == true && QFile::exists(currentFileURL.toLocalFile()))
            {
                int r = KMessageBox::warningContinueCancel(static_cast<QWidget *>(parent()),
                        i18n("A file named \"%1\" already exists. "
                             "Overwrite it?",
                             currentFileURL.fileName()),
                        i18n("Overwrite File?"), KStandardGuiItem::overwrite());

                if (r == KMessageBox::Cancel)
                    return;
            }
        }
        else
        {
            fname = tmpfile.fileName();
        }

        if (fname.right(7).toLower() != ".kstars")
            fname += ".kstars";

        QFile f(fname);
        if (!f.open(QIODevice::WriteOnly))
        {
            QString message = i18n("Could not open file %1.", f.fileName());
            KSNotification::sorry(message, i18n("Could Not Open File"));
            currentFileURL.clear();
            return;
        }

        QTextStream ostream(&f);
        writeScript(ostream);
        f.close();

#ifndef _WIN32
        //set rwx for owner, rx for group, rx for other
        chmod(fname.toLatin1(), S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
#endif

        if (tmpfile.fileName() == fname)
        {
            //need to upload to remote location
            //if ( ! KIO::NetAccess::upload( tmpfile.fileName(), currentFileURL, (QWidget*) 0 ) )
            if (KIO::storedHttpPost(&tmpfile, currentFileURL)->exec() == false)
            {
                QString message = i18n("Could not upload image to remote location: %1", currentFileURL.url());
                KSNotification::sorry(message, i18n("Could not upload file"));
            }
        }

        setUnsavedChanges(false);
    }
    else
    {
        QString message = i18n("Invalid URL: %1", currentFileURL.url());
        KSNotification::sorry(message, i18n("Invalid URL"));
        currentFileURL.clear();
    }
}

void ScriptBuilder::slotSaveAs()
{
    currentFileURL.clear();
    currentScriptName.clear();
    slotSave();
}

void ScriptBuilder::saveWarning()
{
    if (UnsavedChanges)
    {
        QString caption = i18n("Save Changes to Script?");
        QString message = i18n("The current script has unsaved changes.  Would you like to save before closing it?");
        int ans =
            KMessageBox::warningYesNoCancel(nullptr, message, caption, KStandardGuiItem::save(), KStandardGuiItem::discard());
        if (ans == KMessageBox::Yes)
        {
            slotSave();
            setUnsavedChanges(false);
        }
        else if (ans == KMessageBox::No)
        {
            setUnsavedChanges(false);
        }

        //Do nothing if 'cancel' selected
    }
}

void ScriptBuilder::slotRunScript()
{
    //hide window while script runs
    // If this is uncommented, the program hangs before the script is executed.  Why?
    //	hide();

    //Save current script to a temporary file, then execute that file.
    //For some reason, I can't use KTempFile here!  If I do, then the temporary script
    //is not executable.  Bizarre...
    //KTempFile tmpfile;
    //QString fname = tmpfile.name();
    QString fname = QDir::tempPath() + QDir::separator() + "kstars-tempscript";

    QFile f(fname);
    if (f.exists())
        f.remove();
    if (!f.open(QIODevice::WriteOnly))
    {
        QString message = i18n("Could not open file %1.", f.fileName());
        KSNotification::sorry(message, i18n("Could Not Open File"));
        currentFileURL.clear();
        return;
    }

    QTextStream ostream(&f);
    writeScript(ostream);
    f.close();

#ifndef _WIN32
    //set rwx for owner, rx for group, rx for other
    chmod(QFile::encodeName(f.fileName()), S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH);
#endif

    QProcess p;
#ifdef Q_OS_OSX
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    QString path            = env.value("PATH", "");
    env.insert("PATH", "/usr/local/bin:" + QCoreApplication::applicationDirPath() + ':' + path);
    p.setProcessEnvironment(env);
#endif
    p.start(f.fileName());

    if (!p.waitForStarted())
        qDebug() << "Process did not start.";

    while (!p.waitForFinished(10))
    {
        qApp->processEvents(); //otherwise tempfile may get deleted before script completes.
        if (p.state() != QProcess::Running)
            break;
    }
    //delete temp file
    if (f.exists())
        f.remove();

    //uncomment if 'hide()' is uncommented...
    //	show();
}

/*
  This can't work anymore and is also not portable in any way :(
*/
void ScriptBuilder::writeScript(QTextStream &ostream)
{
    // FIXME Without --print-reply, the dbus-send doesn't do anything, why??
    QString dbus_call    = "dbus-send --dest=org.kde.kstars --print-reply ";
    QString main_method  = "/KStars org.kde.kstars.";
    QString clock_method = "/KStars/SimClock org.kde.kstars.SimClock.";

    //Write script header
    ostream << "#!/bin/bash\n";
    ostream << "#KStars DBus script: " << currentScriptName << '\n';
    ostream << "#by " << currentAuthor << '\n';
    ostream << "#last modified: " << KStarsDateTime::currentDateTime().toString(Qt::ISODate) << '\n';
    ostream << "#\n";

    foreach (ScriptFunction *sf, ScriptList)
    {
        if (!sf->valid())
            continue;

        if (sf->isClockFunction())
        {
            ostream << dbus_call << clock_method << sf->scriptLine() << '\n';
        }
        else
        {
            ostream << dbus_call << main_method << sf->scriptLine() << '\n';
        }
    }

    //Write script footer
    ostream << "##\n";
    ostream.flush();
}

void ScriptBuilder::readScript(QTextStream &istream)
{
    QString line;
    QString service_name = "org.kde.kstars.";
    QString fn_name;

    while (!istream.atEnd())
    {
        line = istream.readLine();

        //look for name of script
        if (line.contains("#KStars DBus script: "))
            currentScriptName = line.mid(21).trimmed();

        //look for author of scriptbuilder
        if (line.contains("#by "))
            currentAuthor = line.mid(4).trimmed();

        //Actual script functions
        if (line.left(4) == "dbus")
        {
            //is ClockFunction?
            if (line.contains("SimClock"))
            {
                service_name += "SimClock.";
            }

            //remove leading dbus prefix
            line = line.mid(line.lastIndexOf(service_name) + service_name.count());

            fn_name = line.left(line.indexOf(' '));

            line = line.mid(line.indexOf(' ') + 1);

            //construct a stringlist that is fcn name and its arg name/value pairs
            QStringList fn;

            // If the function lacks any arguments, do not attempt to split
            //            if (fn_name != line)
            //                fn = line.split(' ');

            if (parseFunction(fn_name, line))
            {
                sb->ScriptListBox->addItem(ScriptList.last()->name());
                // Initially, any read script is valid!
                ScriptList.last()->setValid(true);
            }
            else
                qWarning() << i18n("Could not parse script.  Line was: %1", line);

        } // end if left(4) == "dcop"
    }     // end while !atEnd()

    //Select first item in sb->ScriptListBox
    if (sb->ScriptListBox->count())
    {
        sb->ScriptListBox->setCurrentItem(nullptr);
        slotArgWidget();
    }
}

bool ScriptBuilder::parseFunction(const QString &fn_name, const QString &fn_args)
{
    // clean up the string list first if needed
    // We need to perform this in case we havea quoted string "NGC 3000" because this will counted
    // as two arguments, and it should be counted as one.
    //    bool foundQuote(false), quoteProcessed(false);
    //    QString cur, arg;
    //    QStringList::iterator it;

    //    for (it = fn.begin(); it != fn.end(); ++it)
    //    {
    //        cur = (*it);

    //        cur = cur.mid(cur.indexOf(":") + 1).remove('\'');

    //        (*it) = cur;

    //        if (cur.startsWith('\"'))
    //        {
    //            arg += cur.rightRef(cur.length() - 1);
    //            arg += ' ';
    //            foundQuote     = true;
    //            quoteProcessed = true;
    //        }
    //        else if (cur.endsWith('\"'))
    //        {
    //            arg += cur.leftRef(cur.length() - 1);
    //            arg += '\'';
    //            foundQuote = false;
    //        }
    //        else if (foundQuote)
    //        {
    //            arg += cur;
    //            arg += ' ';
    //        }
    //        else
    //        {
    //            arg += cur;
    //            arg += '\'';
    //        }
    //    }

    //    if (quoteProcessed)
    //        fn = arg.split(' ', QString::SkipEmptyParts);

    QRegularExpression re("(?<=:)[^:\\s]*");
    QRegularExpressionMatchIterator i = re.globalMatch(fn_args);
    QStringList fn;
    while (i.hasNext())
    {
        QRegularExpressionMatch match = i.next();
        fn << match.captured(0).remove("\"");
    };

    //loop over known functions to find a name match
    for (auto &sf : KStarsFunctionList)
    {
        if (fn_name == sf->name())
        {
            //            if (fn_name == "setGeoLocation")
            //            {
            //                ScriptList.append(new ScriptFunction(sf));
            //                ScriptList.last()->setArg(0, fn[0]);
            //                ScriptList.last()->setArg(1, fn[1]);
            //                ScriptList.last()->setArg(2, fn[2]);
            //            }
            //            else if (fn.count() != sf->numArgs())
            //                return false;

            ScriptList.append(new ScriptFunction(sf));

            for (int i = 0; i < sf->numArgs(); ++i)
                ScriptList.last()->setArg(i, fn[i]);

            return true;
        }

        foreach (ScriptFunction *sf, SimClockFunctionList)
        {
            if (fn_name == sf->name())
            {
                if (fn.count() != sf->numArgs())
                    return false;

                ScriptList.append(new ScriptFunction(sf));

                for (int i = 0; i < sf->numArgs(); ++i)
                    ScriptList.last()->setArg(i, fn[i]);

                return true;
            }
        }
    }

    //if we get here, no function-name match was found
    return false;
}

void ScriptBuilder::setUnsavedChanges(bool b)
{
    if (checkForChanges)
    {
        UnsavedChanges = b;
        sb->SaveButton->setEnabled(b);
    }
}

void ScriptBuilder::slotCopyFunction()
{
    if (!UnsavedChanges)
        setUnsavedChanges(true);

    int Pos = sb->ScriptListBox->currentRow() + 1;
    ScriptList.insert(Pos, new ScriptFunction(ScriptList[Pos - 1]));
    //copy ArgVals
    for (int i = 0; i < ScriptList[Pos - 1]->numArgs(); ++i)
        ScriptList[Pos]->setArg(i, ScriptList[Pos - 1]->argVal(i));

    sb->ScriptListBox->insertItem(Pos, ScriptList[Pos]->name());
    //sb->ScriptListBox->setSelected( Pos, true );
    sb->ScriptListBox->setCurrentRow(Pos);
    slotArgWidget();
}

void ScriptBuilder::slotRemoveFunction()
{
    setUnsavedChanges(true);

    int Pos = sb->ScriptListBox->currentRow();
    ScriptList.removeAt(Pos);
    sb->ScriptListBox->takeItem(Pos);
    if (sb->ScriptListBox->count() == 0)
    {
        sb->ArgStack->setCurrentWidget(argBlank);
        sb->CopyButton->setEnabled(false);
        sb->RemoveButton->setEnabled(false);
        sb->RunButton->setEnabled(false);
        sb->SaveAsButton->setEnabled(false);
    }
    else
    {
        //sb->ScriptListBox->setSelected( Pos, true );
        if (Pos == sb->ScriptListBox->count())
        {
            Pos = Pos - 1;
        }
        sb->ScriptListBox->setCurrentRow(Pos);
    }
    slotArgWidget();
}

void ScriptBuilder::slotAddFunction()
{
    ScriptFunction *found        = nullptr;
    QTreeWidgetItem *currentItem = sb->FunctionTree->currentItem();

    if (currentItem == nullptr || currentItem->parent() == nullptr)
        return;

    for (auto &sc : KStarsFunctionList)
    {
        if (sc->prototype() == currentItem->text(0))
        {
            found = sc;
            break;
        }
    }

    for (auto &sc : SimClockFunctionList)
    {
        if (sc->prototype() == currentItem->text(0))
        {
            found = sc;
            break;
        }
    }

    if (found == nullptr)
        return;

    setUnsavedChanges(true);

    int Pos = sb->ScriptListBox->currentRow() + 1;

    ScriptList.insert(Pos, new ScriptFunction(found));
    sb->ScriptListBox->insertItem(Pos, ScriptList[Pos]->name());
    sb->ScriptListBox->setCurrentRow(Pos);
    slotArgWidget();
}

void ScriptBuilder::slotMoveFunctionUp()
{
    if (sb->ScriptListBox->currentRow() > 0)
    {
        setUnsavedChanges(true);

        //QString t = sb->ScriptListBox->currentItem()->text();
        QString t      = sb->ScriptListBox->currentItem()->text();
        unsigned int n = sb->ScriptListBox->currentRow();

        ScriptFunction *tmp = ScriptList.takeAt(n);
        ScriptList.insert(n - 1, tmp);

        sb->ScriptListBox->takeItem(n);
        sb->ScriptListBox->insertItem(n - 1, t);
        sb->ScriptListBox->setCurrentRow(n - 1);
        slotArgWidget();
    }
}

void ScriptBuilder::slotMoveFunctionDown()
{
    if (sb->ScriptListBox->currentRow() > -1 && sb->ScriptListBox->currentRow() < ((int)sb->ScriptListBox->count()) - 1)
    {
        setUnsavedChanges(true);

        QString t      = sb->ScriptListBox->currentItem()->text();
        unsigned int n = sb->ScriptListBox->currentRow();

        ScriptFunction *tmp = ScriptList.takeAt(n);
        ScriptList.insert(n + 1, tmp);

        sb->ScriptListBox->takeItem(n);
        sb->ScriptListBox->insertItem(n + 1, t);
        sb->ScriptListBox->setCurrentRow(n + 1);
        slotArgWidget();
    }
}

void ScriptBuilder::slotArgWidget()
{
    //First, setEnabled on buttons that act on the selected script function
    if (sb->ScriptListBox->currentRow() == -1) //no selection
    {
        sb->CopyButton->setEnabled(false);
        sb->RemoveButton->setEnabled(false);
        sb->UpButton->setEnabled(false);
        sb->DownButton->setEnabled(false);
    }
    else if (sb->ScriptListBox->count() == 1) //only one item, so disable up/down buttons
    {
        sb->CopyButton->setEnabled(true);
        sb->RemoveButton->setEnabled(true);
        sb->UpButton->setEnabled(false);
        sb->DownButton->setEnabled(false);
    }
    else if (sb->ScriptListBox->currentRow() == 0) //first item selected
    {
        sb->CopyButton->setEnabled(true);
        sb->RemoveButton->setEnabled(true);
        sb->UpButton->setEnabled(false);
        sb->DownButton->setEnabled(true);
    }
    else if (sb->ScriptListBox->currentRow() == ((int)sb->ScriptListBox->count()) - 1) //last item selected
    {
        sb->CopyButton->setEnabled(true);
        sb->RemoveButton->setEnabled(true);
        sb->UpButton->setEnabled(true);
        sb->DownButton->setEnabled(false);
    }
    else //other item selected
    {
        sb->CopyButton->setEnabled(true);
        sb->RemoveButton->setEnabled(true);
        sb->UpButton->setEnabled(true);
        sb->DownButton->setEnabled(true);
    }

    //RunButton and SaveAs button enabled when script not empty.
    if (sb->ScriptListBox->count())
    {
        sb->RunButton->setEnabled(true);
        sb->SaveAsButton->setEnabled(true);
    }
    else
    {
        sb->RunButton->setEnabled(false);
        sb->SaveAsButton->setEnabled(false);
        setUnsavedChanges(false);
    }

    //Display the function's arguments widget
    if (sb->ScriptListBox->currentRow() > -1 && sb->ScriptListBox->currentRow() < ((int)sb->ScriptListBox->count()))
    {
        unsigned int n     = sb->ScriptListBox->currentRow();
        ScriptFunction *sf = ScriptList.at(n);

        checkForChanges = false; //Don't signal unsaved changes

        if (sf->name() == "lookTowards")
        {
            sb->ArgStack->setCurrentWidget(argLookToward);
            QString s = sf->argVal(0);
            argLookToward->FocusEdit->setEditText(s);
        }
        else if (sf->name() == "addLabel" || sf->name() == "removeLabel" || sf->name() == "addTrail" ||
                 sf->name() == "removeTrail")
        {
            sb->ArgStack->setCurrentWidget(argFindObject);
            QString s = sf->argVal(0);
            argFindObject->NameEdit->setText(s);
        }
        else if (sf->name() == "setRaDec")
        {
            bool ok(false);
            double r(0.0), d(0.0);
            dms ra(0.0);

            sb->ArgStack->setCurrentWidget(argSetRaDec);

            ok = !sf->argVal(0).isEmpty();
            if (ok)
                r = sf->argVal(0).toDouble(&ok);
            else
                argSetRaDec->RABox->clear();
            if (ok)
            {
                ra.setH(r);
                argSetRaDec->RABox->showInHours(ra);
            }

            ok = !sf->argVal(1).isEmpty();
            if (ok)
                d = sf->argVal(1).toDouble(&ok);
            else
                argSetRaDec->DecBox->clear();
            if (ok)
                argSetRaDec->DecBox->showInDegrees(dms(d));
        }
        else if (sf->name() == "setAltAz")
        {
            bool ok(false);
            double x(0.0), y(0.0);

            sb->ArgStack->setCurrentWidget(argSetAltAz);

            ok = !sf->argVal(0).isEmpty();
            if (ok)
                y = sf->argVal(0).toDouble(&ok);
            else
                argSetAltAz->AzBox->clear();
            if (ok)
                argSetAltAz->AltBox->showInDegrees(dms(y));
            else
                argSetAltAz->AltBox->clear();

            ok = !sf->argVal(1).isEmpty();
            x  = sf->argVal(1).toDouble(&ok);
            if (ok)
                argSetAltAz->AzBox->showInDegrees(dms(x));
        }
        else if (sf->name() == "zoomIn")
        {
            sb->ArgStack->setCurrentWidget(argBlank);
            //no Args
        }
        else if (sf->name() == "zoomOut")
        {
            sb->ArgStack->setCurrentWidget(argBlank);
            //no Args
        }
        else if (sf->name() == "defaultZoom")
        {
            sb->ArgStack->setCurrentWidget(argBlank);
            //no Args
        }
        else if (sf->name() == "zoom")
        {
            sb->ArgStack->setCurrentWidget(argZoom);
            bool ok(false);
            /*double z = */ sf->argVal(0).toDouble(&ok);
            if (ok)
                argZoom->ZoomBox->setText(sf->argVal(0));
            else
                argZoom->ZoomBox->setText("2000.");
        }
        else if (sf->name() == "exportImage")
        {
            sb->ArgStack->setCurrentWidget(argExportImage);
            argExportImage->ExportFileName->setUrl(QUrl::fromUserInput(sf->argVal(0)));
            bool ok(false);
            int w = 0, h = 0;
            w = sf->argVal(1).toInt(&ok);
            if (ok)
                h = sf->argVal(2).toInt(&ok);
            if (ok)
            {
                argExportImage->ExportWidth->setValue(w);
                argExportImage->ExportHeight->setValue(h);
            }
            else
            {
                argExportImage->ExportWidth->setValue(SkyMap::Instance()->width());
                argExportImage->ExportHeight->setValue(SkyMap::Instance()->height());
            }
        }
        else if (sf->name() == "printImage")
        {
            if (sf->argVal(0) == i18n("true"))
                argPrintImage->UsePrintDialog->setChecked(true);
            else
                argPrintImage->UsePrintDialog->setChecked(false);
            if (sf->argVal(1) == i18n("true"))
                argPrintImage->UseChartColors->setChecked(true);
            else
                argPrintImage->UseChartColors->setChecked(false);
        }
        else if (sf->name() == "setLocalTime")
        {
            sb->ArgStack->setCurrentWidget(argSetLocalTime);
            bool ok(false);
            int year = 0, month = 0, day = 0, hour = 0, min = 0, sec = 0;

            year = sf->argVal(0).toInt(&ok);
            if (ok)
                month = sf->argVal(1).toInt(&ok);
            if (ok)
                day = sf->argVal(2).toInt(&ok);
            if (ok)
                argSetLocalTime->DateWidget->setDate(QDate(year, month, day));
            else
                argSetLocalTime->DateWidget->setDate(QDate::currentDate());

            hour = sf->argVal(3).toInt(&ok);
            if (sf->argVal(3).isEmpty())
                ok = false;
            if (ok)
                min = sf->argVal(4).toInt(&ok);
            if (ok)
                sec = sf->argVal(5).toInt(&ok);
            if (ok)
                argSetLocalTime->TimeBox->setTime(QTime(hour, min, sec));
            else
                argSetLocalTime->TimeBox->setTime(QTime(QTime::currentTime()));
        }
        else if (sf->name() == "waitFor")
        {
            sb->ArgStack->setCurrentWidget(argWaitFor);
            bool ok(false);
            int sec = sf->argVal(0).toInt(&ok);
            if (ok)
                argWaitFor->DelayBox->setValue(sec);
            else
                argWaitFor->DelayBox->setValue(0);
        }
        else if (sf->name() == "waitForKey")
        {
            sb->ArgStack->setCurrentWidget(argWaitForKey);
            if (sf->argVal(0).length() == 1 || sf->argVal(0).toLower() == "space")
                argWaitForKey->WaitKeyEdit->setText(sf->argVal(0));
            else
                argWaitForKey->WaitKeyEdit->setText(QString());
        }
        else if (sf->name() == "setTracking")
        {
            sb->ArgStack->setCurrentWidget(argSetTracking);
            if (sf->argVal(0) == i18n("true"))
                argSetTracking->CheckTrack->setChecked(true);
            else
                argSetTracking->CheckTrack->setChecked(false);
        }
        else if (sf->name() == "changeViewOption")
        {
            sb->ArgStack->setCurrentWidget(argChangeViewOption);
            argChangeViewOption->OptionName->setCurrentIndex(argChangeViewOption->OptionName->findText(sf->argVal(0)));
            argChangeViewOption->OptionValue->setText(sf->argVal(1));
        }
        else if (sf->name() == "setGeoLocation")
        {
            sb->ArgStack->setCurrentWidget(argSetGeoLocation);
            argSetGeoLocation->CityName->setText(sf->argVal(0));
            argSetGeoLocation->ProvinceName->setText(sf->argVal(1));
            argSetGeoLocation->CountryName->setText(sf->argVal(2));
        }
        else if (sf->name() == "setColor")
        {
            sb->ArgStack->setCurrentWidget(argSetColor);
            if (sf->argVal(0).isEmpty())
                sf->setArg(0, "SkyColor"); //initialize default value
            argSetColor->ColorName->setCurrentIndex(
                argSetColor->ColorName->findText(KStarsData::Instance()->colorScheme()->nameFromKey(sf->argVal(0))));
            argSetColor->ColorValue->setColor(QColor(sf->argVal(1).remove('\\')));
        }
        else if (sf->name() == "loadColorScheme")
        {
            sb->ArgStack->setCurrentWidget(argLoadColorScheme);
            argLoadColorScheme->SchemeList->setCurrentItem(
                argLoadColorScheme->SchemeList->findItems(sf->argVal(0).remove('\"'), Qt::MatchExactly).at(0));
        }
        else if (sf->name() == "stop")
        {
            sb->ArgStack->setCurrentWidget(argBlank);
            //no Args
        }
        else if (sf->name() == "start")
        {
            sb->ArgStack->setCurrentWidget(argBlank);
            //no Args
        }
        else if (sf->name() == "setClockScale")
        {
            sb->ArgStack->setCurrentWidget(argTimeScale);
            bool ok(false);
            double ts = sf->argVal(0).toDouble(&ok);
            if (ok)
                argTimeScale->TimeScale->tsbox()->changeScale(float(ts));
            else
                argTimeScale->TimeScale->tsbox()->changeScale(0.0);
        }

        checkForChanges = true; //signal unsaved changes if the argument widgets are changed
    }
}

void ScriptBuilder::slotShowDoc()
{
    ScriptFunction *found        = nullptr;
    QTreeWidgetItem *currentItem = sb->FunctionTree->currentItem();

    if (currentItem == nullptr || currentItem->parent() == nullptr)
        return;

    for (auto &sc : KStarsFunctionList)
    {
        if (sc->prototype() == currentItem->text(0))
        {
            found = sc;
            break;
        }
    }

    for (auto &sc : SimClockFunctionList)
    {
        if (sc->prototype() == currentItem->text(0))
        {
            found = sc;
            break;
        }
    }

    if (found == nullptr)
    {
        sb->AddButton->setEnabled(false);
        qWarning() << i18n("Function index out of bounds.");
        return;
    }

    sb->AddButton->setEnabled(true);
    sb->FuncDoc->setHtml(found->description());
}

//Slots for Arg Widgets
void ScriptBuilder::slotFindCity()
{
    QPointer<LocationDialog> ld = new LocationDialog(this);

    if (ld->exec() == QDialog::Accepted)
    {
        if (ld->selectedCity())
        {
            // set new location names
            argSetGeoLocation->CityName->setText(ld->selectedCityName());
            if (!ld->selectedProvinceName().isEmpty())
            {
                argSetGeoLocation->ProvinceName->setText(ld->selectedProvinceName());
            }
            else
            {
                argSetGeoLocation->ProvinceName->clear();
            }
            argSetGeoLocation->CountryName->setText(ld->selectedCountryName());

            ScriptFunction *sf = ScriptList[sb->ScriptListBox->currentRow()];
            if (sf->name() == "setGeoLocation")
            {
                setUnsavedChanges(true);

                sf->setArg(0, ld->selectedCityName());
                sf->setArg(1, ld->selectedProvinceName());
                sf->setArg(2, ld->selectedCountryName());
            }
            else
            {
                warningMismatch("setGeoLocation");
            }
        }
    }
    delete ld;
}

void ScriptBuilder::slotFindObject()
{
    if (FindDialog::Instance()->exec() == QDialog::Accepted && FindDialog::Instance()->targetObject())
    {
        setUnsavedChanges(true);

        if (sender() == argLookToward->FindButton)
            argLookToward->FocusEdit->setEditText(FindDialog::Instance()->targetObject()->name());
        else
            argFindObject->NameEdit->setText(FindDialog::Instance()->targetObject()->name());
    }
}

void ScriptBuilder::slotShowOptions()
{
    //Show tree-view of view options
    if (otv->exec() == QDialog::Accepted)
    {
        argChangeViewOption->OptionName->setCurrentIndex(
            argChangeViewOption->OptionName->findText(otv->optionsList()->currentItem()->text(0)));
    }
}

void ScriptBuilder::slotLookToward()
{
    ScriptFunction *sf = ScriptList[sb->ScriptListBox->currentRow()];

    if (sf->name() == "lookTowards")
    {
        setUnsavedChanges(true);

        sf->setArg(0, argLookToward->FocusEdit->currentText());
        sf->setValid(true);
    }
    else
    {
        warningMismatch("lookTowards");
    }
}

void ScriptBuilder::slotArgFindObject()
{
    ScriptFunction *sf = ScriptList[sb->ScriptListBox->currentRow()];

    if (sf->name() == "addLabel" || sf->name() == "removeLabel" || sf->name() == "addTrail" ||
            sf->name() == "removeTrail")
    {
        setUnsavedChanges(true);

        sf->setArg(0, argFindObject->NameEdit->text());
        sf->setValid(true);
    }
    else
    {
        warningMismatch(sf->name());
    }
}

void ScriptBuilder::slotRa()
{
    ScriptFunction *sf = ScriptList[sb->ScriptListBox->currentRow()];

    if (sf->name() == "setRaDec")
    {
        //do nothing if box is blank (because we could be clearing boxes while switching argWidgets)
        if (argSetRaDec->RABox->text().isEmpty())
            return;

        bool ok(false);
        dms ra = argSetRaDec->RABox->createDms(false, &ok);
        if (ok)
        {
            setUnsavedChanges(true);

            sf->setArg(0, QString("%1").arg(ra.Hours()));
            if (!sf->argVal(1).isEmpty())
                sf->setValid(true);
        }
        else
        {
            sf->setArg(0, QString());
            sf->setValid(false);
        }
    }
    else
    {
        warningMismatch("setRaDec");
    }
}

void ScriptBuilder::slotDec()
{
    ScriptFunction *sf = ScriptList[sb->ScriptListBox->currentRow()];

    if (sf->name() == "setRaDec")
    {
        //do nothing if box is blank (because we could be clearing boxes while switching argWidgets)
        if (argSetRaDec->DecBox->text().isEmpty())
            return;

        bool ok(false);
        dms dec = argSetRaDec->DecBox->createDms(true, &ok);
        if (ok)
        {
            setUnsavedChanges(true);

            sf->setArg(1, QString("%1").arg(dec.Degrees()));
            if (!sf->argVal(0).isEmpty())
                sf->setValid(true);
        }
        else
        {
            sf->setArg(1, QString());
            sf->setValid(false);
        }
    }
    else
    {
        warningMismatch("setRaDec");
    }
}

void ScriptBuilder::slotAz()
{
    ScriptFunction *sf = ScriptList[sb->ScriptListBox->currentRow()];

    if (sf->name() == "setAltAz")
    {
        //do nothing if box is blank (because we could be clearing boxes while switching argWidgets)
        if (argSetAltAz->AzBox->text().isEmpty())
            return;

        bool ok(false);
        dms az = argSetAltAz->AzBox->createDms(true, &ok);
        if (ok)
        {
            setUnsavedChanges(true);
            sf->setArg(1, QString("%1").arg(az.Degrees()));
            if (!sf->argVal(0).isEmpty())
                sf->setValid(true);
        }
        else
        {
            sf->setArg(1, QString());
            sf->setValid(false);
        }
    }
    else
    {
        warningMismatch("setAltAz");
    }
}

void ScriptBuilder::slotAlt()
{
    ScriptFunction *sf = ScriptList[sb->ScriptListBox->currentRow()];

    if (sf->name() == "setAltAz")
    {
        //do nothing if box is blank (because we could be clearing boxes while switching argWidgets)
        if (argSetAltAz->AltBox->text().isEmpty())
            return;

        bool ok(false);
        dms alt = argSetAltAz->AltBox->createDms(true, &ok);
        if (ok)
        {
            setUnsavedChanges(true);

            sf->setArg(0, QString("%1").arg(alt.Degrees()));
            if (!sf->argVal(1).isEmpty())
                sf->setValid(true);
        }
        else
        {
            sf->setArg(0, QString());
            sf->setValid(false);
        }
    }
    else
    {
        warningMismatch("setAltAz");
    }
}

void ScriptBuilder::slotChangeDate()
{
    ScriptFunction *sf = ScriptList[sb->ScriptListBox->currentRow()];

    if (sf->name() == "setLocalTime")
    {
        setUnsavedChanges(true);

        QDate date = argSetLocalTime->DateWidget->date();

        sf->setArg(0, QString("%1").arg(date.year()));
        sf->setArg(1, QString("%1").arg(date.month()));
        sf->setArg(2, QString("%1").arg(date.day()));
        if (!sf->argVal(3).isEmpty())
            sf->setValid(true);
    }
    else
    {
        warningMismatch("setLocalTime");
    }
}

void ScriptBuilder::slotChangeTime()
{
    ScriptFunction *sf = ScriptList[sb->ScriptListBox->currentRow()];

    if (sf->name() == "setLocalTime")
    {
        setUnsavedChanges(true);

        QTime time = argSetLocalTime->TimeBox->time();

        sf->setArg(3, QString("%1").arg(time.hour()));
        sf->setArg(4, QString("%1").arg(time.minute()));
        sf->setArg(5, QString("%1").arg(time.second()));
        if (!sf->argVal(0).isEmpty())
            sf->setValid(true);
    }
    else
    {
        warningMismatch("setLocalTime");
    }
}

void ScriptBuilder::slotWaitFor()
{
    ScriptFunction *sf = ScriptList[sb->ScriptListBox->currentRow()];

    if (sf->name() == "waitFor")
    {
        bool ok(false);
        int delay = argWaitFor->DelayBox->text().toInt(&ok);

        if (ok)
        {
            setUnsavedChanges(true);

            sf->setArg(0, QString("%1").arg(delay));
            sf->setValid(true);
        }
        else
        {
            sf->setValid(false);
        }
    }
    else
    {
        warningMismatch("waitFor");
    }
}

void ScriptBuilder::slotWaitForKey()
{
    ScriptFunction *sf = ScriptList[sb->ScriptListBox->currentRow()];

    if (sf->name() == "waitForKey")
    {
        QString sKey = argWaitForKey->WaitKeyEdit->text().trimmed();

        //DCOP script can only use single keystrokes; make sure entry is either one character,
        //or the word 'space'
        if (sKey.length() == 1 || sKey == "space")
        {
            setUnsavedChanges(true);

            sf->setArg(0, sKey);
            sf->setValid(true);
        }
        else
        {
            sf->setValid(false);
        }
    }
    else
    {
        warningMismatch("waitForKey");
    }
}

void ScriptBuilder::slotTracking()
{
    ScriptFunction *sf = ScriptList[sb->ScriptListBox->currentRow()];

    if (sf->name() == "setTracking")
    {
        setUnsavedChanges(true);

        sf->setArg(0, (argSetTracking->CheckTrack->isChecked() ? i18n("true") : i18n("false")));
        sf->setValid(true);
    }
    else
    {
        warningMismatch("setTracking");
    }
}

void ScriptBuilder::slotViewOption()
{
    ScriptFunction *sf = ScriptList[sb->ScriptListBox->currentRow()];

    if (sf->name() == "changeViewOption")
    {
        if (argChangeViewOption->OptionName->currentIndex() >= 0 && argChangeViewOption->OptionValue->text().length())
        {
            setUnsavedChanges(true);

            sf->setArg(0, argChangeViewOption->OptionName->currentText());
            sf->setArg(1, argChangeViewOption->OptionValue->text());
            sf->setValid(true);
        }
        else
        {
            sf->setValid(false);
        }
    }
    else
    {
        warningMismatch("changeViewOption");
    }
}

void ScriptBuilder::slotChangeCity()
{
    ScriptFunction *sf = ScriptList[sb->ScriptListBox->currentRow()];

    if (sf->name() == "setGeoLocation")
    {
        QString city = argSetGeoLocation->CityName->text();

        if (city.length())
        {
            setUnsavedChanges(true);

            sf->setArg(0, city);
            if (sf->argVal(2).length())
                sf->setValid(true);
        }
        else
        {
            sf->setArg(0, QString());
            sf->setValid(false);
        }
    }
    else
    {
        warningMismatch("setGeoLocation");
    }
}

void ScriptBuilder::slotChangeProvince()
{
    ScriptFunction *sf = ScriptList[sb->ScriptListBox->currentRow()];

    if (sf->name() == "setGeoLocation")
    {
        QString province = argSetGeoLocation->ProvinceName->text();

        if (province.length())
        {
            setUnsavedChanges(true);

            sf->setArg(1, province);
            if (sf->argVal(0).length() && sf->argVal(2).length())
                sf->setValid(true);
        }
        else
        {
            sf->setArg(1, QString());
            //might not be invalid
        }
    }
    else
    {
        warningMismatch("setGeoLocation");
    }
}

void ScriptBuilder::slotChangeCountry()
{
    ScriptFunction *sf = ScriptList[sb->ScriptListBox->currentRow()];

    if (sf->name() == "setGeoLocation")
    {
        QString country = argSetGeoLocation->CountryName->text();

        if (country.length())
        {
            setUnsavedChanges(true);

            sf->setArg(2, country);
            if (sf->argVal(0).length())
                sf->setValid(true);
        }
        else
        {
            sf->setArg(2, QString());
            sf->setValid(false);
        }
    }
    else
    {
        warningMismatch("setGeoLocation");
    }
}

void ScriptBuilder::slotTimeScale()
{
    ScriptFunction *sf = ScriptList[sb->ScriptListBox->currentRow()];

    if (sf->name() == "setClockScale")
    {
        setUnsavedChanges(true);

        sf->setArg(0, QString("%1").arg(argTimeScale->TimeScale->tsbox()->timeScale()));
        sf->setValid(true);
    }
    else
    {
        warningMismatch("setClockScale");
    }
}

void ScriptBuilder::slotZoom()
{
    ScriptFunction *sf = ScriptList[sb->ScriptListBox->currentRow()];

    if (sf->name() == "zoom")
    {
        setUnsavedChanges(true);

        bool ok(false);
        argZoom->ZoomBox->text().toDouble(&ok);
        if (ok)
        {
            sf->setArg(0, argZoom->ZoomBox->text());
            sf->setValid(true);
        }
    }
    else
    {
        warningMismatch("zoom");
    }
}

void ScriptBuilder::slotExportImage()
{
    ScriptFunction *sf = ScriptList[sb->ScriptListBox->currentRow()];

    if (sf->name() == "exportImage")
    {
        setUnsavedChanges(true);

        sf->setArg(0, argExportImage->ExportFileName->url().url());
        sf->setArg(1, QString("%1").arg(argExportImage->ExportWidth->value()));
        sf->setArg(2, QString("%1").arg(argExportImage->ExportHeight->value()));
        sf->setValid(true);
    }
    else
    {
        warningMismatch("exportImage");
    }
}

void ScriptBuilder::slotPrintImage()
{
    ScriptFunction *sf = ScriptList[sb->ScriptListBox->currentRow()];

    if (sf->name() == "printImage")
    {
        setUnsavedChanges(true);

        sf->setArg(0, (argPrintImage->UsePrintDialog->isChecked() ? i18n("true") : i18n("false")));
        sf->setArg(1, (argPrintImage->UseChartColors->isChecked() ? i18n("true") : i18n("false")));
        sf->setValid(true);
    }
    else
    {
        warningMismatch("exportImage");
    }
}

void ScriptBuilder::slotChangeColorName()
{
    ScriptFunction *sf = ScriptList[sb->ScriptListBox->currentRow()];

    if (sf->name() == "setColor")
    {
        setUnsavedChanges(true);

        argSetColor->ColorValue->setColor(KStarsData::Instance()->colorScheme()->colorAt(argSetColor->ColorName->currentIndex()));
        sf->setArg(0, KStarsData::Instance()->colorScheme()->keyAt(argSetColor->ColorName->currentIndex()));
        QString cname(argSetColor->ColorValue->color().name());
        //if ( cname.at(0) == '#' ) cname = "\\" + cname; //prepend a "\" so bash doesn't think we have a comment
        sf->setArg(1, cname);
        sf->setValid(true);
    }
    else
    {
        warningMismatch("setColor");
    }
}

void ScriptBuilder::slotChangeColor()
{
    ScriptFunction *sf = ScriptList[sb->ScriptListBox->currentRow()];

    if (sf->name() == "setColor")
    {
        setUnsavedChanges(true);

        sf->setArg(0, KStarsData::Instance()->colorScheme()->keyAt(argSetColor->ColorName->currentIndex()));
        QString cname(argSetColor->ColorValue->color().name());
        //if ( cname.at(0) == '#' ) cname = "\\" + cname; //prepend a "\" so bash doesn't think we have a comment
        sf->setArg(1, cname);
        sf->setValid(true);
    }
    else
    {
        warningMismatch("setColor");
    }
}

void ScriptBuilder::slotLoadColorScheme()
{
    ScriptFunction *sf = ScriptList[sb->ScriptListBox->currentRow()];

    if (sf->name() == "loadColorScheme")
    {
        setUnsavedChanges(true);

        sf->setArg(0, '\"' + argLoadColorScheme->SchemeList->currentItem()->text() + '\"');
        sf->setValid(true);
    }
    else
    {
        warningMismatch("loadColorScheme");
    }
}

void ScriptBuilder::slotClose()
{
    saveWarning();

    if (!UnsavedChanges)
    {
        ScriptList.clear();
        sb->ScriptListBox->clear();
        sb->ArgStack->setCurrentWidget(argBlank);
        close();
    }
}

//TODO JM: INDI Scripting to be included in KDE 4.1

#if 0
void ScriptBuilder::slotINDIStartDeviceName()
{
    ScriptFunction * sf = ScriptList[ sb->ScriptListBox->currentRow() ];

    if ( sf->name() == "startINDI" )
    {
        setUnsavedChanges( true );

        sf->setArg(0, argStartINDI->deviceName->text());
        sf->setArg(1, argStartINDI->LocalButton->isChecked() ? "true" : "false");
        sf->setValid(true);
    }
    else
    {
        warningMismatch( "startINDI" );
    }

}

void ScriptBuilder::slotINDIStartDeviceMode()
{

    ScriptFunction * sf = ScriptList[ sb->ScriptListBox->currentRow() ];

    if ( sf->name() == "startINDI" )
    {
        setUnsavedChanges( true );

        sf->setArg(1, argStartINDI->LocalButton->isChecked() ? "true" : "false");
        sf->setValid(true);
    }
    else
    {
        warningMismatch( "startINDI" );
    }

}

void ScriptBuilder::slotINDISetDevice()
{

    ScriptFunction * sf = ScriptList[ sb->ScriptListBox->currentRow() ];

    if ( sf->name() == "setINDIDevice" )
    {
        setUnsavedChanges( true );

        sf->setArg(0, argSetDeviceINDI->deviceName->text());
        sf->setValid(true);
    }
    else
    {
        warningMismatch( "startINDI" );
    }
}

void ScriptBuilder::slotINDIShutdown()
{

    ScriptFunction * sf = ScriptList[ sb->ScriptListBox->currentRow() ];

    if ( sf->name() == "shutdownINDI" )
    {
        if (argShutdownINDI->deviceName->text().isEmpty())
        {
            sf->setValid(false);
            return;
        }

        if (sf->argVal(0) != argShutdownINDI->deviceName->text())
            setUnsavedChanges( true );

        sf->setArg(0, argShutdownINDI->deviceName->text());
        sf->setValid(true);
    }
    else
    {
        warningMismatch( "shutdownINDI" );
    }

}

void ScriptBuilder::slotINDISwitchDeviceConnection()
{

    ScriptFunction * sf = ScriptList[ sb->ScriptListBox->currentRow() ];

    if ( sf->name() == "switchINDI" )
    {

        if (sf->argVal(0) != (argSwitchINDI->OnButton->isChecked() ? "true" : "false"))
            setUnsavedChanges( true );

        sf->setArg(0, argSwitchINDI->OnButton->isChecked() ? "true" : "false");
        sf->setValid(true);
    }
    else
    {
        warningMismatch( "switchINDI" );
    }

}

void ScriptBuilder::slotINDISetPortDevicePort()
{
    ScriptFunction * sf = ScriptList[ sb->ScriptListBox->currentRow() ];

    if ( sf->name() == "setINDIPort" )
    {

        if (argSetPortINDI->devicePort->text().isEmpty())
        {
            sf->setValid(false);
            return;
        }

        if (sf->argVal(0) != argSetPortINDI->devicePort->text())
            setUnsavedChanges( true );

        sf->setArg(0, argSetPortINDI->devicePort->text());
        sf->setValid(true);
    }
    else
    {
        warningMismatch( "setINDIPort" );
    }

}

void ScriptBuilder::slotINDISetTargetCoordDeviceRA()
{
    ScriptFunction * sf = ScriptList[ sb->ScriptListBox->currentRow() ];

    if ( sf->name() == "setINDITargetCoord" )
    {
        //do nothing if box is blank (because we could be clearing boxes while switching argWidgets)
        if ( argSetTargetCoordINDI->RABox->text().isEmpty() )
        {
            sf->setValid(false);
            return;
        }

        bool ok(false);
        dms ra = argSetTargetCoordINDI->RABox->createDms(false, &ok);
        if ( ok )
        {

            if (sf->argVal(0) != QString( "%1" ).arg( ra.Hours() ))
                setUnsavedChanges( true );

            sf->setArg( 0, QString( "%1" ).arg( ra.Hours() ) );
            if ( ( ! sf->argVal(1).isEmpty() ))
                sf->setValid( true );
            else
                sf->setValid(false);

        }
        else
        {
            sf->setArg( 0, QString() );
            sf->setValid( false );
        }
    }
    else
    {
        warningMismatch( "setINDITargetCoord" );
    }

}

void ScriptBuilder::slotINDISetTargetCoordDeviceDEC()
{
    ScriptFunction * sf = ScriptList[ sb->ScriptListBox->currentRow() ];

    if ( sf->name() == "setINDITargetCoord" )
    {
        //do nothing if box is blank (because we could be clearing boxes while switching argWidgets)
        if ( argSetTargetCoordINDI->DecBox->text().isEmpty() )
        {
            sf->setValid(false);
            return;
        }

        bool ok(false);
        dms dec = argSetTargetCoordINDI->DecBox->createDms(true, &ok);
        if ( ok )
        {

            if (sf->argVal(1) != QString( "%1" ).arg( dec.Degrees() ))
                setUnsavedChanges( true );

            sf->setArg( 1, QString( "%1" ).arg( dec.Degrees() ) );
            if ( ( ! sf->argVal(0).isEmpty() ))
                sf->setValid( true );
            else
                sf->setValid(false);

        }
        else
        {
            sf->setArg( 1, QString() );
            sf->setValid( false );
        }
    }
    else
    {
        warningMismatch( "setINDITargetCoord" );
    }

}

void ScriptBuilder::slotINDISetTargetNameTargetName()
{

    ScriptFunction * sf = ScriptList[ sb->ScriptListBox->currentRow() ];

    if ( sf->name() == "setINDITargetName" )
    {
        if (argSetTargetNameINDI->targetName->text().isEmpty())
        {
            sf->setValid(false);
            return;
        }

        if (sf->argVal(0) != argSetTargetNameINDI->targetName->text())
            setUnsavedChanges( true );

        sf->setArg(0, argSetTargetNameINDI->targetName->text());
        sf->setValid(true);
    }
    else
    {
        warningMismatch( "setINDITargetName" );
    }

}

void ScriptBuilder::slotINDISetActionName()
{
    ScriptFunction * sf = ScriptList[ sb->ScriptListBox->currentRow() ];

    if ( sf->name() == "setINDIAction" )
    {
        if (argSetActionINDI->actionName->text().isEmpty())
        {
            sf->setValid(false);
            return;
        }

        if (sf->argVal(0) != argSetActionINDI->actionName->text())
            setUnsavedChanges( true );

        sf->setArg(0, argSetActionINDI->actionName->text());
        sf->setValid(true);
    }
    else
    {
        warningMismatch( "setINDIAction" );
    }

}

void ScriptBuilder::slotINDIWaitForActionName()
{
    ScriptFunction * sf = ScriptList[ sb->ScriptListBox->currentRow() ];

    if ( sf->name() == "waitForINDIAction" )
    {
        if (argWaitForActionINDI->actionName->text().isEmpty())
        {
            sf->setValid(false);
            return;
        }

        if (sf->argVal(0) != argWaitForActionINDI->actionName->text())
            setUnsavedChanges( true );

        sf->setArg(0, argWaitForActionINDI->actionName->text());
        sf->setValid(true);
    }
    else
    {
        warningMismatch( "waitForINDIAction" );
    }

}

void ScriptBuilder::slotINDISetFocusSpeed()
{
    ScriptFunction * sf = ScriptList[ sb->ScriptListBox->currentRow() ];

    if ( sf->name() == "setINDIFocusSpeed" )
    {

        if (sf->argVal(0).toInt() != argSetFocusSpeedINDI->speedIN->value())
            setUnsavedChanges( true );

        sf->setArg(0, QString("%1").arg(argSetFocusSpeedINDI->speedIN->value()));
        sf->setValid(true);
    }
    else
    {
        warningMismatch( "setINDIFocusSpeed" );
    }

}

void ScriptBuilder::slotINDIStartFocusDirection()
{
    ScriptFunction * sf = ScriptList[ sb->ScriptListBox->currentRow() ];

    if ( sf->name() == "startINDIFocus" )
    {
        if (sf->argVal(0) != argStartFocusINDI->directionCombo->currentText())
            setUnsavedChanges( true );

        sf->setArg(0, argStartFocusINDI->directionCombo->currentText());
        sf->setValid(true);
    }
    else
    {
        warningMismatch( "startINDIFocus" );
    }

}

void ScriptBuilder::slotINDISetFocusTimeout()
{
    ScriptFunction * sf = ScriptList[ sb->ScriptListBox->currentRow() ];

    if ( sf->name() == "setINDIFocusTimeout" )
    {
        if (sf->argVal(0).toInt() != argSetFocusTimeoutINDI->timeOut->value())
            setUnsavedChanges( true );

        sf->setArg(0, QString("%1").arg(argSetFocusTimeoutINDI->timeOut->value()));
        sf->setValid(true);
    }
    else
    {
        warningMismatch( "setINDIFocusTimeout" );
    }

}

void ScriptBuilder::slotINDISetGeoLocationDeviceLong()
{
    ScriptFunction * sf = ScriptList[ sb->ScriptListBox->currentRow() ];

    if ( sf->name() == "setINDIGeoLocation" )
    {
        //do nothing if box is blank (because we could be clearing boxes while switching argWidgets)
        if ( argSetGeoLocationINDI->longBox->text().isEmpty())
        {
            sf->setValid(false);
            return;
        }

        bool ok(false);
        dms longitude = argSetGeoLocationINDI->longBox->createDms(true, &ok);
        if ( ok )
        {

            if (sf->argVal(0) != QString( "%1" ).arg( longitude.Degrees()))
                setUnsavedChanges( true );

            sf->setArg( 0, QString( "%1" ).arg( longitude.Degrees() ) );
            if ( ! sf->argVal(1).isEmpty() )
                sf->setValid( true );
            else
                sf->setValid(false);

        }
        else
        {
            sf->setArg( 0, QString() );
            sf->setValid( false );
        }
    }
    else
    {
        warningMismatch( "setINDIGeoLocation" );
    }

}

void ScriptBuilder::slotINDISetGeoLocationDeviceLat()
{
    ScriptFunction * sf = ScriptList[ sb->ScriptListBox->currentRow() ];

    if ( sf->name() == "setINDIGeoLocation" )
    {
        //do nothing if box is blank (because we could be clearing boxes while switching argWidgets)
        if ( argSetGeoLocationINDI->latBox->text().isEmpty() )
        {
            sf->setValid(false);
            return;
        }

        bool ok(false);
        dms latitude = argSetGeoLocationINDI->latBox->createDms(true, &ok);
        if ( ok )
        {

            if (sf->argVal(1) != QString( "%1" ).arg( latitude.Degrees()))
                setUnsavedChanges( true );

            sf->setArg( 1, QString( "%1" ).arg( latitude.Degrees() ) );
            if ( ! sf->argVal(0).isEmpty() )
                sf->setValid( true );
            else
                sf->setValid(false);

        }
        else
        {
            sf->setArg( 1, QString() );
            sf->setValid( false );
        }
    }
    else
    {
        warningMismatch( "setINDIGeoLocation" );
    }

}

void ScriptBuilder::slotINDIStartExposureTimeout()
{
    ScriptFunction * sf = ScriptList[ sb->ScriptListBox->currentRow() ];

    if ( sf->name() == "startINDIExposure" )
    {

        if (sf->argVal(0).toInt() != argStartExposureINDI->timeOut->value())
            setUnsavedChanges( true );

        sf->setArg(0, QString("%1").arg(argStartExposureINDI->timeOut->value()));
        sf->setValid(true);
    }
    else
    {
        warningMismatch( "startINDIExposure" );
    }

}

void ScriptBuilder::slotINDISetUTC()
{
    ScriptFunction * sf = ScriptList[ sb->ScriptListBox->currentRow() ];

    if ( sf->name() == "setINDIUTC" )
    {

        if (argSetUTCINDI->UTC->text().isEmpty())
        {
            sf->setValid(false);
            return;
        }

        if (sf->argVal(0) != argSetUTCINDI->UTC->text())
            setUnsavedChanges( true );

        sf->setArg(0, argSetUTCINDI->UTC->text());
        sf->setValid(true);
    }
    else
    {
        warningMismatch( "setINDIUTC" );
    }

}

void ScriptBuilder::slotINDISetScopeAction()
{
    ScriptFunction * sf = ScriptList[ sb->ScriptListBox->currentRow() ];

    if ( sf->name() == "setINDIScopeAction" )
    {

        if (sf->argVal(0) != argSetScopeActionINDI->actionCombo->currentText())
            setUnsavedChanges( true );

        sf->setArg(0, argSetScopeActionINDI->actionCombo->currentText());
        sf->setINDIProperty("CHECK");
        sf->setValid(true);
    }
    else
    {
        warningMismatch( "setINDIScopeAction" );
    }

}

void ScriptBuilder::slotINDISetFrameType()
{
    ScriptFunction * sf = ScriptList[ sb->ScriptListBox->currentRow() ];

    if ( sf->name() == "setINDIFrameType" )
    {

        if (sf->argVal(0) != argSetFrameTypeINDI->typeCombo->currentText())
            setUnsavedChanges( true );

        sf->setArg(0, argSetFrameTypeINDI->typeCombo->currentText());
        sf->setValid(true);
    }
    else
    {
        warningMismatch( "setINDIFrameType" );
    }

}

void ScriptBuilder::slotINDISetCCDTemp()
{
    ScriptFunction * sf = ScriptList[ sb->ScriptListBox->currentRow() ];

    if ( sf->name() == "setINDICCDTemp" )
    {

        if (sf->argVal(0).toInt() != argSetCCDTempINDI->temp->value())
            setUnsavedChanges( true );

        sf->setArg(0, QString("%1").arg(argSetCCDTempINDI->temp->value()));
        sf->setValid(true);
    }
    else
    {
        warningMismatch( "setINDICCDTemp" );
    }

}

void ScriptBuilder::slotINDISetFilterNum()
{

    ScriptFunction * sf = ScriptList[ sb->ScriptListBox->currentRow() ];

    if ( sf->name() == "setINDIFilterNum" )
    {

        if (sf->argVal(0).toInt() != argSetFilterNumINDI->filter_num->value())
            setUnsavedChanges( true );

        sf->setArg(0, QString("%1").arg(argSetFilterNumINDI->filter_num->value()));
        sf->setValid(true);
    }
    else
    {
        warningMismatch( "setINDIFilterNum" );
    }


}
#endif

void ScriptBuilder::warningMismatch(const QString &expected) const
{
    qWarning() << i18n("Mismatch between function and Arg widget (expected %1.)", QString(expected));
}
