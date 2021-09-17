/*
    SPDX-FileCopyrightText: 2003 Jason Harris <kstars@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "fovdialog.h"

#include <QFile>
#include <QFrame>
#include <QPainter>
#include <QTextStream>
#include <QPaintEvent>
#include <QDebug>
#include <QPushButton>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLineEdit>

#include <KActionCollection>
#include <KLocalizedString>
#include <kcolorbutton.h>
#include <KMessageBox>

#include "kstars.h"
#include "kstarsdata.h"
#include "widgets/fovwidget.h"
#include "Options.h"

// This is needed to make FOV work with QVariant
Q_DECLARE_METATYPE(FOV *)

int FOVDialog::fovID = -1;

namespace
{
// Try to convert text in KLine edit to double
inline double textToDouble(const QLineEdit *edit, bool *ok = nullptr)
{
    return edit->text().replace(QLocale().decimalPoint(), ".").toDouble(ok);
}

// Extract FOV from QListWidget. No checking is done
FOV *getFOV(QListWidgetItem *item)
{
    return item->data(Qt::UserRole).value<FOV *>();
}

// Convert double to QString
QString toString(double x, int precision = 2)
{
    return QString::number(x, 'f', precision).replace('.', QLocale().decimalPoint());
}
}

FOVDialogUI::FOVDialogUI(QWidget *parent) : QFrame(parent)
{
    setupUi(this);
}

NewFOVUI::NewFOVUI(QWidget *parent) : QFrame(parent)
{
    setupUi(this);
}

//---------FOVDialog---------------//
FOVDialog::FOVDialog(QWidget *p) : QDialog(p)
{
#ifdef Q_OS_OSX
    setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
#endif
    // Register FOV* data type
    if (fovID == -1)
        fovID = qRegisterMetaType<FOV *>("FOV*");
    fov = new FOVDialogUI(this);

    setWindowTitle(i18nc("@title:window", "Set FOV Indicator"));

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(fov);
    setLayout(mainLayout);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Close);
    mainLayout->addWidget(buttonBox);
    connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(close()));

    connect(fov->FOVListBox, SIGNAL(currentRowChanged(int)), SLOT(slotSelect(int)));
    connect(fov->NewButton, SIGNAL(clicked()), SLOT(slotNewFOV()));
    connect(fov->EditButton, SIGNAL(clicked()), SLOT(slotEditFOV()));
    connect(fov->RemoveButton, SIGNAL(clicked()), SLOT(slotRemoveFOV()));

    // Read list of FOVs and for each FOV create listbox entry, which stores it.
    foreach (FOV *f, FOVManager::getFOVs())
    {
        addListWidget(f);
    }
}

FOVDialog::~FOVDialog()
{
    // Delete FOVs
    //for(int i = 0; i < fov->FOVListBox->count(); i++) {
    //delete getFOV( fov->FOVListBox->item(i) );
    //}
}

QListWidgetItem *FOVDialog::addListWidget(FOV *f)
{
    QListWidgetItem *item = new QListWidgetItem(f->name(), fov->FOVListBox);
    item->setData(Qt::UserRole, QVariant::fromValue<FOV *>(f));
    return item;
}

void FOVDialog::slotSelect(int irow)
{
    bool enable = irow >= 0;
    fov->RemoveButton->setEnabled(enable);
    fov->EditButton->setEnabled(enable);
    if (enable)
    {
        //paint dialog with selected FOV symbol
        fov->ViewBox->setFOV(getFOV(fov->FOVListBox->currentItem()));
        fov->ViewBox->update();
    }
}

void FOVDialog::slotNewFOV()
{
    QPointer<NewFOV> newfdlg = new NewFOV(this);
    if (newfdlg->exec() == QDialog::Accepted)
    {
        FOV *newfov = new FOV(newfdlg->getFOV());
        FOVManager::addFOV(newfov);
        addListWidget(newfov);
        fov->FOVListBox->setCurrentRow(fov->FOVListBox->count() - 1);
    }
    delete newfdlg;
}

void FOVDialog::slotEditFOV()
{
    //Preload current values
    QListWidgetItem *item = fov->FOVListBox->currentItem();
    if (item == nullptr)
        return;
    FOV *f = item->data(Qt::UserRole).value<FOV *>();

    // Create dialog
    QPointer<NewFOV> newfdlg = new NewFOV(this, f);
    if (newfdlg->exec() == QDialog::Accepted)
    {
        // Overwrite FOV
        f->sync(newfdlg->getFOV());
        fov->ViewBox->update();
    }
    delete newfdlg;
}

void FOVDialog::slotRemoveFOV()
{
    int i = fov->FOVListBox->currentRow();
    if (i >= 0)
    {
        QListWidgetItem *item = fov->FOVListBox->takeItem(i);
        FOVManager::removeFOV(getFOV(item));
        delete item;
    }
}

//-------------NewFOV------------------//

NewFOV::NewFOV(QWidget *parent, const FOV *fov) : QDialog(parent), f()
{
    ui = new NewFOVUI(this);

    setWindowTitle(i18nc("@title:window", "New FOV Indicator"));

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(ui);
    setLayout(mainLayout);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    mainLayout->addWidget(buttonBox);
    connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

    okB = buttonBox->button(QDialogButtonBox::Ok);

    // Initialize FOV if required
    if (fov != nullptr)
    {
        f.sync(*fov);
        ui->FOVName->setText(f.name());
        ui->FOVEditX->setText(toString(f.sizeX()));
        ui->FOVEditY->setText(toString(f.sizeY()));
        ui->FOVEditOffsetX->setText(toString(f.offsetX()));
        ui->FOVEditOffsetY->setText(toString(f.offsetY()));
        ui->FOVEditRotation->setText(toString(f.PA()));
        ui->ColorButton->setColor(QColor(f.color()));
        ui->ShapeBox->setCurrentIndex(f.shape());
        ui->FOVLockCP->setChecked(f.lockCelestialPole());

        ui->TLength2->setValue(Options::telescopeFocalLength());
        ui->cameraWidth->setValue(Options::cameraWidth());
        ui->cameraHeight->setValue(Options::cameraHeight());
        ui->cameraPixelSizeW->setValue(Options::cameraPixelWidth());
        ui->cameraPixelSizeH->setValue(Options::cameraPixelHeight());

        ui->ViewBox->setFOV(&f);
        ui->ViewBox->update();
    }

    connect(ui->FOVName, SIGNAL(textChanged(QString)), SLOT(slotUpdateFOV()));
    connect(ui->FOVEditX, SIGNAL(textChanged(QString)), SLOT(slotUpdateFOV()));
    connect(ui->FOVEditY, SIGNAL(textChanged(QString)), SLOT(slotUpdateFOV()));
    connect(ui->FOVEditOffsetX, SIGNAL(textChanged(QString)), SLOT(slotUpdateFOV()));
    connect(ui->FOVEditOffsetY, SIGNAL(textChanged(QString)), SLOT(slotUpdateFOV()));
    connect(ui->FOVEditRotation, SIGNAL(textChanged(QString)), SLOT(slotUpdateFOV()));
    connect(ui->FOVLockCP, SIGNAL(toggled(bool)), SLOT(slotUpdateFOV()));
    connect(ui->ColorButton, SIGNAL(changed(QColor)), SLOT(slotUpdateFOV()));
    connect(ui->ShapeBox, SIGNAL(activated(int)), SLOT(slotUpdateFOV()));
    connect(ui->ComputeEyeFOV, SIGNAL(clicked()), SLOT(slotComputeFOV()));
    connect(ui->ComputeCameraFOV, SIGNAL(clicked()), SLOT(slotComputeFOV()));
    connect(ui->ComputeHPBW, SIGNAL(clicked()), SLOT(slotComputeFOV()));
    connect(ui->ComputeBinocularFOV, SIGNAL(clicked()), SLOT(slotComputeFOV()));
    connect(ui->ComputeTLengthFromFNum1, SIGNAL(clicked()), SLOT(slotComputeTelescopeFL()));
    connect(ui->DetectFromINDI, SIGNAL(clicked()), SLOT(slotDetectFromINDI()));

#ifndef HAVE_INDI
    ui->DetectFromINDI->setEnabled(false);
#endif

    // Populate eyepiece AFOV options. The userData field contains the apparent FOV associated with that option
    ui->EyepieceAFOV->insertItem(0, i18nc("Specify the apparent field of view (AFOV) manually", "Specify AFOV"), -1);
    ui->EyepieceAFOV->addItem(i18nc("Eyepiece Design / Brand / Name; Optional", "Ramsden (Typical)"), 30);
    ui->EyepieceAFOV->addItem(i18nc("Eyepiece Design / Brand / Name; Optional", "Orthoscopic (Typical)"), 45);
    ui->EyepieceAFOV->addItem(i18nc("Eyepiece Design / Brand / Name; Optional", "Ploessl (Typical)"), 50);
    ui->EyepieceAFOV->addItem(i18nc("Eyepiece Design / Brand / Name; Optional", "Erfle (Typical)"), 60);
    ui->EyepieceAFOV->addItem(i18nc("Eyepiece Design / Brand / Name; Optional", "Tele Vue Radian"), 60);
    ui->EyepieceAFOV->addItem(i18nc("Eyepiece Design / Brand / Name; Optional", "Baader Hyperion"), 68);
    ui->EyepieceAFOV->addItem(i18nc("Eyepiece Design / Brand / Name; Optional", "Tele Vue Panoptic"), 68);
    ui->EyepieceAFOV->addItem(i18nc("Eyepiece Design / Brand / Name; Optional", "Tele Vue Delos"), 72);
    ui->EyepieceAFOV->addItem(i18nc("Eyepiece Design / Brand / Name; Optional", "Meade UWA"), 82);
    ui->EyepieceAFOV->addItem(i18nc("Eyepiece Design / Brand / Name; Optional", "Tele Vue Nagler"), 82);
    ui->EyepieceAFOV->addItem(i18nc("Eyepiece Design / Brand / Name; Optional", "Tele Vue Ethos (Typical)"), 100);

    connect(ui->EyepieceAFOV, SIGNAL(currentIndexChanged(int)), SLOT(slotEyepieceAFOVChanged(int)));

    ui->LinearFOVDistance->insertItem(0, i18n("1000 yards"));
    ui->LinearFOVDistance->insertItem(1, i18n("1000 meters"));
    connect(ui->LinearFOVDistance, SIGNAL(currentIndexChanged(int)), SLOT(slotBinocularFOVDistanceChanged(int)));

    slotUpdateFOV();
}

void NewFOV::slotBinocularFOVDistanceChanged(int index)
{
    QString text = (index == 0 ? i18n("feet") : i18n("meters"));
    ui->LabelUnits->setText(text);
}

void NewFOV::slotUpdateFOV()
{
    bool okX, okY;
    f.setName(ui->FOVName->text());
    float sizeX = textToDouble(ui->FOVEditX, &okX);
    float sizeY = textToDouble(ui->FOVEditY, &okY);
    if (okX && okY)
        f.setSize(sizeX, sizeY);

    float xoffset = textToDouble(ui->FOVEditOffsetX, &okX);
    float yoffset = textToDouble(ui->FOVEditOffsetY, &okY);
    if (okX && okY)
        f.setOffset(xoffset, yoffset);

    float rot = textToDouble(ui->FOVEditRotation, &okX);
    if (okX)
        f.setPA(rot);

    f.setShape(static_cast<FOV::Shape>(ui->ShapeBox->currentIndex()));
    f.setColor(ui->ColorButton->color().name());
    f.setLockCelestialPole(ui->FOVLockCP->isChecked());

    okB->setEnabled(!f.name().isEmpty() && okX && okY);

    ui->ViewBox->setFOV(&f);
    ui->ViewBox->update();
}

void NewFOV::slotComputeFOV()
{
    if (sender() == ui->ComputeEyeFOV && ui->TLength1->value() > 0.0)
    {
        ui->FOVEditX->setText(toString(60.0 * ui->EyeFOV->value() * ui->EyeLength->value() / ui->TLength1->value()));
        ui->FOVEditY->setText(ui->FOVEditX->text());
    }
    else if (sender() == ui->ComputeCameraFOV && ui->TLength2->value() > 0.0)
    {
        /*double sx = (double)ui->ChipWidth->value() * 3438.0 / ui->TLength2->value();
        double sy = (double)ui->ChipHeight->value() * 3438.0 / ui->TLength2->value();
        //const double aspectratio = 3.0/2.0; // Use the default aspect ratio for DSLRs / Film (i.e. 3:2)*/

        // FOV in arcmins
        double fov_x = 206264.8062470963552 * ui->cameraWidth->value() * ui->cameraPixelSizeW->value() / 60000.0 / ui->TLength2->value();
        double fov_y = 206264.8062470963552 * ui->cameraHeight->value() * ui->cameraPixelSizeH->value() / 60000.0 / ui->TLength2->value();

        ui->FOVEditX->setText(toString(fov_x));
        ui->FOVEditY->setText(toString(fov_y));
    }
    else if (sender() == ui->ComputeHPBW && ui->RTDiameter->value() > 0.0 && ui->WaveLength->value() > 0.0)
    {
        ui->FOVEditX->setText(toString(34.34 * 1.2 * ui->WaveLength->value() / ui->RTDiameter->value()));
        // Beam width for an antenna is usually a circle on the sky.
        ui->ShapeBox->setCurrentIndex(4);
        ui->FOVEditY->setText(ui->FOVEditX->text());
        slotUpdateFOV();
    }
    else if (sender() == ui->ComputeBinocularFOV && ui->LinearFOV->value() > 0.0 &&
             ui->LinearFOVDistance->currentIndex() >= 0)
    {
        double sx =
            atan((double)ui->LinearFOV->value() / ((ui->LinearFOVDistance->currentIndex() == 0) ? 3000.0 : 1000.0)) *
            180.0 * 60.0 / dms::PI;
        ui->FOVEditX->setText(toString(sx));
        ui->FOVEditY->setText(ui->FOVEditX->text());
    }
}

