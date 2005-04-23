/*  Image Sequence
    Capture image sequence from an imaging device.
    
    Copyright (C) 2003 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#ifndef IMAGESEQUENCE_H
#define IMAGESEQUENCE_H

#include "imgsequencedlgui.h"

class KStars;
class QTimer;
class INDIStdDevice;

class imagesequence : public imgSequenceDlg
{
  Q_OBJECT

public:
  imagesequence(QWidget* parent = 0, const char* name = 0, bool modal = FALSE, WFlags fl = 0 );
  ~imagesequence();
  

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
 void newFITS(QString deviceLabel);
 void checkCCD(int CCDNum);
 void updateFilterCombo(int filterNum);

};

#endif

