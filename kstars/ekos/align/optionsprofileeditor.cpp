/*  Astrometry.net Options Editor
    Copyright (C) 2017 Jasem Mutlaq <mutlaqja@ikarustech.com>
    Copyright (C) 2017 Robert Lancaster <rlancaste@gmail.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#include "optionsprofileeditor.h"

#include "align.h"
#include "kstars.h"
#include "Options.h"
#include "kspaths.h"
#include "ksutils.h"

#include <KConfigDialog>

namespace Ekos
{
OptionsProfileEditor::OptionsProfileEditor(Align *parent) : QWidget(KStars::Instance())
{
    setupUi(this);

    alignModule = parent;

    //Get a pointer to the KConfigDialog
    m_ConfigDialog = KConfigDialog::exists("alignsettings");

    savedOptionsProfiles = KSPaths::writableLocation(QStandardPaths::GenericDataLocation) + QString("SavedOptionsProfiles.ini");
    loadProfiles();

    connect(addOptionProfile,&QAbstractButton::clicked, this, [this](){
        bool ok;
        QString name = QInputDialog::getText(this, tr("New Options Profile"),
                              tr("What would you like your profile to be called?"), QLineEdit::Normal,
                              "", &ok);
        if (ok && !name.isEmpty())
        {
            SSolver::Parameters params = getSettingsFromUI();
            params.listName = name;
            optionsList.append(params);
            optionsProfile->addItem(name);
            optionsProfile->setCurrentText(name);
        }
    });

    connect(removeOptionProfile,&QAbstractButton::clicked, this, [this](){
        int item = optionsProfile->currentIndex();
        optionsProfile->removeItem(item);
        optionsList.removeAt(item);

    });

    connect(loadBackups, &QPushButton::clicked, this, &OptionsProfileEditor::loadBackupProfiles);
    connect(loadDefaults, &QPushButton::clicked, this, &OptionsProfileEditor::loadDefaultProfiles);
    connect(reloadProfiles, &QPushButton::clicked, this, &OptionsProfileEditor::loadProfiles);
    connect(saveBackups, &QPushButton::clicked, this, &OptionsProfileEditor::saveBackupProfiles);

    QList<QLineEdit *> lines = this->findChildren<QLineEdit *>();
    foreach(QLineEdit *line, lines)
        connect(line, &QLineEdit::textEdited, this, &OptionsProfileEditor::settingJustChanged);

    QList<QCheckBox *> checks = this->findChildren<QCheckBox *>();
    foreach(QCheckBox *check, checks)
        connect(check, &QCheckBox::stateChanged, this, &OptionsProfileEditor::settingJustChanged);

    QList<QComboBox *> combos = this->findChildren<QComboBox *>();
    foreach(QComboBox *combo, combos)
    {
        if(combo != optionsProfile)
            connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &OptionsProfileEditor::settingJustChanged);
    }

    QList<QSpinBox *> spins = this->findChildren<QSpinBox *>();
    foreach(QSpinBox *spin, spins)
        connect(spin, QOverload<int>::of(&QSpinBox::valueChanged), this, &OptionsProfileEditor::settingJustChanged);

    connect(m_ConfigDialog->button(QDialogButtonBox::Apply), SIGNAL(clicked()), SLOT(slotApply()));
    connect(m_ConfigDialog->button(QDialogButtonBox::Ok), SIGNAL(clicked()), SLOT(slotApply()));
}

void OptionsProfileEditor::connectOptionsProfileComboBox()
{
    connect(optionsProfile, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &OptionsProfileEditor::loadOptionsProfile);
    if(optionsProfile->count()>0)
        optionsProfile->setCurrentIndex(0);
}

void OptionsProfileEditor::disconnectOptionsProfileComboBox()
{
    disconnect(optionsProfile, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &OptionsProfileEditor::loadOptionsProfile);
}

void OptionsProfileEditor::settingJustChanged()
{
    optionsAreSaved = false;
}

void OptionsProfileEditor::loadProfile(int profile)
{
    optionsProfile->setCurrentIndex(profile);
}

void OptionsProfileEditor::loadOptionsProfile()
{

    SSolver::Parameters oldOptions = getSettingsFromUI();

    if( !optionsAreSaved )
    {
        if(QMessageBox::question(this, "Abort?", "You made unsaved changes in the settings, do you really wish to overwrite them?") == QMessageBox::No)
        {
            return;
        }
        optionsAreSaved = true; //They just got overwritten
    }

    SSolver::Parameters newOptions;
    newOptions = optionsList.at(optionsProfile->currentIndex());
    QList<QWidget *> controls = this->findChildren<QWidget *>();
    foreach(QWidget *control, controls)
        control->blockSignals(true);
    sendSettingsToUI(newOptions);
    foreach(QWidget *control, controls)
        control->blockSignals(false);
}

//This sets all the settings for either the internal or external sextractor
//based on the requested settings in the mainwindow interface.
//If you are implementing the StellarSolver Library in your progra, you may choose to change some or all of these settings or use the defaults.
SSolver::Parameters OptionsProfileEditor::getSettingsFromUI()
{
    SSolver::Parameters params;
    params.listName = "Custom";
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
    StellarSolver::createConvFilterFromFWHM(&params, fwhm->text().toDouble());

    //Star Filter Settings
    params.resort = resort->isChecked();
    params.maxSize = maxSize->text().toDouble();
    params.minSize = minSize->text().toDouble();
    params.maxEllipse = maxEllipse->text().toDouble();
    params.keepNum = keepNum->text().toDouble();
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
    params.downsample = downsample->value();
    params.search_radius = radius->text().toDouble();

    return params;
}

void OptionsProfileEditor::sendSettingsToUI(SSolver::Parameters a)
{
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
        fwhm->setText(QString::number(a.fwhm));

    //Star Filter Settings

        maxSize->setText(QString::number(a.maxSize));
        minSize->setText(QString::number(a.minSize));
        maxEllipse->setText(QString::number(a.maxEllipse));
        keepNum->setText(QString::number(a.keepNum));
        brightestPercent->setText(QString::number(a.removeBrightest));
        dimmestPercent->setText(QString::number(a.removeDimmest));
        saturationLimit->setText(QString::number(a.saturationLimit));

    //Astrometry Settings

        downsample->setValue(a.downsample);
        inParallel->setChecked(a.inParallel);
        multiAlgo->setCurrentIndex(a.multiAlgorithm);
        solverTimeLimit->setText(QString::number(a.solverTimeLimit));
        minWidth->setText(QString::number(a.minwidth));
        maxWidth->setText(QString::number(a.maxwidth));
        radius->setText(QString::number(a.search_radius));
        resort->setChecked(a.resort);
}

void OptionsProfileEditor::loadProfiles()
{
    disconnectOptionsProfileComboBox();
    optionsProfile->clear();
    optionsList = StellarSolver::loadSavedOptionsProfiles(savedOptionsProfiles);
    foreach(SSolver::Parameters params, optionsList)
        optionsProfile->addItem(params.listName);
    connectOptionsProfileComboBox();
}

void OptionsProfileEditor::saveProfiles()
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
    foreach(QString group, groups)
    {
        bool groupInList = false;
        foreach(SSolver::Parameters params, optionsList)
        {
           if(params.listName == group)
               groupInList = true;
        }
        if( ! groupInList)
            settings.remove(group);
    }
}

void OptionsProfileEditor::loadDefaultProfiles()
{
    disconnectOptionsProfileComboBox();
    optionsList = StellarSolver::getBuiltInProfiles();
    optionsProfile->clear();
    foreach(SSolver::Parameters param, optionsList)
        optionsProfile->addItem(param.listName);
    connectOptionsProfileComboBox();
}

void OptionsProfileEditor::saveBackupProfiles()
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

void OptionsProfileEditor::loadBackupProfiles()
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
    foreach(QString group, groups)
    {
        settings.beginGroup(group);
        QStringList keys = settings.childKeys();
        QMap<QString, QVariant> map;
        foreach(QString key, keys)
            map.insert(key, settings.value(key));
        SSolver::Parameters newParams = SSolver::Parameters::convertFromMap(map);
        foreach(SSolver::Parameters params, optionsList)
        {
            optionsList.append(newParams);
            optionsProfile->addItem(group);
        }
    }
    connectOptionsProfileComboBox();
}

void OptionsProfileEditor::slotApply()
{
    int index = optionsProfile->currentIndex();
    SSolver::Parameters currentParams = getSettingsFromUI();
    SSolver::Parameters savedParams = optionsList.at(index);
    if(!(currentParams == savedParams))
    {
        currentParams.listName = savedParams.listName;
        optionsList.replace(index, currentParams);
    }
    optionsAreSaved = true;

    saveProfiles();
    emit optionsProfilesUpdated();
}

}
