/***************************************************************************
                          fitshistogram.h  -  FITS Historgram
                          ---------------
    begin                : Thu Mar 4th 2004
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
 
 #ifndef FITSHISTOGRAM
 #define FITSHISTOGRAM
 
 #include "histdialog.h"
 #include <kcommand.h>
 
 #define BARS 500
 
 class FITSViewer;
 class QPixmap;
 
 class FITSHistogram : public histDialog
 {
   Q_OBJECT
   
   public:
    FITSHistogram(QWidget *parent, const char * name = 0);
    ~FITSHistogram();
    
    void constructHistogram(unsigned int *buffer);
    int  findMax();
    int type;
    
    private:
    int histArray[BARS]; 
    int binSize;

    FITSViewer * viewer;
    QPixmap *histogram;
    
    protected:
    void paintEvent( QPaintEvent *e);
    void mouseMoveEvent( QMouseEvent *e);
    
    
    public slots:
    void applyScale();
    void updateBoxes();
    
    
 };
 
 class histCommand : public KCommand
{
  public:
        histCommand(QWidget * parent, int newType, QImage *newIMG, QImage *oldIMG, unsigned int * old_buffer);
	~histCommand();
            
        void execute();
        void unexecute();
        QString name() const;

    protected:
        int type;
	unsigned int *oldBuffer;
        FITSViewer *viewer;
        QImage *newImage;
	QImage *oldImage;
};
 
 
 #endif
 
 