void NewFOV::slotEyepieceAFOVChanged(int index)
{
    if (index == 0)
    {
        ui->EyeFOV->setEnabled(true);
    }
    else
    {
        bool ok;
        ui->EyeFOV->setEnabled(false);
        ui->EyeFOV->setValue(ui->EyepieceAFOV->itemData(index).toFloat(&ok));
        Q_ASSERT(ok);
    }
}

void NewFOV::slotComputeTelescopeFL()
{
    TelescopeFL *telescopeFLDialog = new TelescopeFL(this);
    if (telescopeFLDialog->exec() == QDialog::Accepted)
    {
        ui->TLength1->setValue(telescopeFLDialog->computeFL());
    }
    delete telescopeFLDialog;
}

void NewFOV::slotDetectFromINDI()
{
    QDBusInterface alignInterface("org.kde.kstars",
                                  "/KStars/Ekos/Align",
                                  "org.kde.kstars.Ekos.Align",
                                  QDBusConnection::sessionBus());

    QDBusReply<QList<double>> cameraReply = alignInterface.call("cameraInfo");
    if (cameraReply.isValid())
    {
        QList<double> values = cameraReply.value();

        ui->cameraWidth->setValue(values[0]);
        ui->cameraHeight->setValue(values[1]);
        ui->cameraPixelSizeW->setValue(values[2]);
        ui->cameraPixelSizeH->setValue(values[3]);
    }

    QDBusReply<QList<double>> telescopeReply = alignInterface.call("telescopeInfo");
    if (telescopeReply.isValid())
    {
        QList<double> values = telescopeReply.value();
        ui->TLength2->setValue(values[0]);
    }

    QDBusReply<QList<double>> solutionReply = alignInterface.call("getSolutionResult");
    if (solutionReply.isValid())
    {
        QList<double> values = solutionReply.value();
        if (values[0] > -1e6)
            ui->FOVEditRotation->setText(QString::number(values[0]));
    }
}


