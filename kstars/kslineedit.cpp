
#include "kslineedit.h"


KSLineEdit::KSLineEdit(QWidget *parent, const char *name)
  : KLineEdit(parent, name)
{
}


KSLineEdit::~KSLineEdit()
{
}

void KSLineEdit::focusInEvent( QFocusEvent *e ) {
	if ( e->gotFocus() ) emit gotFocus();
}

#include "kslineedit.moc"
