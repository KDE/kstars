/*  Ekos Port Selector
    Copyright (C) 2021 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#pragma once

#include <QDialog>
#include <QStandardItemModel>
#include <QJsonObject>

#include <memory>

#include "indi/indistd.h"
#include "profileinfo.h"

class PortSelector : public QDialog
{
public:
    explicit PortSelector(QWidget *parent = nullptr);

};
