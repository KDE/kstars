/*  FITS Options
    Copyright (C) 2017 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

*/

#ifndef OPSFITS_H_
#define OPSFITS_H_

#include "ui_opsfits.h"

#include <QStandardItemModel>

#include <kconfigdialog.h>

class KStars;


/** @class OpsFITS
 *The FITS Tab of the Options window.  Configure FITS options including look and feel and how FITS Viewer processes the data.
 *@author Jasem Mutlaq
 *@version 1.0
 */
class OpsFITS : public QFrame, public Ui::OpsFITS
{
    Q_OBJECT

public:
    explicit OpsFITS();
};

#endif
