
#ifndef _KSLINEEDIT_H_
#define _KSLINEEDIT_H_

#include <qwidget.h>
#include <klineedit.h>

/**
 * 
 * Jason Harris
 **/
class KSLineEdit : public KLineEdit
{
Q_OBJECT

public:
	KSLineEdit(QWidget *parent, const char *name=0);
	~KSLineEdit();

signals:
	void gotFocus();

protected:
	virtual void focusInEvent( QFocusEvent *e );

};

#endif
