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
 
 #include <kdebug.h>
 
 #include "fitsprocess.h"
 #include "fitsviewer.h"
 #include "fitsimage.h"
 
 FITSProcess::FITSProcess(FITSViewer *parent, QStringList darkFiles, QStringList flatFiles, int darkMode, int flatMode)
 {
   unsigned int * buffer;
   darkCombineMode = darkMode;
   flatCombineMode = flatMode;
   viewer          = parent;
   
   darkFrames.setAutoDelete(true);
   flatFrames.setAutoDelete(true);
   
   for (int i=0; i < darkFiles.size(); i++)
   {
     if ( (buffer = viewer->loadData(darkFiles[i].ascii(), buffer)) == NULL)
     {
        kdDebug() << "Error loading dark file " << darkFiles[i] << endl;
	break;
     }
     
     darkFrames.append(buffer);
   }
   
   for (int i=0; i < flatFiles.size(); i++)
   {
     if ( (buffer = viewer->loadData(flatFiles[i].ascii(), buffer)) == NULL)
     {
        kdDebug() << "Error loading flat file " << flatFiles[i] << endl;
	break;
     }
     
     flatFrames.append(buffer);
   }
   
   //TODO change this!
   if (darkFrames.count() > 0)
   	subtract( viewer->imgBuffer, darkFrames.at(0));
   if (flatFrames.count() > 0);
   {
       //subtract( flatFrames.at(0), darkFrames.at(0));
       //divide( viewer->imgBuffer, flatFrames.at(0));
   }
 
 }
 
 FITSProcess::~ FITSProcess()
 {
 
 
 
 }
 
 void FITSProcess::combine(QPtrList<unsigned int *> list, int mode)
 {
 
 
 
 
 
 
 }
 
 void FITSProcess::subtract(unsigned int * img1, unsigned int * img2)
 {
   kdDebug() << "in substact " << endl;
   if (img1 == NULL) kdDebug() << "img1 is NULL " << endl;
   if (img2 == NULL) kdDebug() << "img2 is NULL " << endl;
   kdDebug() << "img1[134817] " << img1[134817] << " -- img2[134817] " << img2[134817] << endl;
   
   int n = viewer->stats.width * viewer->stats.height;
 
   for (int i=0; i < n; i++)
    if (img1[i] < img2[i])
      img1[i] = 0;
    else img1[i] -= img2[i];
     
  
  kdDebug() << "img1[134817]= " << img1[134817] << endl;
      
  kdDebug() << "Finished substract " << endl;
 }
 
 void FITSProcess::divide(unsigned int * img1, unsigned int * img2)
 {
  kdDebug() << "in divide " << endl;
  int n = viewer->stats.width * viewer->stats.height;
  kdDebug() << "img1[134817] " << img1[134817] << " -- img2[134817] " << img2[134817] << endl;
  
   for (int i=0; i < n; i++)
   {
     if (img2[i] == 0)
       img1[i] = 0;
     else
       img1[i] = (int) ( (double) img1[i] / (double) img2[i]);
   }
   
  kdDebug() << "img1[134817]= " << img1[134817] << endl;
  kdDebug() << "finished divide " << endl;
 }
 
 
 
 
 