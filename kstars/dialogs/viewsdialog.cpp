/*
    SPDX-FileCopyrightText: 2003 Jason Harris <kstars@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "viewsdialog.h"
#include <QPointer>
#include <QPainter>
#include <QMessageBox>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <kstars_debug.h>

#include "Options.h"

ViewsDialogUI::ViewsDialogUI(QWidget *parent) : QFrame(parent)
{
    setupUi(this);
}

//----ViewsDialogStringListModel-----//
Qt::ItemFlags ViewsDialogStringListModel::flags(const QModelIndex &index) const
{
    Qt::ItemFlags defaultFlags = QStringListModel::flags(index);
    if (index.isValid())
    {
        return defaultFlags & (~Qt::ItemIsDropEnabled);
    }
    return defaultFlags;
}

//---------ViewsDialog---------------//
ViewsDialog::ViewsDialog(QWidget *p) : QDialog(p)
{
#ifdef Q_OS_OSX
    setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
#endif
    ui = new ViewsDialogUI(this);

    setWindowTitle(i18nc("@title:window", "Manage Sky Map Views"));

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(ui);
    setLayout(mainLayout);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Close);
    mainLayout->addWidget(buttonBox);
    connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(close()));

    // Read list of Views and for each view, create a listbox entry
    m_model = new ViewsDialogStringListModel(this);
    syncModel();
    ui->ViewListBox->setModel(m_model);
    ui->ViewListBox->setDragDropMode(QAbstractItemView::InternalMove);
    ui->ViewListBox->setDefaultDropAction(Qt::MoveAction);
    ui->ViewListBox->setDragDropOverwriteMode(false);

    connect(ui->ViewListBox->selectionModel(), &QItemSelectionModel::currentChanged, this, &ViewsDialog::slotSelectionChanged);
    connect(m_model, &ViewsDialogStringListModel::rowsMoved, this, &ViewsDialog::syncFromModel);
    connect(ui->NewButton, SIGNAL(clicked()), SLOT(slotNewView()));
    connect(ui->EditButton, SIGNAL(clicked()), SLOT(slotEditView()));
    connect(ui->RemoveButton, SIGNAL(clicked()), SLOT(slotRemoveView()));

}

void ViewsDialog::syncModel()
{
    QStringList viewNames;
    for(const auto &view : SkyMapViewManager::getViews())
    {
        viewNames.append(view.name);
    }
    m_model->setStringList(viewNames);
}

void ViewsDialog::syncFromModel()
{
    // FIXME: Inefficient code, but it's okay because number of items is small
    QHash<QString, SkyMapView> nameToViewMap;
    for(const auto &view : SkyMapViewManager::getViews())
    {
        nameToViewMap.insert(view.name, view);
    }
    QStringList updatedList = m_model->stringList();
    SkyMapViewManager::drop();
    for (const auto &view : updatedList)
    {
        SkyMapViewManager::addView(nameToViewMap[view]);
    }
    SkyMapViewManager::save();
}

void ViewsDialog::slotSelectionChanged(const QModelIndex &current, const QModelIndex &prev)
{
    Q_UNUSED(prev);
    bool enable = current.isValid();
    ui->RemoveButton->setEnabled(enable);
    ui->EditButton->setEnabled(enable);
}

void ViewsDialog::slotNewView()
{
    QPointer<NewView> newViewDialog = new NewView(this);
    if (newViewDialog->exec() == QDialog::Accepted)
    {
        const auto view = newViewDialog->getView();
        SkyMapViewManager::addView(view);
        m_model->insertRow(m_model->rowCount());
        QModelIndex index = m_model->index(m_model->rowCount() - 1, 0);
        m_model->setData(index, view.name);
        ui->ViewListBox->setCurrentIndex(index);
    }
    delete newViewDialog;
}

void ViewsDialog::slotEditView()
{
    //Preload current values
    QModelIndex currentIndex = ui->ViewListBox->currentIndex();
    if (!currentIndex.isValid())
        return;
    const QString viewName = m_model->data(currentIndex).toString();
    std::optional<SkyMapView> view = SkyMapViewManager::viewNamed(viewName);
    Q_ASSERT(!!view);
    if (!view)
    {
        qCCritical(KSTARS) << "Programming Error";
        return; // Eh?
    }

    // Create dialog
    QPointer<NewView> newViewDialog = new NewView(this, view);
    if (newViewDialog->exec() == QDialog::Accepted)
    {
        // Overwrite Views
        SkyMapViewManager::removeView(viewName);
        const auto view = newViewDialog->getView();
        SkyMapViewManager::addView(view);
        syncModel();
    }
    delete newViewDialog;
}

void ViewsDialog::slotRemoveView()
{
    QModelIndex currentIndex = ui->ViewListBox->currentIndex();
    if (!currentIndex.isValid())
        return;
    const QString viewName = m_model->data(currentIndex).toString();
    if (SkyMapViewManager::removeView(viewName))
    {
        m_model->removeRow(currentIndex.row());
    }
}

//-------------NewViews------------------//

class SliderResetEventFilter : public QObject
{
    public:
        SliderResetEventFilter(QSlider *slider, QObject *parent = nullptr)
            : QObject(parent)
            , m_slider(slider)
        {
            if (m_slider)
            {
                m_slider->installEventFilter(this);
            }
        }

        bool eventFilter(QObject *obj, QEvent *event)
        {
            if (obj == m_slider && event->type() == QEvent::MouseButtonDblClick)
            {
                QMouseEvent *mouseEvent = dynamic_cast<QMouseEvent*>(event);
                Q_ASSERT(!!mouseEvent);
                if (mouseEvent->button() == Qt::LeftButton)
                {
                    m_slider->setValue(0);
                    return true;
                }
            }
            return QObject::eventFilter(obj, event);
        }

    private:
        QSlider *m_slider;
};

NewView::NewView(QWidget *parent, std::optional<SkyMapView> _view) : QDialog(parent)
{
    setupUi(this);

    if (_view)
    {
        setWindowTitle(i18nc("@title:window", "Edit View"));
    }
    else
    {
        setWindowTitle(i18nc("@title:window", "New View"));
    }

    fieldOfViewSpinBox->addUnit("degrees", 1.0);
    fieldOfViewSpinBox->addUnit("arcmin", 1 / 60.);
    fieldOfViewSpinBox->addUnit("arcsec", 1 / 3600.);
    fieldOfViewSpinBox->doubleSpinBox->setMaximum(600.0);
    fieldOfViewSpinBox->doubleSpinBox->setMinimum(0.01);
    fieldOfViewSpinBox->setEnabled(false);
    fieldOfViewSpinBox->doubleSpinBox->setValue(1.0);

    // Enable the "OK" button only when the "Name" field is not empty
    connect(viewNameLineEdit, &QLineEdit::textChanged, [&](const QString & text)
    {
        buttonBox->button(QDialogButtonBox::Ok)->setDisabled(text.isEmpty());
    });

    // Enable the FOV spin box and unit combo only when the Set FOV checkbox is checked
    connect(fieldOfViewCheckBox, &QCheckBox::toggled, fieldOfViewSpinBox, &UnitSpinBoxWidget::setEnabled);

    // Update the angle value and graphic when the viewing angle slider is changed
    connect(viewingAngleSlider, &QSlider::valueChanged, [&](const double value)
    {
        viewingAngleLabel->setText(QString("%1°").arg(QString::number(value)));
        this->updateViewingAnglePreviews();
    });
    viewingAngleSlider->setValue(0); // Force the updates

    // Update the viewing angle graphic when the erect observer correction is enabled / disabled
    connect(disableErectObserverCheckBox, &QCheckBox::toggled, this, &NewView::updateViewingAnglePreviews);

    // Disable erect observer when using equatorial mount
    connect(mountTypeComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), [&](const int index) {
        if (index == 0)
        {
            // Equatorial
            disableErectObserverCheckBox->setChecked(true);
            disableErectObserverCheckBox->setEnabled(false);
        }
        else
        {
            // Altazimuth
            disableErectObserverCheckBox->setEnabled(true);
        }
    });


    // Set up everything else
    m_topPreview = new QPixmap(400, 300);
    m_bottomPreview = new QPixmap(400, 300);
    m_observerPixmap = new QPixmap(":/images/observer.png");
    new SliderResetEventFilter(viewingAngleSlider);

    // Finally, initialize fields as required
    if (_view)
    {
        const auto view = *_view;
        m_originalName = view.name;
        viewNameLineEdit->setText(view.name);
        mountTypeComboBox->setCurrentIndex(view.useAltAz ? 1 : 0);
        if (view.inverted && view.mirror)
        {
            invertedMirroredViewType->setChecked(true);
        }
        else if (view.inverted)
        {
            invertedViewType->setChecked(true);
        }
        else if (view.mirror)
        {
            mirroredViewType->setChecked(true);
        }
        else
        {
            correctViewType->setChecked(true);
        }

        viewingAngleSlider->setValue(view.viewAngle);
        disableErectObserverCheckBox->setChecked(!view.erectObserver);
        if (!std::isnan(view.fov))
        {
            fieldOfViewCheckBox->setChecked(true);
            fieldOfViewSpinBox->doubleSpinBox->setValue(view.fov);
        }
    }

}

NewView::~NewView()
{
    delete m_topPreview;
    delete m_bottomPreview;
    delete m_observerPixmap;
}

const SkyMapView NewView::getView() const
{
    struct SkyMapView view;

    view.name      = viewNameLineEdit->text();
    view.useAltAz  = (mountTypeComboBox->currentIndex() > 0);
    view.viewAngle = viewingAngleSlider->value();
    view.mirror    = invertedMirroredViewType->isChecked() || mirroredViewType->isChecked();
    view.inverted  = invertedMirroredViewType->isChecked() || invertedViewType->isChecked();
    view.fov       = fieldOfViewCheckBox->isChecked() ? fieldOfViewSpinBox->value() : NaN::d;
    view.erectObserver = !(disableErectObserverCheckBox->isChecked());

    return view;
}

void NewView::done(int r)
{
    if (r == QDialog::Accepted)
    {
        const QString name = viewNameLineEdit->text();
        if (name != m_originalName)
        {
            if (!!SkyMapViewManager::viewNamed(name))
            {
                QMessageBox::critical(this, i18n("Conflicting View Name"),
                                      i18n("There already exists a view with the name you attempted to use. Please choose a different name for this view."));
                return;
            }
        }
    }
    QDialog::done(r);
    return;
}

void NewView::updateViewingAnglePreviews()
{
    Q_ASSERT(!!m_topPreview);
    Q_ASSERT(!!m_bottomPreview);
    Q_ASSERT(!!m_observerPixmap);

    QPen pen(this->palette().color(QPalette::WindowText));
    {
        m_topPreview->fill(Qt::transparent);
        float cx = m_topPreview->width() / 2., cy = m_topPreview->height() / 2.;
        float size = std::min(m_topPreview->width(), m_topPreview->height());
        float r = 0.75 * (size / 2.);
        QPainter p(m_topPreview);

        // Circle representing tube / secondary cage
        pen.setWidth(5);
        p.setPen(pen);
        p.drawEllipse(QPointF(cx, cy), r, r);

        // Cross hairs representing secondary vanes
        pen.setWidth(3);
        p.setPen(pen);
        p.drawLine(cx - r, cy, cx + r, cy);
        p.drawLine(cx, cy - r, cx, cy + r);

        // Focuser
        QPainterPathStroker stroker;
        stroker.setWidth(20.f);
        QPainterPath focuserPath;
        double theta = dms::DegToRad * (viewingAngleSlider->value() - 90);
        focuserPath.moveTo(cx + (r + 5.) * std::cos(theta), cy + (r + 5.) * std::sin(theta));
        focuserPath.lineTo(cx + (r + 25.) * std::cos(theta), cy + (r + 25.) * std::sin(theta));
        p.drawPath(stroker.createStroke(focuserPath));

        // Observer
        if (!disableErectObserverCheckBox->isChecked() && std::abs(viewingAngleSlider->value()) > 1)
        {
            p.drawPixmap(QPointF(
                             viewingAngleSlider->value() > 0 ? m_topPreview->width() - m_observerPixmap->width() : 0,
                             m_topPreview->height() - m_observerPixmap->height()),
                         viewingAngleSlider->value() < 0 ?
                         m_observerPixmap->transformed(QMatrix(-1, 0, 0, 1, 0, 0)) :
                         *m_observerPixmap);
        }
        p.end();

        // Display the pixmap to the QLabel
        viewingAnglePreviewTop->setPixmap(m_topPreview->scaled(viewingAnglePreviewTop->width(), viewingAnglePreviewTop->height(),
                                          Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }

    {
        m_bottomPreview->fill(Qt::transparent);
        float cx = m_bottomPreview->width() / 2., cy = m_bottomPreview->height() / 2.;
        float size = std::min(m_bottomPreview->width(), m_bottomPreview->height());
        float r = 0.75 * (size / 2.);
        QPainter p(m_bottomPreview);

        // Circle representing the back of an SCT
        pen.setWidth(5);
        p.setPen(pen);
        p.drawEllipse(QPointF(cx, cy), r, r);

        // Focuser
        QPainterPathStroker stroker;
        stroker.setWidth(20.f);
        QPainterPath focuserPath;
        double theta = dms::DegToRad * (-viewingAngleSlider->value() - 90);
        focuserPath.moveTo(cx, cy);
        focuserPath.lineTo(cx + 25. * std::cos(theta), cy + 25. * std::sin(theta));
        p.drawPath(stroker.createStroke(focuserPath));

        // Observer
        if (!disableErectObserverCheckBox->isChecked() && std::abs(viewingAngleSlider->value()) > 1)
        {
            p.drawPixmap(QPointF(
                             viewingAngleSlider->value() < 0 ? m_bottomPreview->width() - m_observerPixmap->width() : 0,
                             m_bottomPreview->height() - m_observerPixmap->height()),
                         viewingAngleSlider->value() > 0 ?
                         m_observerPixmap->transformed(QMatrix(-1, 0, 0, 1, 0, 0)) :
                         *m_observerPixmap);
        }

        // Display the pixmap on the QLabel
        p.end();
        viewingAnglePreviewBottom->setPixmap(m_bottomPreview->scaled(
                viewingAnglePreviewBottom->width(), viewingAnglePreviewBottom->height(),
                Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
}
