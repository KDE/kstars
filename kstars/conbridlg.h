/***************************************************************************
                          conbridlg.h  -  Contrast/Brightness Dialog
                             -------------------
    begin                : Fri Feb 6th 2004
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
 *  									   *
 ***************************************************************************/
 
 #ifndef CONTRASTBRIGHTNESSDLG_H
 #define CONTRASTBRIGHTNESSDLG_H
 
 #include <kdialogbase.h>
 #include <kcommand.h>
 #include <klocale.h>
 
 class ConBriForm;
 class FITSViewer;

  
class ContrastBrightnessDlg : public KDialogBase {
	Q_OBJECT
 public:
   ContrastBrightnessDlg(QWidget *parent=0);
   ~ContrastBrightnessDlg();
   
  QSize sizeHint() const;
  
  private:
  int contrast;
  int brightness;
  int height;
  int width;
  FITSViewer *viewer;
  ConBriForm *ConBriDlg;
  unsigned char *localImgBuffer;
  unsigned char *templateImgBuffer;
  
  public slots:
  void setContrast(int contrastValue);
  void setBrightness(int brightnessValue);
   
};

class conbriCommand : public KCommand
{
  public:
        conbriCommand(QWidget * parent, QImage *newIMG, QImage *oldIMG);
	~conbriCommand();
            
        void execute();
        void unexecute();
        QString name() const {
            return i18n("Brightness/Contrast");
        }

    protected:
        FITSViewer *viewer;
        QImage *newImage;
	QImage *oldImage;
};


#endif
