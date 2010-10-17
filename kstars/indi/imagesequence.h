/*  Image Sequence
    Capture image sequence from an imaging device.
    
    Copyright (C) 2003 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#ifndef IMAGESEQUENCE_H_
#define IMAGESEQUENCE_H_

#include <QFrame>
#include <QDialog>

#include "ui_imgsequencedlg.h"

class KStars;
class QTimer;
class INDIStdDevice;

class imagesequence : public QDialog, public Ui::imgSequence
{
    Q_OBJECT

public:
    imagesequence(QWidget* parent = 0 );
    virtual ~imagesequence();

    bool updateStatus();

private:
    KStars *ksw;
    QTimer *seqTimer;
    INDIStdDevice *stdDevCCD;
    INDIStdDevice *stdDevFilter;

    bool	active;
    bool	ISOStamp;
    double	seqExpose;
    int	seqTotalCount;
    int	seqCurrentCount;
    int	seqDelay;
    int     retries;
    int     lastCCD;
    int     lastFilter;
    QString currentCCD;
    QString currentFilter;

    bool	verifyCCDIntegrity();
    bool    verifyFilterIntegrity();
    void    resetButtons();
    void    selectFilter();

public slots:
    bool setupCCDs();
    bool setupFilters();
    void newCCD();
    void newFilter();

    void startSequence();
    void stopSequence();
    void captureImage();
    void prepareCapture();
    void newFITS(const QString &deviceLabel);
    void checkCCD(int CCDNum);
    void updateFilterCombo(int filterNum);

};

#endif

