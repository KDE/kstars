#ifndef FOCUS_H
#define FOCUS_H

#include "focus.h"
#include "capture.h"

#include "ui_focus.h"


#include "indi/indistd.h"

namespace Ekos
{


class Focus : public QWidget, public Ui::Focus
{

    Q_OBJECT

public:
    Focus();


    void addFocuser(ISD::GDInterface *newFocuser);
    void addCCD(ISD::GDInterface *newCCD);

    typedef enum { FOCUS_NONE, FOCUS_IN, FOCUS_OUT } FocusType;

public slots:

    /* Focus */
    void startFocus();
    void stopFocus();
    void capture();

    void FocusIn(int ms=1000);
    void FocusOut(int ms=1000);

    void toggleAutofocus(bool enable);

    void newFITS(IBLOB *bp);

private:

    /* Focus */
    ISD::Focuser *currentFocuser;
    ISD::CCD *currentCCD;

    Ekos::Capture *captureP;

    FocusType lastFocusDirection;

    double HFR;

};

}

#endif  // Focus_H
