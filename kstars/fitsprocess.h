/***************************************************************************
                          fitsprocess.h  -  Image processing utilities
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
 
 #ifndef FITSPROCESS_H
 #define FITSPROCESS_H
 
 #include <qptrlist.h>
 #include <qstringlist.h>
 #include <kcommand.h>
 
 /*1. QPtrList<unsigned int *> darkFrames;
      2. QPtrList<unsigned int *> flatFrames;
      3. The class reads the hdu of each FITS, the size of each frame must match the original frame, if not, abort and inform the user.
      4. Ignore the EXPOSURE (time in milliseconds) differences for now. We need to compensate for differences by employing different methods of extrapolation later. 
      5. void combine(int mode); mode is either FITS_AVERAGE or FITS_MEDIAN.
      6. void subtract(unsigned int * img1, unsigned int * img2); we know numOfPixels already.
      7. void divide(unsigned int * img1, unsigned int * img2); we know numOfPixels already.*/

class FITSViewer;
class QImage;

class FITSProcess
{
   public:
     FITSProcess(FITSViewer *parent, QStringList darkFiles, QStringList flatFiles, QStringList darkflatFiles, int darkMode, int flatMode, int darkflatMode);
     ~FITSProcess();
     
     QPtrList<float> darkFrames;
     QPtrList<float> flatFrames;
     QPtrList<float> darkflatFrames;
     FITSViewer *viewer;
     
     int npix;
     int darkCombineMode;
     int flatCombineMode;
     int darkflatCombineMode;
     float *finalDark;
     float *finalFlat;
     float *finalDarkFlat;
     
     float * combine(QPtrList<float> & frames, int mode);
     void subtract(float * img1, float * img2);
     void divide(float * img1, float * img2);
     void reduce();
     void normalize(float *buf);
     float quick_select(float * arr, int n);
     float average(float * array, int n);
     float min(float *buf);
     
};

class FITSProcessCommand : public KCommand
{
  public:
  
  FITSProcessCommand(FITSViewer *parent);
  ~FITSProcessCommand();
 
  void execute();
  void unexecute();
  QString name() const;
  
  private:
  FITSViewer *viewer;
  float * buffer;
  QImage *oldImage;
 
};
 
#endif
