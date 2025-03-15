/*
    SPDX-FileCopyrightText: 2023 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "imageoverlaycomponent.h"
#include "skycomponent.h"
#include <QSharedPointer>
#include <QImage>
#include <QObject>
#include <QLabel>
#include <QTimer>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QGroupBox>
#include <QComboBox>
#include <QAbstractItemView>
#include "fitsviewer/fitsdata.h"

class QTableWidget;
class SolverUtils;

class ImageOverlay
{
    public:
        enum Status
        {
            UNPROCESSED = 0,
            BAD_FILE,
            PLATE_SOLVE_FAILURE,
            OTHER_ERROR,
            AVAILABLE,
            NUM_STATUS
        };

        ImageOverlay(const QString &filename = "", bool enabled = true, const QString &nickname = "",
                     Status status = UNPROCESSED, double orientation = 0, double ra = 0, double dec = 0,
                     double pixelsPerArcsec = 0, bool eastToTheRight = true, int width = 0, int height = 0)
            : m_Filename(filename), m_Enabled(enabled), m_Nickname(nickname), m_Status(status),
              m_Orientation(orientation), m_RA(ra), m_DEC(dec), m_ArcsecPerPixel(pixelsPerArcsec),
              m_EastToTheRight(eastToTheRight), m_Width(width), m_Height(height)
        {
        }

        QString m_Filename;
        bool m_Enabled = true;
        QString m_Nickname;
        Status m_Status = UNPROCESSED;
        double m_Orientation = 0.0;
        double m_RA = 0.0;
        double m_DEC = 0.0;
        double m_ArcsecPerPixel = 0.0;
        bool m_EastToTheRight = true;
        int m_Width = 0;
        int m_Height = 0;
        QSharedPointer<QImage> m_Img = nullptr;
};

/**
 * @class ImageOverlayComponent
 * Represents the ImageOverlay overlay
 * @author Hy Murveit
 * @version 1.0
 */
class ImageOverlayComponent : public QObject, public SkyComponent
{
    Q_OBJECT

  public:
    explicit ImageOverlayComponent(SkyComposite *);

    virtual ~ImageOverlayComponent() override;

    bool selected() override;
    void draw(SkyPainter *skyp) override;
    void setWidgets(QTableWidget *table, QPlainTextEdit *statusDisplay, QPushButton *solveButton,
                    QGroupBox *tableTitleBox, QComboBox *solverProfile);
    void updateTable();

    const QList<ImageOverlay> imageOverlays() const {
        return m_Overlays;
    }
    const QList<ImageOverlay> temporaryImageOverlays() const {
        return m_TemporaryOverlays;
    }

    public slots:
    void startSolving();
    void abortSolving();
    void show();
    void reload();  // not currently implemented.
    QString directory()
    {
        return m_Directory;
    };
    void addTemporaryImageOverlay(const ImageOverlay &overlay);

 signals:
    void updateLog(const QString &message);

private slots:
    void tryAgain();
    void updateStatusDisplay(const QString &message);

private:
    void loadFromUserDB();
    void saveToUserDB();
    void solveImage(const QString &filename);
    void solverDone(bool timedOut, bool success, const FITSImage::Solution &solution, double elapsedSeconds);
    void initializeGui();
    int numAvailable();
    void cellChanged(int row, int col);
    void statusCellChanged(int row);
    void selectionChanged();
    void initSolverProfiles();

    // Methods that load the image files in the background.
    void loadAllImageFiles();
    void loadImageFileLoop();
    bool loadImageFile();
    QImage *loadImageFile (const QString &fullFilename, bool mirror);


    QTableWidget *m_ImageOverlayTable;
    QGroupBox *m_TableGroupBox;
    QAbstractItemView::EditTriggers m_EditTriggers;
    QPlainTextEdit *m_StatusDisplay;
    QPushButton *m_SolveButton;
    QComboBox *m_SolverProfile;
    QStringList m_LogText;
    bool m_Initialized = false;

    QList<ImageOverlay> m_Overlays;
    QList<ImageOverlay> m_TemporaryOverlays;
    QMap<QString, int> m_Filenames;
    QSharedPointer<SolverUtils> m_Solver;
    QList<int> m_RowsToSolve;
    QString m_Directory;
    QTimer m_TryAgainTimer;
    QFuture<void> m_LoadImagesFuture;
};
