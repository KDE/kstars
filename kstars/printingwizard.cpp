#include "printingwizard.h"

#include "pwizprinterselection.h"

#include "QStackedWidget"
#include "kstars.h"
#include <kstandarddirs.h>

PWizWelcomeUI::PWizWelcomeUI(QWidget *parent) : QFrame(parent)
{
    setupUi(this);
}

PWizTypeSelectionUI::PWizTypeSelectionUI(QWidget *parent) : QFrame(parent)
{
    setupUi(this);
}

PrintingWizard::PrintingWizard(QWidget *parent) : KDialog(parent),
    m_KStars(KStars::Instance()), m_Printer(0)
{
    setupWidgets();
    setupConnections();
}

PrintingWizard::~PrintingWizard()
{}

void PrintingWizard::slotPrevPage()
{
    m_WizardStack->setCurrentIndex(m_WizardStack->currentIndex() - 1);
    updateButtons();
}

void PrintingWizard::slotNextPage()
{
    m_WizardStack->setCurrentIndex(m_WizardStack->currentIndex() + 1);
    updateButtons();
}

void PrintingWizard::setupWidgets()
{
    m_WizardStack = new QStackedWidget(this);
    setMainWidget(m_WizardStack);

    setCaption(i18n("Printing Wizard"));

    setButtons(KDialog::User1 | KDialog::User2 | KDialog::Cancel);

    setButtonGuiItem(KDialog::User1, KGuiItem(i18n("&Next") + QString(" >"), QString(), i18n("Go to next Wizard page")));
    setButtonGuiItem(KDialog::User2, KGuiItem(QString("< ") + i18n("&Back"), QString(), i18n("Go to previous Wizard page")));

    m_WizWelcomeUI = new PWizWelcomeUI(m_WizardStack);
    m_WizPrinterSelectionUI = new PWizPrinterSelectionUI(this, m_WizardStack);
    m_WizTypeSelectionUI = new PWizTypeSelectionUI(m_WizardStack);

    m_WizardStack->addWidget(m_WizWelcomeUI);
    m_WizardStack->addWidget(m_WizPrinterSelectionUI);
    m_WizardStack->addWidget(m_WizTypeSelectionUI);

    QPixmap bannerImg;
    if(bannerImg.load(KStandardDirs::locate("appdata", "wzstars.png")))
    {
        m_WizWelcomeUI->banner->setPixmap(bannerImg);
    }

    enableButton(KDialog::User2, false);
}

void PrintingWizard::setupConnections()
{
    connect(this, SIGNAL(user1Clicked()), this, SLOT(slotNextPage()));
    connect(this, SIGNAL(user2Clicked()), this, SLOT(slotPrevPage()));
}

void PrintingWizard::updateButtons()
{
    enableButton(KDialog::User1, m_WizardStack->currentIndex() < m_WizardStack->count() - 1);
    enableButton(KDialog::User2, m_WizardStack->currentIndex() > 0);
}


