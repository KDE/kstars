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
 
 #include <stdlib.h>
 #include <unistd.h>
 
 #include <kdebug.h>
 #include <klocale.h>
 #include <kprogress.h>
 #include <kapplication.h>
 
 #include <qimage.h>
 
 #include "fitsprocess.h"
 #include "fitsviewer.h"
 #include "fitsimage.h"
 
 #define ELEM_SWAP(a,b) { register float t=(a);(a)=(b);(b)=t; }
 
 FITSProcess::FITSProcess(FITSViewer *parent, QStringList darkFiles, QStringList flatFiles, QStringList darkflatFiles, int darkMode, int flatMode, int darkflatMode)
 {
   float * buffer      = NULL;
   darkCombineMode     = darkMode;
   flatCombineMode     = flatMode;
   darkflatCombineMode = darkflatMode;
   viewer              = parent;
   finalDark           = NULL;
   finalFlat           = NULL;
   finalDarkFlat       = NULL;
   npix                = viewer->image->width * viewer->image->height;
   
   darkFrames.setAutoDelete(true);
   flatFrames.setAutoDelete(true);
   darkflatFrames.setAutoDelete(true);
   
   KProgressDialog reduceProgress(0, 0, i18n("FITS Viewer"), i18n("Image Loading Process..."));
   reduceProgress.progressBar()->setTotalSteps(darkFiles.size() + flatFiles.size() + darkflatFiles.size());
   reduceProgress.setMinimumWidth(300);
   reduceProgress.show();
   
   int nprogress = 0;
   
   /* #1 Load dark frames */
   for (unsigned int i=0; i < darkFiles.size(); i++)
   {
     if ( (buffer = viewer->loadData(darkFiles[i].ascii(), buffer)) == NULL)
     {
        kdDebug() << "Error loading dark file " << darkFiles[i] << endl;
	break;
     }
     
     reduceProgress.progressBar()->setProgress(++nprogress);
     kapp->processEvents();
     darkFrames.append(buffer);
   }
   
   /* Load flat frames */
   for (unsigned int i=0; i < flatFiles.size(); i++)
   {
     if ( (buffer = viewer->loadData(flatFiles[i].ascii(), buffer)) == NULL)
     {
        kdDebug() << "Error loading flat file " << flatFiles[i] << endl;
	break;
     }
     
     reduceProgress.progressBar()->setProgress(++nprogress);
     kapp->processEvents();
     flatFrames.append(buffer);
   }
   
   /* Load dark frames for the flat field */
   for (unsigned int i=0; i < darkflatFiles.size(); i++)
   {
     if ( (buffer = viewer->loadData(darkflatFiles[i].ascii(), buffer)) == NULL)
     {
        kdDebug() << "Error loading dark flat file " << darkflatFiles[i] << endl;
	break;
     }
     
     reduceProgress.progressBar()->setProgress(++nprogress);
     kapp->processEvents();
     darkflatFrames.append(buffer);
   }
    
 }
 

 FITSProcess::~FITSProcess() {}
 
float * FITSProcess::combine(QPtrList<float> & frames, int mode)
 {
    int nframes = frames.count();
    float *dest;
    float *narray;
    
    if (nframes == 0)
    {
      dest = NULL;
      return dest;
    }
    else if (nframes == 1)
    {
      dest = frames.at(0);
      return dest;
    }
    
    dest = frames.at(0);
    narray = (float *) malloc (nframes * sizeof(float));
    
    switch (mode)
    {
     /* Average */
     case 0:
      for (int i=0; i < npix; i++)
      {
        for (int j=0; j < nframes; j++)
	  narray[j] = *((frames.at(j)) + i);
	dest[i] = average(narray, nframes);

      }
      break;
    /* Median */
     case 1:
      for (int i=0; i < npix; i++)
      {
        for (int j=0; j < nframes; j++)
	  narray[j] = *((frames.at(j)) + i);
	dest[i] = quick_select(narray, nframes);
      }
      break;	    
     }
     
     free(narray);
     return dest;
}
 
 /*
 *  This Quickselect routine is based on the algorithm described in
 *  "Numerical recipies in C", Second Edition,
 *  Cambridge University Press, 1992, Section 8.5, ISBN 0-521-43108-5
 */
