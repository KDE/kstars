#ifndef EXPORTIMAGEDIALOG_H
#define EXPORTIMAGEDIALOG_H

#include "ui_exportimagedialog.h"

class KStars;
class SkyQPainter;

class ExportImageDialogUI : public QFrame, public Ui::ExportImageDialog
{
    Q_OBJECT

public:
    explicit ExportImageDialogUI(QWidget *parent = 0);
};


class ExportImageDialog : public KDialog
{
    Q_OBJECT

public:
    ExportImageDialog(KStars *kstars, const QString &url, int w, int h);
    ~ExportImageDialog();

private slots:
    void exportImage();
    void switchLegendConfig(bool newState);

private:
    void setupWidgets();
    void setupConnections();

    void addLegend(SkyQPainter *painter);
    void addLegend(QPaintDevice *pd);

    KStars *m_KStars;
    ExportImageDialogUI *m_DialogUI;
    QString m_Url;
    int m_Width;
    int m_Height;
};


#endif // EXPORTIMAGEDIALOG_H
