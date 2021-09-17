/*
    SPDX-FileCopyrightText: 2017 Jasem Mutlaq <mutlaqja@ikarustech.com>
    SPDX-FileCopyrightText: 2017 Robert Lancaster <rlancaste@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "stellarsolverprofileeditor.h"

#include "kstars.h"
#include "Options.h"
#include "kspaths.h"
#include "ksmessagebox.h"
#include "stellarsolverprofile.h"

#include <KConfigDialog>
#include <QInputDialog>
#include <QFileDialog>
#include <QSettings>
#include <stellarsolver.h>

namespace Ekos
{
StellarSolverProfileEditor::StellarSolverProfileEditor(QWidget *parent, ProfileGroup group,
        KConfigDialog *dialog) : QWidget(KStars::Instance())
{
    Q_UNUSED(parent);
    setupUi(this);

    //Get a pointer to the KConfigDialog
    m_ConfigDialog = dialog;

    setProfileGroup(group);
    optionsProfileGroup->setEnabled(false);

    /* // we may want to do this eventually
    connect(optionsProfileGroup, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index){
        setProfileGroup((ProfileGroup)index);
    });
    */

    openProfile->setIcon(
        QIcon::fromTheme("document-open"));
    openProfile->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    connect(openProfile, &QPushButton::clicked, this, &StellarSolverProfileEditor::openSingleProfile);

    saveProfile->setIcon(
        QIcon::fromTheme("document-save"));
    saveProfile->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    connect(saveProfile, &QPushButton::clicked, this, &StellarSolverProfileEditor::saveSingleProfile);

    /*  This is not implemented yet.
    copyProfile->setIcon(
        QIcon::fromTheme("edit-copy"));
    copyProfile->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    */
    copyProfile->setVisible(false);  //until we enable it.

    saveBackups->setIcon(
        QIcon::fromTheme("document-save"));
    saveBackups->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    connect(saveBackups, &QPushButton::clicked, this, &StellarSolverProfileEditor::saveBackupProfiles);

    loadBackups->setIcon(
        QIcon::fromTheme("document-open"));
    loadBackups->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    connect(loadBackups, &QPushButton::clicked, this, &StellarSolverProfileEditor::openBackupProfiles);

    reloadProfiles->setIcon(
        QIcon::fromTheme("system-reboot"));
    reloadProfiles->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    connect(reloadProfiles, &QPushButton::clicked, this, &StellarSolverProfileEditor::loadProfiles);

    loadDefaults->setIcon(
        QIcon::fromTheme("go-down"));
    loadDefaults->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    connect(loadDefaults, &QPushButton::clicked, this, &StellarSolverProfileEditor::loadDefaultProfiles);

    connect(addOptionProfile, &QAbstractButton::clicked, this, [this]()
    {
        bool ok;
        QString name = QInputDialog::getText(this, tr("New Options Profile"),
                                             tr("What would you like your profile to be called?"), QLineEdit::Normal,
                                             "", &ok);
        if (ok && !name.isEmpty())
        {
            disconnectOptionsProfileComboBox();
            SSolver::Parameters params = getSettingsFromUI();
            params.listName = name;
            optionsList.append(params);
            optionsProfile->addItem(name);
            optionsProfile->setCurrentText(name);
            openOptionsProfileNum = optionsProfile->count() - 1;
            settingJustChanged();
            connectOptionsProfileComboBox();
        }
    });

    connect(removeOptionProfile, &QAbstractButton::clicked, this, [this]()
    {
        if(optionsList.count() == 0)
            return;
        int item = optionsProfile->currentIndex();
        disconnectOptionsProfileComboBox();
        optionsProfile->removeItem(item);
        optionsList.removeAt(item);
        if(optionsProfile->count() > 0)
        {
            if(item > optionsList.count() - 1) //So that it doesn't try to set the new list to a nonexistant one
                item--;
            openOptionsProfileNum = item;
            loadOptionsProfileIgnoreOldSettings(item); //Because the old one no longer exists
        }
        settingJustChanged();
        connectOptionsProfileComboBox();
    });

    connect(description, &QTextEdit::textChanged, this, &StellarSolverProfileEditor::settingJustChanged);

    QList<QLineEdit *> lines = this->findChildren<QLineEdit *>();
    for(QLineEdit *line : lines)
        connect(line, &QLineEdit::textEdited, this, &StellarSolverProfileEditor::settingJustChanged);

    QList<QCheckBox *> checks = this->findChildren<QCheckBox *>();
    for(QCheckBox *check : checks)
        connect(check, &QCheckBox::stateChanged, this, &StellarSolverProfileEditor::settingJustChanged);

    QList<QComboBox *> combos = this->findChildren<QComboBox *>();
    for(QComboBox *combo : combos)
    {
        if(combo != optionsProfile)
            connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &StellarSolverProfileEditor::settingJustChanged);
    }

    QList<QSpinBox *> spins = this->findChildren<QSpinBox *>();
    for(QSpinBox *spin : spins)
        connect(spin, QOverload<int>::of(&QSpinBox::valueChanged), this, &StellarSolverProfileEditor::settingJustChanged);

    if(selectedProfileGroup != AlignProfiles)
        astrometryOptions->setVisible(false);
    connect(m_ConfigDialog->button(QDialogButtonBox::Apply), SIGNAL(clicked()), SLOT(slotApply()));
    connect(m_ConfigDialog->button(QDialogButtonBox::Ok), SIGNAL(clicked()), SLOT(slotApply()));
}

