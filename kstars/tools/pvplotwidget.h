#ifndef PVPLOTWIDGET_H
#define PVPLOTWIDGET_H

#include <QFrame>

class PVPlotWidget : public KStarsPlotWidget
{
Q_OBJECT
public:
	PVPlotWidget( double x1, double x2, double y1, double y2, 
			QWidget *parent=0, const char *name=0 );
	PVPlotWidget( QWidget *parent=0, const char *name=0 );
	~PVPlotWidget();

public slots:
	void slotZoomIn();
	void slotZoomOut();

signals:
	void doubleClicked( double, double );

protected:
	virtual void keyPressEvent( QKeyEvent *e );
	virtual void mousePressEvent( QMouseEvent *e );
	virtual void mouseMoveEvent( QMouseEvent *e );
	virtual void mouseReleaseEvent( QMouseEvent * );
	virtual void mouseDoubleClickEvent( QMouseEvent *e );
	virtual void wheelEvent( QWheelEvent *e );

private:
	bool mouseButtonDown;
	int oldx, oldy;
	PlanetViewer *pv;
};

#endif
