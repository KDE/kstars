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
 #include <klocale.h>
 
 class ConBriForm;
 class FITSViewer;
 class QImage;

  
class ContrastBrightnessDlg : public KDialogBase {
	Q_OBJECT
 public:
   ContrastBrightnessDlg(QWidget *parent=0);
   ~ContrastBrightnessDlg();
   
  QSize sizeHint() const;
  void range(int min, int max, int & num);
  void range(float min, float max, float & num);
  
  float *localImgBuffer;
  float offs;
  float scale;
  
  private:
  int contrast;
  int brightness;
  int height;
  int width;
  FITSViewer *viewer;
  ConBriForm *ConBriDlg;
  //unsigned char *localImgBuffer;
  
  QImage *displayImage;
  QImage *tempImage;
  
  public slots:
  void setContrast(int contrastValue);
  void setBrightness(int brightnessValue);
   
};


#endif
