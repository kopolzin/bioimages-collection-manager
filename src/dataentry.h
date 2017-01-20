// The MIT License (MIT)
//
// Copyright (c) 2014-2015 Ken Polzin
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifndef DATAENTRY_H
#define DATAENTRY_H

#include <QMainWindow>
#include <QtCore>
#include <QtGui>
#include <QFileSystemModel>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUrl>
#include <QCompleter>
#include <QProgressDialog>
#include <QLineEdit>
#include <QtConcurrent>
#include <QListWidgetItem>
#include <memory>

#include "agent.h"
#include "determination.h"
#include "image.h"
#include "organism.h"
#include "sensu.h"
#include "help.h"

namespace Ui {
class DataEntry;
}

class DataEntry : public QMainWindow
{
    Q_OBJECT

public:
    explicit DataEntry(const QStringList &fileNames, const QHash<QString,QString> &incNameSpaceHash,
                       const QHash<QString, QString> &photogHash, const QHash<QString, int> &trailingHash,
                       const QHash<QString, QString> &incAgentHash, QWidget *parent = 0);
    explicit DataEntry(const QStringList &fileNames, const QHash<QString, QString> &photogHash,
                       const QHash<QString,QString> &incAgentHash, const QList<Image> &ims, QWidget *parent = 0);
    ~DataEntry();

signals:
   void windowClosed();
   void iconifyDone();

private slots:
    void exifToolFinished();
    void loadUSDANames();

    void on_actionQuit_triggered();
    void on_actionSave_changes_triggered();
    void on_actionHelp_Index_triggered();

    void on_thumbWidget_itemSelectionChanged();
    void on_nextButton_clicked();
    void on_previousButton_clicked();
    void on_zoomIn_clicked();
    void on_zoomOut_clicked();

    void on_reverseGeocodeButton_clicked();
    void httpFinished();

    void on_generateNewOrganismIDButton_clicked();
    void on_image_county_box_textEdited(const QString &arg1);
    void on_image_stateProvince_box_textEdited(const QString &arg1);
    void on_image_countryCode_box_textEdited(const QString &arg1);
    void on_image_continent_box_textEdited(const QString &arg1);

    void on_organismID_textEdited(const QString &arg1);
    void on_tsnID_textEdited(const QString &arg1);
    void on_nameAccordingToID_currentIndexChanged(const QString &arg1);
    void on_identificationRemarks_textEdited(const QString &arg1);
    void on_establishmentMeans_activated(const QString &arg1);

    void on_geodedicDatum_textEdited(const QString &arg1);
    void on_geonamesAdmin_textEdited(const QString &arg1);
    void on_geonamesOther_textEdited(const QString &arg1);

    void on_copyrightYear_textEdited(const QString &arg1);
    void on_copyrightStatement_textEdited(const QString &arg1);

    void on_groupOfSpecimenBox_currentTextChanged(const QString &arg1);
    void on_partOfSpecimenBox_currentTextChanged(const QString &arg1);
    void on_viewOfSpecimenBox_currentTextChanged(const QString &arg1);
    void on_imageCaption_textEdited(const QString &arg1);

    void on_imageLatAndLongBox_textEdited(const QString &arg1);
    void on_elevation_textEdited(const QString &arg1);
    void on_locality_textEdited(const QString &arg1);

    void on_image_informationWithheld_box_activated(const QString &arg1);
    void on_image_dataGeneralization_box_activated(const QString &arg1);
    void on_image_occurrenceRemarks_box_textEdited(const QString &arg1);

    void on_image_date_box_textEdited(const QString &arg1);
    void on_image_time_box_textEdited(const QString &arg1);
    void on_image_focalLength_box_textEdited(const QString &arg1);

    void on_manualIDOverride_clicked();
    void on_cameoButton_clicked();
    void on_thumbnailFilter_activated(int index);
    void on_schemeButton_clicked();

    void on_usageTermsBox_currentIndexChanged(int index);
    void on_copyrightOwner_activated(const QString &arg1);
    void on_collectionCode_activated(const QString &arg1);
    void on_catalogNumber_textEdited(const QString &arg1);

    void on_imageGeoreferenceRemarks_activated(const QString &arg1);
    void on_organismGeoreferenceRemarks_activated(const QString &arg1);
    void on_organismLatLong_textEdited(const QString &arg1);
    void on_organismRemarks_textEdited(const QString &arg1);

    void on_organismName_textEdited(const QString &arg1);
    void on_organismScope_textEdited(const QString &arg1);
    void on_htmlNote_textEdited(const QString &arg1);
    void on_highResURL_textEdited(const QString &arg1);

    void on_newDeterminationButton_clicked();
    void on_nextDetermination_clicked();
    void on_previousDetermation_clicked();