float FITSProcess::quick_select(float * arr, int n) 
{
    int low, high ;
    int median;
    int middle, ll, hh;

    low = 0 ; high = n-1 ; median = (low + high) / 2;
    for (;;) {
        if (high <= low) /* One element only */
            return arr[median] ;

        if (high == low + 1) {  /* Two elements only */
            if (arr[low] > arr[high])
                ELEM_SWAP(arr[low], arr[high]) ;
            return arr[median] ;
        }

    /* Find median of low, middle and high items; swap into position low */
    middle = (low + high) / 2;
    if (arr[middle] > arr[high])    ELEM_SWAP(arr[middle], arr[high]) ;
    if (arr[low] > arr[high])       ELEM_SWAP(arr[low], arr[high]) ;
    if (arr[middle] > arr[low])     ELEM_SWAP(arr[middle], arr[low]) ;

    /* Swap low item (now in position middle) into position (low+1) */
    ELEM_SWAP(arr[middle], arr[low+1]) ;

    /* Nibble from each end towards middle, swapping items when stuck */
    ll = low + 1;
    hh = high;
    for (;;) {
        do ll++; while (arr[low] > arr[ll]) ;
        do hh--; while (arr[hh]  > arr[low]) ;

        if (hh < ll)
        break;

        ELEM_SWAP(arr[ll], arr[hh]) ;
    }

    /* Swap middle item (in position low) back into correct position */
    ELEM_SWAP(arr[low], arr[hh]) ;

    /* Re-set active partition */
    if (hh <= median)
        low = ll;
        if (hh >= median)
        high = hh - 1;
    }
}
 
 float FITSProcess::average(float * array, int n)
 {
    double total=0;
    for (int i=0; i < n; i++)
      total += array[i];
      
    return (total / n);
 }
    
 
 void FITSProcess::subtract(float * img1, float * img2)
{
   for (int i=0; i < npix; i++)
    if (img1[i] < img2[i])
      img1[i] = 0;
    else img1[i] -= img2[i];
     
}
 
 void FITSProcess::divide(float * img1, float * img2)
{
   for (int i=0; i < npix; i++)
   {
     if (img2[i] == 0)
       img1[i] = 0;
     else
       img1[i] = img1[i] / img2[i];
   }
   
}

void FITSProcess::normalize(float *buf)
{

   float avg = average(buf, npix);
   if (!avg) return;
    
   if (avg < 0)
     avg += abs((int) min(buf));
    
  for (int i=0; i < npix; i++)
    buf[i] = buf[i] / avg;

}

float FITSProcess::min(float *buf)
{
  float lmin= buf[0];
  
  for (int i=1; i < npix; i++)
    if (buf[i] < lmin) lmin = buf[i];
    
  return lmin;
}

void FITSProcess::reduce()
{
   KProgressDialog reduceProgress(0, 0, i18n("FITS Viewer"), i18n("Image Reduction Process..."));
   reduceProgress.progressBar()->setTotalSteps(20);
   reduceProgress.setMinimumWidth(300);
   reduceProgress.show();
   
   reduceProgress.progressBar()->setProgress(1);
   kapp->processEvents();
   
   /* Combine darks */
   finalDark = combine(darkFrames, darkCombineMode);
   
   reduceProgress.progressBar()->setProgress(5);
   kapp->processEvents();
   
   /* Combine flats */
   finalFlat = combine(flatFrames, flatCombineMode);
   reduceProgress.progressBar()->setProgress(10);
   kapp->processEvents();
   
   /* Combine dark flats */
   finalDarkFlat = combine(darkflatFrames, darkflatCombineMode);
   reduceProgress.progressBar()->setProgress(12);
   kapp->processEvents();
     
   /* Subtract the dark frame */
   if (finalDark)
   	subtract( viewer->imgBuffer, finalDark);
   reduceProgress.progressBar()->setProgress(14);
   kapp->processEvents();
   

   /* Subtract the fark frame from the flat field and then apply to the image buffer */
   if (finalFlat)
   {
     if (finalDarkFlat)
        subtract( finalFlat, finalDarkFlat);
      reduceProgress.progressBar()->setProgress(16);
      kapp->processEvents();
     
     normalize(finalFlat);
     reduceProgress.progressBar()->setProgress(18);
     kapp->processEvents();
     
     divide(viewer->imgBuffer, finalFlat);
   }
   
   reduceProgress.progressBar()->setProgress(20);
   kapp->processEvents();

} 
 
FITSProcessCommand::FITSProcessCommand(FITSViewer *parent)
{
 
   viewer  = parent;
   oldImage  = new QImage();
   // TODO apply maximum compression against this buffer
   buffer = (float *) malloc (viewer->image->width * viewer->image->height * sizeof(float));
   memcpy(buffer, viewer->imgBuffer, viewer->image->width * viewer->image->height * sizeof(float));
 
 }
 
FITSProcessCommand::~FITSProcessCommand()
{
    free (buffer);
    delete (oldImage);
}
 
void FITSProcessCommand::execute()
{
  memcpy(viewer->imgBuffer, buffer, viewer->image->width * viewer->image->height * sizeof(float));
  *oldImage = viewer->image->displayImage->copy();
}
  
void FITSProcessCommand::unexecute()
{
  
  memcpy( viewer->imgBuffer, buffer, viewer->image->width * viewer->image->height * sizeof(float));
  viewer->calculateStats();
  *viewer->image->displayImage = oldImage->copy();
  viewer->image->zoomToCurrent();
 
}
  
QString FITSProcessCommand::name() const
{
  return i18n("Image Reduction");
}


