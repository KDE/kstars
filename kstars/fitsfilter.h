/***************************************************************************
                          fitsfilter.h  -   FITS Filters
                          ----------------
    begin                : Wed Mar 10th 2004
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
 
 #ifndef FITSFILTER_H
 #define FITSFILTER_H
 
 #include <qstringlist.h>
 #include <kdialogbase.h>
 
 class FITSViewer;
 class FilterUI;
 
 class FITSFilter : public KDialogBase
 {
    Q_OBJECT
    
      public:
        FITSFilter(QWidget * parent = 0, const char * name =0);
	~FITSFilter();
	
      private:
         FITSViewer *viewer;
	 FilterUI   *filterWidget;
	 
	 QStringList  filterBrowser;
	 QStringList  filterBuffer;
	 
     public slots:
     void slotAddFilter();
     void slotMoveFilterUp();
     void slotMoveFilterDown();
     void slotCopyFilter();
     void slotRemoveFilter();
	 
};





#endif

