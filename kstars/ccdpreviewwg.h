/*  CCD Preview
    Copyright (C) 2005 Dirk Huenniger <hunniger@cip.physik.uni-bonn.de>

    Adapted from streamwg by Jasem Mutlaq

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
    
 */
 
 #ifndef CCDPREVIEWWG_H
 #define CCDPREVIEWWG_H
 
 #include <qpixmap.h>
 #include <kpixmapio.h>
  
 #include "ccdpreviewui.h"
 #include "qframe.h"
 
enum Pixelorder {PIXELORDER_NORMAL=1, PIXELORDER_DUAL=2};

 class QImage;
 class QSocketNotifier;
 class CCDVideoWG;
 class INDIStdDevice;
 class QPainter;
 class QVBoxLayout;
 
 class CCDPreviewWG : public CCDPreviewForm
 {
   Q_OBJECT
   
    public:
      CCDPreviewWG(INDIStdDevice *inStdDev, QWidget * parent =0, const char * name =0);
      ~CCDPreviewWG();
 
   friend class CCDVideoWG;
   friend class INDIStdDevice;
   
   void setColorFrame(bool color);
   void setCtrl(int wd, int ht,int po, int bpp, unsigned long mgd);
   void setCCDInfo(double in_fwhm, int in_mu);
   void enableStream(bool enable);
   
   bool	processStream;
   int         		 streamWidth, streamHeight;
   CCDVideoWG		*streamFrame;
   bool			 colorFrame;
   
   private:
   INDIStdDevice        *stdDev;
   QPixmap               playPix, pausePix, capturePix;
   QVBoxLayout           *videoFrameLayout;
   double fwhm;
   int mu;
   
   protected:
   void closeEvent ( QCloseEvent * e );
   void resizeEvent(QResizeEvent *ev);
   
   
   public slots: 
   void playPressed();
   void captureImage();
   void brightnessChanged(int value);
   void contrastChanged(int value);
   void gammaChanged(int value);
   void updateFWHM();
 };
 
 class CCDVideoWG : public QFrame
 {
      Q_OBJECT
   
    public:
      CCDVideoWG(QWidget * parent =0, const char * name =0);
      ~CCDVideoWG();
      
      friend class CCDPreviewWG;
      
      void newFrame(unsigned char *buffer, int buffSiz, int w, int h);

    private:
      void redrawVideoWG(void);
      long		totalBaseCount;
      QRgb              *grayTable;
      QImage		*streamImage;
      QPixmap		 qPix;
      KPixmapIO		 kPixIO;
      unsigned char *streamBuffer;
      unsigned char *displayBuffer;
      long  streamBufferPos;
      double scale,offset,gamma;  
      int bytesPerPixel, PixelOrder;
      int Width, Height;
      unsigned long maxGoodData;
    protected:
     void paintEvent(QPaintEvent *ev);

   public slots: 
};

#endif  //CCDPREVIEWWG