void StellarSolverProfileEditor::setProfileGroup(ProfileGroup group)
{
    selectedProfileGroup = group;
    optionsProfileGroup->setCurrentIndex(static_cast<int>(group));
    QString profileGroupFileName;
    switch(selectedProfileGroup)
    {
        case AlignProfiles:
            profileGroupFileName = "SavedAlignProfiles.ini";
            break;
        case FocusProfiles:
            profileGroupFileName = "SavedFocusProfiles.ini";
            break;
        case GuideProfiles:
            profileGroupFileName = "SavedGuideProfiles.ini";
            break;
        case HFRProfiles:
            profileGroupFileName = "SavedHFRProfiles.ini";
            break;
    }

    savedOptionsProfiles = QDir(KSPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath(profileGroupFileName);
    loadProfiles();
}

void StellarSolverProfileEditor::connectOptionsProfileComboBox()
{
    connect(optionsProfile, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            &StellarSolverProfileEditor::loadOptionsProfile);
}

void StellarSolverProfileEditor::disconnectOptionsProfileComboBox()
{
    disconnect(optionsProfile, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
               &StellarSolverProfileEditor::loadOptionsProfile);
}

void StellarSolverProfileEditor::settingJustChanged()
{
    optionsAreSaved = false;
    m_ConfigDialog->button(QDialogButtonBox::Apply)->setEnabled(true);
}

void StellarSolverProfileEditor::loadProfile(int profile)
{
    optionsProfile->setCurrentIndex(profile);
}

void StellarSolverProfileEditor::loadOptionsProfile()
{
    if(optionsProfile->count() == 0)
        return;
    if(openOptionsProfileNum < optionsList.count() && openOptionsProfileNum > 0)
    {
        SSolver::Parameters editorOptions = getSettingsFromUI();
        SSolver::Parameters currentProfile = optionsList.at(openOptionsProfileNum);
        if(!(editorOptions == currentProfile) || editorOptions.description != currentProfile.description)
        {
            editorOptions.listName = currentProfile.listName;
            optionsList.replace(openOptionsProfileNum, editorOptions);
        }
    }
    SSolver::Parameters newProfile = optionsList.at(optionsProfile->currentIndex());
    QList<QWidget *> controls = this->findChildren<QWidget *>();
    for(QWidget *control : controls)
        control->blockSignals(true);
    sendSettingsToUI(newProfile);
    for(QWidget *control : controls)
        control->blockSignals(false);
    openOptionsProfileNum = optionsProfile->currentIndex();
}

void StellarSolverProfileEditor::loadOptionsProfileIgnoreOldSettings(int index)
{
    if(index >= optionsProfile->count())
        return;
    SSolver::Parameters newProfile = optionsList.at(index);
    QList<QWidget *> controls = this->findChildren<QWidget *>();
    for(QWidget *control : controls)
        control->blockSignals(true);
    sendSettingsToUI(newProfile);
    optionsProfile->setCurrentIndex(index);
    openOptionsProfileNum = optionsProfile->currentIndex();
    for(QWidget *control : controls)
        control->blockSignals(false);
}

//This sets all the settings for either the internal or external sextractor
//based on the requested settings in the mainwindow interface.
//If you are implementing the StellarSolver Library in your progra, you may choose to change some or all of these settings or use the defaults.
SSolver::Parameters StellarSolverProfileEditor::getSettingsFromUI()
{
    SSolver::Parameters params;
    params.description = description->toPlainText();
    //These are to pass the parameters to the internal sextractor
    params.apertureShape = (SSolver::Shape) apertureShape->currentIndex();
    params.kron_fact = kron_fact->text().toDouble();
    params.subpix = subpix->text().toInt() ;
    params.r_min = r_min->text().toFloat();
    //params.inflags
    params.magzero = magzero->text().toFloat();
    params.minarea = minarea->text().toFloat();
    params.deblend_thresh = deblend_thresh->text().toInt();
    params.deblend_contrast = deblend_contrast->text().toFloat();
    params.clean = (cleanCheckBox->isChecked()) ? 1 : 0;
    params.clean_param = clean_param->text().toDouble();
    StellarSolver::createConvFilterFromFWHM(&params, fwhm->value());

    //Star Filter Settings
    params.resort = resort->isChecked();
    params.maxSize = maxSize->text().toDouble();
    params.minSize = minSize->text().toDouble();
    params.maxEllipse = maxEllipse->text().toDouble();
    params.initialKeep = initialKeep->text().toInt();
    params.keepNum = keepNum->text().toInt();
    params.removeBrightest = brightestPercent->text().toDouble();
    params.removeDimmest = dimmestPercent->text().toDouble();
    params.saturationLimit = saturationLimit->text().toDouble();

    //Settings that usually get set by the config file

    params.maxwidth = maxWidth->text().toDouble();
    params.minwidth = minWidth->text().toDouble();
    params.inParallel = inParallel->isChecked();
    params.multiAlgorithm = (SSolver::MultiAlgo)multiAlgo->currentIndex();
    params.solverTimeLimit = solverTimeLimit->text().toInt();

    params.resort = resort->isChecked();
    params.autoDownsample = autoDownsample->isChecked();
    params.downsample = downsample->value();
    params.search_radius = radius->text().toDouble();

    return params;
}

void StellarSolverProfileEditor::sendSettingsToUI(SSolver::Parameters a)
{

    description->setText(a.description);
    //Sextractor Settings

    apertureShape->setCurrentIndex(a.apertureShape);
    kron_fact->setText(QString::number(a.kron_fact));
    subpix->setText(QString::number(a.subpix));
    r_min->setText(QString::number(a.r_min));

    magzero->setText(QString::number(a.magzero));
    minarea->setText(QString::number(a.minarea));
    deblend_thresh->setText(QString::number(a.deblend_thresh));
    deblend_contrast->setText(QString::number(a.deblend_contrast));
    cleanCheckBox->setChecked(a.clean == 1);
    clean_param->setText(QString::number(a.clean_param));
    fwhm->setValue(a.fwhm);

    //Star Filter Settings

    maxSize->setText(QString::number(a.maxSize));
    minSize->setText(QString::number(a.minSize));
    maxEllipse->setText(QString::number(a.maxEllipse));
    initialKeep->setText(QString::number(a.initialKeep));
    keepNum->setText(QString::number(a.keepNum));
    brightestPercent->setText(QString::number(a.removeBrightest));
    dimmestPercent->setText(QString::number(a.removeDimmest));
    saturationLimit->setText(QString::number(a.saturationLimit));

    //Astrometry Settings

    autoDownsample->setChecked(a.autoDownsample);
    downsample->setValue(a.downsample);
    inParallel->setChecked(a.inParallel);
    multiAlgo->setCurrentIndex(a.multiAlgorithm);
    solverTimeLimit->setText(QString::number(a.solverTimeLimit));
    minWidth->setText(QString::number(a.minwidth));
    maxWidth->setText(QString::number(a.maxwidth));
    radius->setText(QString::number(a.search_radius));
    resort->setChecked(a.resort);
}

void StellarSolverProfileEditor::copySingleProfile()
{

}

void StellarSolverProfileEditor::openSingleProfile()
{
    QString fileURL = QFileDialog::getOpenFileName(nullptr, "Load Options Profiles File", dirPath,
                      "INI files(*.ini)");
    if (fileURL.isEmpty())
        return;
    if(!QFileInfo(fileURL).exists())
    {
        KSMessageBox::warning(this, "Message", "The file doesn't exist");
        return;
    }
    QSettings settings(fileURL, QSettings::IniFormat);
    QStringList groups = settings.childGroups();
    for(QString group : groups)
    {
        settings.beginGroup(group);
        QStringList keys = settings.childKeys();
        QMap<QString, QVariant> map;
        for(QString key : keys)
            map.insert(key, settings.value(key));
        SSolver::Parameters newParams = SSolver::Parameters::convertFromMap(map);
        optionsList.append(newParams);
        optionsProfile->addItem(group);
    }
    if(optionsProfile->count() > 0)
        loadProfile(optionsProfile->count() - 1);
    m_ConfigDialog->button(QDialogButtonBox::Apply)->setEnabled(true);
}

void StellarSolverProfileEditor::loadProfiles()
{
    if( !optionsAreSaved )
    {
        if(QMessageBox::question(this, "Abort?",
                                 "You made unsaved changes in the settings, do you really wish to overwrite them?") == QMessageBox::No)
        {
            return;
        }
        optionsAreSaved = true; //They just got overwritten
    }
    disconnectOptionsProfileComboBox();
    optionsProfile->clear();
    if(QFile(savedOptionsProfiles).exists())
        optionsList = StellarSolver::loadSavedOptionsProfiles(savedOptionsProfiles);
    else
        optionsList = getDefaultProfiles();
    for(SSolver::Parameters params : optionsList)
        optionsProfile->addItem(params.listName);
    if(optionsList.count() > 0)
    {
        sendSettingsToUI(optionsList.at(0));
        openOptionsProfileNum = 0;
    }
    connectOptionsProfileComboBox();
    m_ConfigDialog->button(QDialogButtonBox::Apply)->setEnabled(false);
}

void StellarSolverProfileEditor::saveSingleProfile()
{
    QString fileURL = QFileDialog::getSaveFileName(nullptr, "Save Options Profiles", dirPath,
                      "INI files(*.ini)");
    if (fileURL.isEmpty())
        return;
    QSettings settings(fileURL, QSettings::IniFormat);
    SSolver::Parameters params = optionsList.at(optionsProfile->currentIndex());
    settings.beginGroup(params.listName);
    QMap<QString, QVariant> map = SSolver::Parameters::convertToMap(params);
    QMapIterator<QString, QVariant> it(map);
    while(it.hasNext())
    {
        it.next();
        settings.setValue(it.key(), it.value());
    }
    settings.endGroup();
}

void StellarSolverProfileEditor::saveProfiles()
{
    QSettings settings(savedOptionsProfiles, QSettings::IniFormat);
    for(int i = 0 ; i < optionsList.count(); i++)
    {
        SSolver::Parameters params = optionsList.at(i);
        settings.beginGroup(params.listName);
        QMap<QString, QVariant> map = SSolver::Parameters::convertToMap(params);
        QMapIterator<QString, QVariant> it(map);
        while(it.hasNext())
        {
            it.next();
            settings.setValue(it.key(), it.value());
        }
        settings.endGroup();
    }
    QStringList groups = settings.childGroups();
    for(QString group : groups)
    {
        bool groupInList = false;
        for(SSolver::Parameters params : optionsList)
        {
            if(params.listName == group)
                groupInList = true;
        }
        if( ! groupInList)
            settings.remove(group);
    }
}

QList<SSolver::Parameters> StellarSolverProfileEditor::getDefaultProfiles()
{
    switch(selectedProfileGroup)
    {
        case FocusProfiles:
            return getDefaultFocusOptionsProfiles();
        case HFRProfiles:
            return getDefaultHFROptionsProfiles();
        case GuideProfiles:
            return getDefaultGuideOptionsProfiles();
        default:
        case AlignProfiles:
            return getDefaultAlignOptionsProfiles();
    }
}

void StellarSolverProfileEditor::loadDefaultProfiles()
{
    if( !optionsAreSaved )
    {
        if(QMessageBox::question(this, "Abort?",
                                 "You made unsaved changes in the settings, do you really wish to overwrite them?") == QMessageBox::No)
        {
            return;
        }
        optionsAreSaved = true; //They just got overwritten
    }
    disconnectOptionsProfileComboBox();
    optionsList = getDefaultProfiles();
    optionsProfile->clear();
    for(SSolver::Parameters param : optionsList)
        optionsProfile->addItem(param.listName);
    connectOptionsProfileComboBox();
    if(optionsProfile->count() > 0)
        loadOptionsProfileIgnoreOldSettings(0);
    m_ConfigDialog->button(QDialogButtonBox::Apply)->setEnabled(true);
}

void StellarSolverProfileEditor::saveBackupProfiles()
{
    QString fileURL = QFileDialog::getSaveFileName(nullptr, "Save Options Profiles", dirPath,
                      "INI files(*.ini)");
    if (fileURL.isEmpty())
        return;
    QSettings settings(fileURL, QSettings::IniFormat);
    for(int i = 0 ; i < optionsList.count(); i++)
    {
        SSolver::Parameters params = optionsList.at(i);
        settings.beginGroup(params.listName);
        QMap<QString, QVariant> map = SSolver::Parameters::convertToMap(params);
        QMapIterator<QString, QVariant> it(map);
        while(it.hasNext())
        {
            it.next();
            settings.setValue(it.key(), it.value());
        }
        settings.endGroup();
    }
}

void StellarSolverProfileEditor::openBackupProfiles()
{
    QString fileURL = QFileDialog::getOpenFileName(nullptr, "Load Options Profiles File", dirPath,
                      "INI files(*.ini)");
    if (fileURL.isEmpty())
        return;
    if(!QFileInfo(fileURL).exists())
    {
        QMessageBox::warning(this, "Message", "The file doesn't exist");
        return;
    }
    disconnectOptionsProfileComboBox();
    optionsList.clear();
    QSettings settings(fileURL, QSettings::IniFormat);
    QStringList groups = settings.childGroups();
    for(QString group : groups)
    {
        settings.beginGroup(group);
        QStringList keys = settings.childKeys();
        QMap<QString, QVariant> map;
        for(QString key : keys)
            map.insert(key, settings.value(key));
        SSolver::Parameters newParams = SSolver::Parameters::convertFromMap(map);
        optionsList.append(newParams);
        optionsProfile->addItem(group);
    }
    if(optionsProfile->count() > 0)
        loadOptionsProfileIgnoreOldSettings(0);
    connectOptionsProfileComboBox();
    m_ConfigDialog->button(QDialogButtonBox::Apply)->setEnabled(true);
}

void StellarSolverProfileEditor::slotApply()
{
    int index = optionsProfile->currentIndex();
    SSolver::Parameters currentParams = getSettingsFromUI();
    SSolver::Parameters savedParams = optionsList.at(index);
    if(!(currentParams == savedParams) || currentParams.description != savedParams.description)
    {
        currentParams.listName = savedParams.listName;
        optionsList.replace(index, currentParams);
    }
    optionsAreSaved = true;

    saveProfiles();
    emit optionsProfilesUpdated();
}

}
