#ifndef KSTARS_SIMCLOCK_H_
#define KSTARS_SIMCLOCK_H_

#include <time.h>
#include <qtimer.h>
#include <qdatetime.h>

#include <simclockinterface.h>

/*
 * @short kstars simulation clock
 * @author Mark Hollomon 
 */


class SimClock : public QObject, public SimClockInterface {
	Q_OBJECT

	public:
		/**
		 * Constructor
		 * @param when the date/time to which the SimClock should be initialized in UTC
		 */
		SimClock(QObject *parent = 0, const QDateTime &when = QDateTime());
		/**
		 * Constructor
		 * @param old a SimClock to initialize from.
		 */
		SimClock(const SimClock &old);

		long double JD();
		QDateTime UTC();
		bool isActive();

		/**
		 * implementation of SimClockInterface
		 */
		virtual ASYNC stop();
		virtual ASYNC start();
		virtual ASYNC setUTC(const QDateTime &newtime);

	public slots:
		virtual ASYNC setScale(float s);

	signals:
		void timeChanged();
		void timeAdvanced();
		void scaleChanged(float);
		void clockStarted();
		void clockStopped();

	private:
		
		long double julian, julianmark;
		// only store zulu stuff. local times require knowledge the clock
		// shouldn't have.
		QDateTime utc;
		QTime gst;
		QTimer tmr;
		double scale;
		QTime sysmark;
		int lastelapsed;
		bool utcvalid;

		// used to generate names for dcop interfaces
		static int idgen;
		// how often to update
		static int TimerInterval;

	private slots:
		// respond to the QTimer::timeout signal
		void tick();

};

#endif
