/***************************************************************************
                          imagereductiondlg.h  -  Image reduction utility
                          -------------------
    begin                : Tue Feb 24 2004
    copyright            : (C) 2004 by Jasem Mutlaq
    email                : mutlaqja@ikarustech.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

 #ifndef IMAGEREDUCTIONDLG_H
 #define IMAGEREDUCTIONDLG_H

 #include <QDialog>
 #include "imagereductionui.h"

 class ImageReductionUI : public QDialog , public Ui::imageReductionUI
 {
   Q_OBJECT
   public:
        ImageReductionUI(QDialog *parent);
  };

 class ImageReductionDlg : public QDialog
 {
   Q_OBJECT

    public:
     ImageReductionDlg(QWidget * parent, const char * name = 0);
     ~ImageReductionDlg();


     ImageReductionUI * ui;

     public slots:
     void addDarkFile();
     void addFlatFile();
     void addDarkFlatFile();
     void removeDarkFile();
     void removeFlatFile();
     void removeDarkFlatFile();
     void detailsDarkFile();
     void detailsFlatFile();
     void detailsDarkFlatFile();

};

#endif

