#ifndef KSTARS_TOGGLEACTION_H
#define KSTARS_TOGGLEACTION_H

#include <qiconset.h>
#include <kaction.h>

class ToggleAction : public KAction {
	Q_OBJECT
		
	public:
		ToggleAction(const QString& ontext, const QIconSet& onpix, const QString& offtext, const QIconSet& offpix, int accel, const QObject* receiver, const char* slot, QObject* parent = 0, const char* name = 0 ) ;
		ToggleAction(const QString& ontext, const QString& offtext, int accel, const QObject* receiver, const char* slot, QObject* parent = 0, const char* name = 0 ) ;
		void setOnToolTip(QString tip);
		void setOffToolTip(QString tip);

	public slots:
		void turnOff();
		void turnOn();

	private:
		QIconSet officon;
		QIconSet onicon;
		QString offcap;
		QString oncap;
		QString onTip;
		QString offTip;
		bool state;
};


#endif