//-------------TelescopeFL------------------//

TelescopeFL::TelescopeFL(QWidget *parent) : QDialog(parent), aperture(nullptr), fNumber(nullptr), apertureUnit(nullptr)
{
    setWindowTitle(i18nc("@title:window", "Telescope Focal Length Calculator"));

    //QWidget *mainWidget = new QWidget( this );
    QGridLayout *mainLayout = new QGridLayout(this);
    setLayout(mainLayout);

    aperture = new QDoubleSpinBox();
    aperture->setRange(0.0, 100000.0);
    aperture->setDecimals(2);
    aperture->setSingleStep(0.1);

    fNumber = new QDoubleSpinBox();
    fNumber->setRange(0.0, 99.9);
    fNumber->setDecimals(2);
    fNumber->setSingleStep(0.1);

    apertureUnit = new QComboBox(this);
    apertureUnit->insertItem(0, i18nc("millimeters", "mm"));
    apertureUnit->insertItem(1, i18n("inch"));

    mainLayout->addWidget(new QLabel(i18n("Aperture diameter: "), this), 0, 0);
    mainLayout->addWidget(aperture, 0, 1);
    mainLayout->addWidget(apertureUnit, 0, 2);
    mainLayout->addWidget(new QLabel(i18nc("F-Number or F-Ratio of optical system", "F-Number: "), this), 1, 0);
    mainLayout->addWidget(fNumber, 1, 1);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    mainLayout->addWidget(buttonBox);
    connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

    show();
}

double TelescopeFL::computeFL() const
{
    const double inch_to_mm = 25.4; // 1 inch, by definition, is 25.4 mm
    return (aperture->value() * fNumber->value() *
            ((apertureUnit->currentIndex() == 1) ?
             inch_to_mm :
             1.0)); // Focal Length = Aperture * F-Number, by definition of F-Number
}

unsigned int FOVDialog::currentItem() const
{
    return fov->FOVListBox->currentRow();
}