    void on_organismAltitude_textEdited(const QString &arg1);
    void on_image_timezone_box_textEdited(const QString &arg1);
    void on_photoshop_Credit_textEdited(const QString &arg1);
    void on_actionExport_entire_database_to_CSV_files_triggered();
    void on_imageTitle_textEdited(const QString &arg1);
    void on_imageDescription_textEdited(const QString &arg1);
    void on_geonamesOtherButton_clicked();
    void on_photographer_activated(const QString &arg1);
    void on_coordinateUncertaintyInMetersBox_editTextChanged(const QString &arg1);

    void on_actionReturn_to_Start_Screen_triggered();
    void on_advancedThumbnailFilter_clicked();
    void on_thumbnailSort_activated(const QString &arg1);
    void on_actionNew_agent_triggered();
    void on_actionNew_sensu_triggered();

    void on_iconifyDone();
    void showContextMenu(const QPoint &pos);
    void deleteThumbnail();

    void on_actionTableview_locally_modified_triggered();
    void on_actionTableview_entire_database_triggered();


private:
    Ui::DataEntry *ui;
    QPointer<Help> help_w;
    QStringList agents;
    QList<Agent> newAgents;
    QList<Determination> currentDeterminations;
    int currentDetermination;
    void loadSensu();
    QHash<QString,QString> sensuHash;
    QMap<QString,QString> usdaNames; // map of <tsnID, "comma-separated scientific names">
    QProgressDialog *progress;
    int progressLength;
    int currentProgress;
    QStringList thumbWidgetItems;
    QList<int> similarImages;

    void setupDataEntry();
    void setupCompleters();
    QStringList allGenera;
    QCompleter *completeGenera;
    QStringList allSpecies;
    QCompleter *completeSpecies;
    QStringList allCommonNames;
    QCompleter *completeCommonNames;
    QStringList allFamilies;
    QCompleter *completeFamilyNames;
    QStringList sensus;
    QCompleter *completeSensu;
    void setTaxaFromTSN(const QString &tsnID);

    void resizeEvent(QResizeEvent *);
    void moveEvent(QMoveEvent *);
    void changeEvent(QEvent*event);
    bool pauseRefreshing;

    QHash<QString, QString> imageHash;
    QHash<QString, int> imageIndexHash;
    QMap<QString, QString> imageIDFilenameMap;
    QMap<QString,QString> dateFilenameMap;
    QHash<QString, QString> nameSpaceHash;
    QHash<QString, QString> photographerHash;
    QHash<QString, int> trailingCharsHash;
    QList<Image> images;

    QString appDir;
    bool fromEditExisting;

    void loadImageIDs();
    QStringList imageIDList;
    QStringList organismIDList;
    int lastOrgNum;
    QString agent;
    QHash<QString,QString> agentHash;
    QString schemeNamespace;
    QString schemeText;
    QString schemeNumber;

    QStringList specimenGroups;// { "unspecified", "woody angiosperms", "herbaceous angiosperms",
                               //  "gymnosperms", "ferns", "cacti", "mosses" };
    void runExifTool();
    QList<QString> imageFileNames;
    int imageFileNamesSize;
    QProcess ps;
    int exifIndex;
    bool exifComplete;
    QString exifOutput;

    QList<QLineEdit*> lineEditNames;
    void generateThumbnails();
    void refreshImageLabel();
    void refreshInputFields();
    void clearInputFields();
    void clearToolTips();
    QString simplifyField(QStringList &fieldList);
    void saveInput(const QString &inputField, const QString &inputData);
    int viewedImage;

    QString baseReverseGeocodeURL;
    void startRequest(QUrl url);
    void reverseGeocodeQueue();
    QList<QString> revgeoURLList;
    QHash<QString,int> revgeoHash;
    QUrl url;
    QNetworkAccessManager qnam;
    QNetworkReply *reply;

    QHash<QString,QString> stateTwoLetter;
    QHash<QString,QString> countyGeonameID;

    void refreshSpecimenGroup(const QString &arg1);
    bool refreshSpecimenPart(const QString &arg1);
    bool refreshSpecimenView(const QString &arg1);
    void averageLocations(const QString &orgID);
    void iconify(const QStringList &imageFileNames);

    QString modifiedNow();

    void scaleImage(double factor);
    void adjustScrollBar(QScrollBar *scrollBar, double factor);
    double scaleFactor;
    QLabel *imageLabel;
    QString viewToNumbers(const QString &group, const QString &part, const QString &view);
    bool pauseSavingView;

    QString singleResult(const QString result, const QString table, const QString field, const QString value);
    QStringList numbersToView(const QString &numbers);
    void removeUnlinkedOrganisms();
    void autosetTitle(QString tsnID, QString organismID);

    QString currentThumbnailSort;
    QString dbLastPublished;
    void refreshAgentDropdowns();
};

#endif // DATAENTRY_H
