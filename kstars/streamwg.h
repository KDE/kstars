/*  Stream Widget
    Copyright (C) 2003 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
    
    2004-03-16: A class to handle video streaming.
 */
 
 #ifndef STREAMWG_H
 #define STREAMWG_H
 
 #include <qpixmap.h>
 #include <kpixmapio.h>
  
 #include "streamformui.h"
 #include "qframe.h"
 
 
 class QImage;
 class QSocketNotifier;
 class VideoWG;
 class INDIStdDevice;
 class QPainter;
 
 class StreamWG : public streamForm
 {
   Q_OBJECT
   
    public:
      StreamWG(INDIStdDevice *inStdDev, QWidget * parent =0, const char * name =0);
      ~StreamWG();
 
   friend class VideoWG;
   friend class INDIStdDevice;
   
   void setColorFrame(bool color);
   void establishDataChannel(QString host, int port);
   void allocateStreamBuffer();
   void setSize(int wd, int ht);
   void enableStream(bool enable);
   
   private:
   
   INDIStdDevice        *stdDev;
   QSocketNotifier 	*sNotifier;
   VideoWG		*streamFrame;
   unsigned char        *streamBuffer;
   int         		 streamFD, streamWidth, streamHeight;
   bool			 processStream;
   bool			 colorFrame;
   int			 frameTotalBytes;
   QPixmap               playPix, pausePix, capturePix;
   
   protected:
   void closeEvent ( QCloseEvent * e );
   void resizeEvent(QResizeEvent *ev);
   
   
   public slots: 
   void streamReceived();
   void playPressed();
   void captureImage();


 };
 
 class VideoWG : public QFrame
 {
      Q_OBJECT
   
    public:
      VideoWG(QWidget * parent =0, const char * name =0);
      ~VideoWG();
      
      friend class StreamWG;
      
      void newFrame(unsigned char *buffer, int w, int h, bool color);
      
    private:
      QRgb              *grayTable;
      QImage		*streamImage;
      QPixmap		 qPix;
      KPixmapIO		 kPixIO;
      
    protected:
     void paintEvent(QPaintEvent *ev);
     
};
 
 #endif
 