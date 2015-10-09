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

#include <QtCore>
#include <QLabel>
#include <QGraphicsPixmapItem>
#include <QPixmapCache>
#include <QMessageBox>
#include <QProgressDialog>
#include <QFileDialog>
#include <QInputDialog>
#include <QScrollBar>
#include <QDialogButtonBox>
#include <QtWidgets>
#include <QtConcurrent>
#include <QThread>
#include <QSqlQuery>
#include <QSqlDriver>
#include <QSqlError>

#include "dataentry.h"
#include "ui_dataentry.h"
#include "advancedthumbnailfilter.h"
#include "exportcsv.h"
#include "geonamesother.h"
#include "newagentdialog.h"
#include "newdeterminationdialog.h"
#include "newsensudialog.h"
#include "startwindow.h"
#include "tableeditor.h"

#include <QDebug>

DataEntry::DataEntry(const QStringList &fileNames, const QHash<QString,QString> &incNameSpaceHash,
                     const QHash<QString,QString> &photogHash, const QHash<QString,int> &trailingHash,
                     const QHash<QString,QString> &incAgentHash, QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::DataEntry)
{
    ui->setupUi(this);

    ui->thumbWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->thumbWidget, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(showContextMenu(QPoint)));

    fromEditExisting = false;
    agentHash = incAgentHash;
    nameSpaceHash = incNameSpaceHash;
    photographerHash = photogHash;
    trailingCharsHash = trailingHash;

    setupDataEntry();

    imageFileNames = fileNames;
    imageFileNamesSize = imageFileNames.size();

    for (int i = 0; i < imageFileNamesSize; ++i)
    {
        QString file = imageFileNames.at(i);
        QFileInfo fileInfo(file);
        QString base = fileInfo.fileName();
        imageHash.insert(base,file);
    }

    // connect exiftool signal
    ps.setProcessChannelMode(QProcess::MergedChannels);
    connect(&ps, SIGNAL(finished(int,QProcess::ExitStatus)), this, SLOT(exifToolFinished()));

    exifIndex = 0;
    exifOutput = "";
    exifComplete = false;

    loadUSDANames();
    setupCompleters();
    runExifTool();  // when finished, generateThumbnails() will be run
}

DataEntry::DataEntry(const QStringList &fileNames, const QHash<QString, QString> &photogHash,
                     const QHash<QString,QString> &incAgentHash, const QList<Image> &ims, QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::DataEntry)
{
    ui->setupUi(this);

    ui->thumbWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->thumbWidget, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(showContextMenu(QPoint)));

    fromEditExisting = true;
    agentHash = incAgentHash;
    photographerHash = photogHash;

    setupDataEntry();

    imageFileNames = fileNames;
    imageFileNamesSize = imageFileNames.size();

    for (int i = 0; i < imageFileNamesSize; ++i)
    {
        QString file = imageFileNames.at(i);
        QFileInfo fileInfo(file);
        QString base = fileInfo.fileName();
        imageHash.insert(base,file);
    }

    // load images table from database
    images = ims;

    int imagesIndex = 0;
    for (Image image : images)
    {
        imageIndexHash.insert(image.fileName,imagesIndex);
        imageIDFilenameMap.insert(image.identifier,image.fileName);
        dateFilenameMap.insertMulti(image.dcterms_created,image.fileName);
        imagesIndex++;
    }

    loadUSDANames();
    setupCompleters();
    generateThumbnails();
}

DataEntry::~DataEntry()
{
    delete ui;
}

void DataEntry::setupDataEntry()
{
    ui->coordinateUncertaintyInMetersBox->lineEdit()->setAlignment(Qt::AlignHCenter);
    ui->copyrightOwner->lineEdit()->setAlignment(Qt::AlignHCenter);
    ui->usageTermsBox->lineEdit()->setAlignment(Qt::AlignHCenter);

    lineEditNames = findChildren<QLineEdit *>();

    scaleFactor = 1;
    imageLabel = new QLabel(this);
    imageLabel->setScaledContents(true);
    pauseRefreshing = false;

    ui->scrollArea->setWidget(imageLabel);

    loadSensu();

    ui->thumbWidget->setViewMode(QListWidget::IconMode);
    ui->thumbWidget->setIconSize(QSize(100,100));
    ui->thumbWidget->setResizeMode(QListWidget::Adjust);
    ui->thumbWidget->setFocus();

    QStringList thumbnailSortList;
    thumbnailSortList << "Sort" << "Image dcterms_identifier A->Z" << "Image filename A->Z";
    thumbnailSortList << "Image date/time Old->New" << "Image date/time New->Old";
    thumbnailSortList << "Organism ID that image depicts A->Z";
    ui->thumbnailSort->addItems(thumbnailSortList);
    ui->thumbnailSort->setCurrentText("Sort");
    ui->thumbnailSort->view()->setMinimumWidth(ui->thumbnailSort->minimumSizeHint().width());

    QStringList filterList;
    filterList << "Displaying all thumbnails";
    filterList << "Thumbnails with no organism ID";
    filterList << "Thumbnails not identified (no tsnID)";
    filterList << "Thumbnails without latitude or longitude";
    filterList << "Thumbnails not reverse geocoded (no country)";
    filterList << "Thumbnails without time zones";
    filterList << "Thumbnails with \"unspecified\" specimen view";
    filterList << "Thumbnails with nominal source of name";
    filterList << "Only show cameos (1 image per organism)";
    ui->thumbnailFilter->addItems(filterList);
    ui->thumbnailFilter->view()->setMinimumWidth(ui->thumbnailFilter->minimumSizeHint().width());

    ui->generateNewOrganismIDButton->setAutoDefault(true);
    ui->manualIDOverride->setAutoDefault(true);
    ui->cameoButton->setAutoDefault(true);
    ui->previousButton->setAutoDefault(true);
    ui->nextButton->setAutoDefault(true);
    ui->reverseGeocodeButton->setAutoDefault(true);

    ui->zoomIn->setIcon(QPixmap(":/zoomin.png"));
    ui->zoomOut->setIcon(QPixmap(":/zoomout.png"));
    ui->tsnIDNotFound->setStyleSheet("color: red");
    ui->tsnIDNotFound->setVisible(false);
    ui->setOrganismIDFirst->setStyleSheet("color: red");
    ui->setOrganismIDFirst->setVisible(false);

    QStringList establishmentList;
    establishmentList << "native" << "introduced" << "naturalised" << "invasive" << "managed" << "uncertain";
    ui->establishmentMeans->addItems(establishmentList);
    ui->establishmentMeans->setCurrentText("");

    pauseSavingView = true;
    specimenGroups << "unspecified" << "woody angiosperms" << "herbaceous angiosperms" << "gymnosperms" << "ferns" << "cacti" << "mosses";
    ui->groupOfSpecimenBox->addItems(specimenGroups);
    ui->groupOfSpecimenBox->setCurrentText("unspecified");
    QStringList specimenParts;
    specimenParts << "unspecified" << "whole organism";
    ui->partOfSpecimenBox->addItems(specimenParts);
    ui->partOfSpecimenBox->setCurrentText("unspecified");
    ui->viewOfSpecimenBox->setCurrentText("unspecified");
    pauseSavingView = false;


    QSqlDatabase db = QSqlDatabase::database();
    db.transaction();

    QSqlQuery qry;
    qry.prepare("SELECT value FROM settings WHERE setting = (?)");
    qry.addBindValue("path.applicationfolder");
    qry.exec();
    if (qry.next())
        appDir = qry.value(0).toString();

    dbLastPublished = "2015-10-03";
    qry.prepare("SELECT value FROM settings WHERE setting = (?)");
    qry.addBindValue("metadata.version");
    qry.exec();
    if (qry.next())
        dbLastPublished = qry.value(0).toString();

    qry.prepare("SELECT value FROM settings WHERE setting = (?)");
    qry.addBindValue("view.dataentry.location");
    qry.exec();
    if (qry.next())
        restoreGeometry(qry.value(0).toByteArray());

    bool wasMaximized = false;
    qry.prepare("SELECT value FROM settings WHERE setting = (?)");
    qry.addBindValue("view.dataentry.fullscreen");
    qry.exec();
    if (qry.next())
        wasMaximized = qry.value(0).toBool();

    schemeNamespace = "";
    qry.prepare("SELECT value FROM settings WHERE setting = (?)");
    qry.addBindValue("org.scheme.namespace");
    qry.exec();
    if (qry.next())
        schemeNamespace = qry.value(0).toString();

    schemeText = "";
    qry.prepare("SELECT value FROM settings WHERE setting = (?)");
    qry.addBindValue("org.scheme.text");
    qry.exec();
    if (qry.next())
        schemeText = qry.value(0).toString();

    schemeNumber = "";
    qry.prepare("SELECT value FROM settings WHERE setting = (?)");
    qry.addBindValue("org.scheme.number");
    qry.exec();
    if (qry.next())
        schemeNumber = qry.value(0).toString();

    lastOrgNum = 10000;
    qry.prepare("SELECT value FROM settings WHERE setting = (?)");
    qry.addBindValue("last.organismnumber");
    qry.exec();
    if (qry.next())
        lastOrgNum = qry.value(0).toInt();

    qry.prepare("SELECT value FROM settings WHERE setting = (?)");
    qry.addBindValue("last.agent");
    qry.exec();
    if (qry.next())
        agent = qry.value(0).toString();
    if (agent.isEmpty())
        qDebug() << "The active agent was not properly loaded in DataEntry::setupDataEntry()";

    baseReverseGeocodeURL = "";
    qry.prepare("SELECT value FROM settings WHERE setting = (?)");
    qry.addBindValue("url.reversegeocode");
    qry.exec();
    if (qry.next())
        baseReverseGeocodeURL = qry.value(0).toString();

    if (!db.commit())
    {
        qDebug() << "Problem committing changes to database in DataEntry::setupDataEntry()";
        db.rollback();
    }

    if (appDir.isEmpty())
        qDebug() << "Error - local application directory cannot be found.";

    if (wasMaximized)
        this->showMaximized();

    QHashIterator<QString, QString> agentIt(agentHash);
    while (agentIt.hasNext()) {
        agentIt.next();
        agents.append(agentIt.key());
    }
    refreshAgentDropdowns();

    loadImageIDs();

    stateTwoLetter.insert("Alabama","AL");
    stateTwoLetter.insert("Alaska","AK");
    stateTwoLetter.insert("Arizona","AZ");
    stateTwoLetter.insert("Arkansas","AR");
    stateTwoLetter.insert("California","CA");
    stateTwoLetter.insert("Colorado","CO");
    stateTwoLetter.insert("Connecticut","CT");
    stateTwoLetter.insert("Delaware","DE");
    stateTwoLetter.insert("District of Columbia","DC");
    stateTwoLetter.insert("Florida","FL");
    stateTwoLetter.insert("Georgia","GA");
    stateTwoLetter.insert("Hawaii","HI");
    stateTwoLetter.insert("Idaho","ID");
    stateTwoLetter.insert("Illinois","IL");
    stateTwoLetter.insert("Indiana","IN");
    stateTwoLetter.insert("Iowa","IA");
    stateTwoLetter.insert("Kansas","KS");
    stateTwoLetter.insert("Kentucky","KY");
    stateTwoLetter.insert("Louisiana","LA");
    stateTwoLetter.insert("Maine","ME");
    stateTwoLetter.insert("Maryland","MD");
    stateTwoLetter.insert("Massachusetts","MA");
    stateTwoLetter.insert("Michigan","MI");
    stateTwoLetter.insert("Minnesota","MN");
    stateTwoLetter.insert("Mississippi","MS");
    stateTwoLetter.insert("Missouri","MO");
    stateTwoLetter.insert("Montana","MT");
    stateTwoLetter.insert("Nebraska","NE");
    stateTwoLetter.insert("Nevada","NV");
    stateTwoLetter.insert("New Hampshire","NH");
    stateTwoLetter.insert("New Jersey","NJ");
    stateTwoLetter.insert("New Mexico","NM");
    stateTwoLetter.insert("New York","NY");
    stateTwoLetter.insert("North Carolina","NC");
    stateTwoLetter.insert("North Dakota","ND");
    stateTwoLetter.insert("Ohio","OH");
    stateTwoLetter.insert("Oklahoma","OK");
    stateTwoLetter.insert("Oregon","OR");
    stateTwoLetter.insert("Pennsylvania","PA");
    stateTwoLetter.insert("Rhode Island","RI");
    stateTwoLetter.insert("South Carolina","SC");
    stateTwoLetter.insert("South Dakota","SD");
    stateTwoLetter.insert("Tennessee","TN");
    stateTwoLetter.insert("Texas","TX");
    stateTwoLetter.insert("Utah","UT");
    stateTwoLetter.insert("Vermont","VT");
    stateTwoLetter.insert("Virginia","VA");
    stateTwoLetter.insert("Washington","WA");
    stateTwoLetter.insert("West Virginia","WV");
    stateTwoLetter.insert("Wisconsin","WI");
    stateTwoLetter.insert("Wyoming","WY");

    QSqlQuery geonamesQuery;
    geonamesQuery.setForwardOnly(true);
    geonamesQuery.prepare("SELECT dwc_county, dcterms_identifier from geonames_admin");
    geonamesQuery.exec();
    while (geonamesQuery.next())
    {
        QString county = geonamesQuery.value(0).toString();
        QString county_id = geonamesQuery.value(1).toString();

        if (county.isEmpty() || county_id.isEmpty())
            continue;

        countyGeonameID.insert(county,county_id);
    }
}

void DataEntry::runExifTool()
{
    QString filesToScan = "";
    exifComplete = true;

    for (; exifIndex < imageFileNames.size(); exifIndex++)
    {
        if (filesToScan.length() + imageFileNames.at(exifIndex).length() < 31000)
        {
            filesToScan += "\"" + imageFileNames.at(exifIndex) + "\" ";
        }
        else
        {
            exifComplete = false;
            break;
        }
    }

    QString exifLocation = "";
    QSqlQuery qry;
    qry.prepare("SELECT value FROM settings WHERE setting = (?)");
    qry.addBindValue("path.exiftool");
    qry.exec();
    if (qry.next())
        exifLocation = qry.value(0).toString();

    QString exifParams = "-c \%+.6f -filename -datetimeoriginal -focallength -gpslatitude -gpslongitude -gpsaltitude -imagewidth -imageheight -createdate -modifydate -filemodifydate -T";
    QString exifCmd = "\"" + exifLocation + "\" " + filesToScan + exifParams;

    ps.start(exifCmd);
}

void DataEntry::exifToolFinished()
{
    exifOutput += ps.readAll();

    if(!exifComplete) {
        runExifTool();
    }
    else
    {
        // parse and store exifOutput's 11 columns of tab-delimited data

        QStringList rowsExifOutput = exifOutput.split("\n");
        rowsExifOutput.removeAll("");

        QSqlDatabase db = QSqlDatabase::database();
        db.transaction();

        int imagesIndex = 0;
        for(QString &row : rowsExifOutput)
        {
            QStringList columns = row.split("\t");
            if (columns.size() == 11)
            {
                if(columns.at(0).isNull() || columns.at(0).isEmpty())
                {
                    qDebug() << "Found a NULL or empty filename column";
                    continue;
                }

                // Convert empty EXIF columns from "-" to ""
                for (int i=0; i<columns.size(); i++)
                {
                    if (columns[i] == "-")
                        columns[i] = "";
                }

                Image newImage;
                newImage.fileName = columns.at(0);
                imageIndexHash.insert(columns.at(0),imagesIndex);
                // check if exiftool can output full path
                newImage.fileAndPath = imageHash.value(newImage.fileName); // fix this later
                QStringList dateTimeSplit;

                // EXIF date/time storage varies, so find one that's used
                // column 1 holds -datetimeoriginal
                // column 8 holds -createdate
                // column 9 holds -modifydate
                // column 10 holds -filemodifydate
                QList<int> itColumns;
                itColumns << 1 << 8 << 9 << 10;
                //for (int i : {1,8,9,10})
                for (int i : itColumns)
                {
                    if (!columns.at(i).isEmpty() && columns.at(i).contains(" "))
                    {
                        dateTimeSplit = columns.at(i).split(" ");
                        break;
                    }
                }
                QString dts = dateTimeSplit.at(1);
                dts.remove("\r");

                if(dateTimeSplit.size() == 2)
                {
                    QString date = dateTimeSplit.at(0);
                    date = date.replace(":","-");
                    QString time = dateTimeSplit.at(1);
                    time.remove("\r");
                    time = time.replace(".",":");
                    // save time zone if present: hh:mm:ss-hh:mm or hh:mm:ss
                    QString timeNoTZ = time;
                    QString tz = "";
                    if (time.contains("-"))
                    {
                        timeNoTZ = time.split("-").at(0);
                        tz = "-" + time.split("-").at(1);
                    }
                    else if (time.contains("+"))
                    {
                        timeNoTZ = time.split("+").at(0);
                        tz = "+" + time.split("+").at(1);
                    }

                    newImage.date = date;
                    newImage.time = timeNoTZ;
                    newImage.timezone = tz;
                    newImage.dcterms_created = date + "T" + time;

                }
                else
                {
                    qDebug() << columns.at(0) + ": Error with dateTimeSplit";
                }

                newImage.focalLength = columns.at(2);
                newImage.focalLength.remove(" mm");

                QString lat = columns.at(3);
                QString lon = columns.at(4);
                QString ele = columns.at(5);
                newImage.decimalLatitude = lat.replace("+","");
                newImage.decimalLongitude = lon.replace("+","");
                if (!ele.isEmpty())
                {
                    newImage.altitudeInMeters = ele.replace(" m Above Sea Level","");
                    newImage.altitudeInMeters = QString::number(qRound(newImage.altitudeInMeters.toDouble()));
                }
                newImage.width = columns.at(6);
                newImage.height = columns.at(7);
                if (newImage.decimalLatitude != "")
                    newImage.coordinateUncertaintyInMeters = "10";

                // create a unique image identifier
                QString base = newImage.fileName;
                QString nameSpace = nameSpaceHash.value(newImage.fileAndPath);

                // assign a new identifier if the image doesn't have one
                int numTrailing = 5;
                if (trailingCharsHash.contains(newImage.fileAndPath))
                {
                    numTrailing = trailingCharsHash.value(newImage.fileAndPath);
                }
                else
                {
                    qDebug() << newImage.fileAndPath + " was not in the trailingCharsHash. Using default value of '5'";
                }

                QString newIdentifier = base.split(".").at(0).right(numTrailing);
                newIdentifier = newIdentifier.remove(QRegExp("[^a-zA-Z\\d_-]"));
                newIdentifier = "http://bioimages.vanderbilt.edu/" + nameSpace + "/" + newIdentifier;

                // if the user set the numTrailing to 0 we do not want to attempt to use newIdentifier
                while (imageIDList.contains(newIdentifier) || numTrailing == 0)
                {
                    numTrailing = 5;
                    lastOrgNum++;
                    newIdentifier = "http://bioimages.vanderbilt.edu/" + nameSpace + "/" + QString::number(lastOrgNum);
                }
                newImage.identifier = newIdentifier;
                newImage.attributionLinkURL = newIdentifier + ".htm";
                imageIDList.append(newIdentifier);
                imageIDFilenameMap.insert(newImage.identifier,newImage.fileName);
                dateFilenameMap.insertMulti(newImage.dcterms_created,newImage.fileName);

                if(photographerHash.contains(newImage.fileAndPath)) {
                    newImage.photographerCode = photographerHash.value(newImage.fileAndPath);
                    newImage.copyrightOwnerID = newImage.photographerCode;
                    newImage.copyrightOwnerName = agentHash.value(newImage.photographerCode);
                    newImage.copyrightStatement = "(c) " + newImage.copyrightYear + " " + newImage.copyrightOwnerName;

                    newImage.credit = newImage.copyrightOwnerName + " http://bioimages.vanderbilt.edu/";
                }
                else
                    qDebug() << "Error: photographer not found for file: " + newImage.fileAndPath;

                images << newImage;

                QSqlQuery query;
                query.setForwardOnly(true);
                query.prepare("INSERT INTO images (fileName, focalLength, dwc_georeferenceRemarks, "
                              "dwc_decimalLatitude, dwc_decimalLongitude, geo_alt, exif_PixelXDimension, "
                              "exif_PixelYDimension, dwc_occurrenceRemarks, dwc_geodeticDatum, "
                              "dwc_coordinateUncertaintyInMeters, dwc_locality, dwc_countryCode, dwc_stateProvince, "
                              "dwc_county, dwc_informationWithheld, dwc_dataGeneralizations, dwc_continent, "
                              "geonamesAdmin, geonamesOther, dcterms_identifier, dcterms_modified, dcterms_title, "
                              "dcterms_description, ac_caption, photographerCode, dcterms_created, photoshop_Credit, "
                              "owner, dcterms_dateCopyrighted, dc_rights, xmpRights_Owner, ac_attributionLinkURL, "
                              "ac_hasServiceAccessPoint, usageTermsIndex, view, xmp_Rating, foaf_depicts, suppress) "
                              "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
                query.addBindValue(newImage.fileName);
                query.addBindValue(newImage.focalLength);
                query.addBindValue(newImage.georeferenceRemarks);
                query.addBindValue(newImage.decimalLatitude);
                query.addBindValue(newImage.decimalLongitude);
                query.addBindValue(newImage.altitudeInMeters);
                query.addBindValue(newImage.width);
                query.addBindValue(newImage.height);
                query.addBindValue(newImage.occurrenceRemarks);
                query.addBindValue(newImage.geodeticDatum);

                query.addBindValue(newImage.coordinateUncertaintyInMeters);
                query.addBindValue(newImage.locality);
                query.addBindValue(newImage.countryCode);
                query.addBindValue(newImage.stateProvince);
                query.addBindValue(newImage.county);
                query.addBindValue(newImage.informationWithheld);
                query.addBindValue(newImage.dataGeneralizations);
                query.addBindValue(newImage.continent);
                query.addBindValue(newImage.geonamesAdmin);
                query.addBindValue(newImage.geonamesOther);

                query.addBindValue(newImage.identifier);
                query.addBindValue(newImage.lastModified);
                query.addBindValue(newImage.title);
                query.addBindValue(newImage.description);
                query.addBindValue(newImage.caption);
                query.addBindValue(newImage.photographerCode);
                query.addBindValue(newImage.dcterms_created);
                query.addBindValue(newImage.credit);
                query.addBindValue(newImage.copyrightOwnerID);
                query.addBindValue(newImage.copyrightYear);

                query.addBindValue(newImage.copyrightStatement);
                query.addBindValue(newImage.copyrightOwnerName);
                query.addBindValue(newImage.attributionLinkURL);
                query.addBindValue(newImage.urlToHighRes);
                query.addBindValue(newImage.usageTermsIndex);
                query.addBindValue(newImage.imageView);
                query.addBindValue(newImage.rating);
                query.addBindValue(newImage.depicts);
                query.addBindValue(newImage.suppress);

                query.exec();

                imagesIndex++;
            }
            else
                qDebug() << "Found a row without 10 columns: " + columns.at(0);
        }

        QSqlQuery qry;
        qry.prepare("INSERT OR REPLACE INTO settings (setting, value) VALUES (?, ?)");
        qry.addBindValue("last.organismnumber");
        qry.addBindValue(lastOrgNum);
        qry.exec();

        if (!db.commit())
        {
            qDebug() << "In on_exifTool_finish(): Problem committing changes to database. Data may be lost.";
            db.rollback();
        }

        qDebug() << "Exiftool finished. Now generating thumbnails.";
        generateThumbnails();
    }
}

void DataEntry::iconify(const QStringList &imageFileNames)
{
    QImageReader defaultImageReader(":/noimage.jpg");
    int defaultWid = defaultImageReader.size().width();
    int defaultHei = defaultImageReader.size().height();
    defaultImageReader.setClipRect(QRect(0,(defaultHei-defaultWid)/2,defaultWid,defaultWid));
    defaultImageReader.setScaledSize(QSize(100,100));
    QPixmap defaultThumb;
    defaultThumb = defaultThumb.fromImage(defaultImageReader.read());
    QIcon defaultIcon(defaultThumb);
    defaultIcon.addPixmap(defaultThumb);

    // load thumbnail cache
    QFile thumbFile(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/data/thumbnails.dat");
    QHash<QString,QIcon> iconCache;
    QList<QString> imageURIList;
    QList<QIcon> iconList;
    bool thumbFileModified = false;
    if (thumbFile.exists())
    {
        thumbFile.open(QIODevice::ReadOnly);
        QDataStream thumbStream(&thumbFile);
        thumbStream.setVersion(QDataStream::Qt_5_5);

        thumbStream >> imageURIList >> iconList;
        thumbFile.close();

        if (imageURIList.size() == iconList.size())
        {
            for (int i = 0; i < imageURIList.size(); i++)
            {
                iconCache.insert(imageURIList.at(i),iconList.at(i));
            }
        }
        else
        {
            QString debugMsg = "Error loading thumbnail cache. imageURIList (" + QString::number(imageURIList.size()) + ") and pmList (" + QString::number(iconList.size()) + ") have different sizes.";
            qDebug() << debugMsg;
        }
    }
    else
    {
        qDebug() << "No thumbnail cache exists.";
    }

    QSqlDatabase db = QSqlDatabase::database();
    if (!fromEditExisting)
    {
        db.transaction();
    }

    thumbWidgetItems.clear();
    for (int i = 0; i < imageFileNamesSize; i++)
    {
        QString file = imageFileNames[i];
        QString base = imageHash.key(file);
        int imInd = -1;
        if (imageIndexHash.contains(base))
            imInd = imageIndexHash.value(base);
        else
            qDebug() << "Error in the imageIndexHash!";

        // only fill out the image info if we didn't come from Edit Existing
        if (!fromEditExisting)
        {
            if(imInd != -1)
            {
                QSqlQuery query;
                query.setForwardOnly(true);
                query.prepare("INSERT OR REPLACE INTO images (fileName, focalLength, dwc_georeferenceRemarks, "
                              "dwc_decimalLatitude, dwc_decimalLongitude, geo_alt, exif_PixelXDimension, "
                              "exif_PixelYDimension, dwc_occurrenceRemarks, dwc_geodeticDatum, "
                              "dwc_coordinateUncertaintyInMeters, dwc_locality, dwc_countryCode, dwc_stateProvince, "
                              "dwc_county, dwc_informationWithheld, dwc_dataGeneralizations, dwc_continent, "
                              "geonamesAdmin, geonamesOther, dcterms_identifier, dcterms_modified, dcterms_title, "
                              "dcterms_description, ac_caption, photographerCode, dcterms_created, photoshop_Credit, "
                              "owner, dcterms_dateCopyrighted, dc_rights, xmpRights_Owner, ac_attributionLinkURL, "
                              "ac_hasServiceAccessPoint, usageTermsIndex, view, xmp_Rating, foaf_depicts, suppress) "
                              "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?) "
                              "WHERE dcterms_identifier = (?)");
                query.addBindValue(images[imInd].fileName);
                query.addBindValue(images[imInd].focalLength);
                query.addBindValue(images[imInd].georeferenceRemarks);
                query.addBindValue(images[imInd].decimalLatitude);
                query.addBindValue(images[imInd].decimalLongitude);
                query.addBindValue(images[imInd].altitudeInMeters);
                query.addBindValue(images[imInd].width);
                query.addBindValue(images[imInd].height);
                query.addBindValue(images[imInd].occurrenceRemarks);
                query.addBindValue(images[imInd].geodeticDatum);

                query.addBindValue(images[imInd].coordinateUncertaintyInMeters);
                query.addBindValue(images[imInd].locality);
                query.addBindValue(images[imInd].countryCode);
                query.addBindValue(images[imInd].stateProvince);
                query.addBindValue(images[imInd].county);
                query.addBindValue(images[imInd].informationWithheld);
                query.addBindValue(images[imInd].dataGeneralizations);
                query.addBindValue(images[imInd].continent);
                query.addBindValue(images[imInd].geonamesAdmin);
                query.addBindValue(images[imInd].geonamesOther);

                query.addBindValue(images[imInd].identifier);
                query.addBindValue(images[imInd].lastModified);
                query.addBindValue(images[imInd].title);
                query.addBindValue(images[imInd].description);
                query.addBindValue(images[imInd].caption);
                query.addBindValue(images[imInd].photographerCode);
                query.addBindValue(images[imInd].dcterms_created);
                query.addBindValue(images[imInd].credit);
                query.addBindValue(images[imInd].copyrightOwnerID);
                query.addBindValue(images[imInd].copyrightYear);

                query.addBindValue(images[imInd].copyrightStatement);
                query.addBindValue(images[imInd].copyrightOwnerName);
                query.addBindValue(images[imInd].attributionLinkURL);
                query.addBindValue(images[imInd].urlToHighRes);
                query.addBindValue(images[imInd].usageTermsIndex);
                query.addBindValue(images[imInd].imageView);
                query.addBindValue(images[imInd].rating);
                query.addBindValue(images[imInd].depicts);
                query.addBindValue(images[imInd].suppress);

                query.addBindValue(images[imInd].identifier);

                query.exec();
            }
            else
                qDebug() << "Error with imageIndexHash call";
        }

        QString identifier = images[imInd].identifier;
        if (iconCache.contains(identifier))
        {
            ui->thumbWidget->addItem(new QListWidgetItem(iconCache.value(identifier),base));
        }
        else if (QFileInfo::exists(file))
        {
            // Scale the images down to thumbnail size
            QImageReader imageReader(file);
            QSize fullSize = imageReader.size();
            int wid = fullSize.width();
            int hei = fullSize.height();

            if (wid == hei)
            {
                imageReader.setScaledSize(QSize(100,100));
            }
            else if (wid > hei)
            {
                imageReader.setClipRect(QRect((wid-hei)/2,0,hei,hei));
                imageReader.setScaledSize(QSize(100,100));
            }
            else
            {
                imageReader.setClipRect(QRect(0,(hei-wid)/2,wid,wid));
                imageReader.setScaledSize(QSize(100,100));
            }

            if (imageReader.canRead())
            {
                QImage img = imageReader.read();
                QPixmap thumb;
                thumb = thumb.fromImage(img);
                imageURIList.append(identifier);
                QIcon thumbIcon(thumb);
                iconList.append(thumbIcon);
                iconCache.insert(identifier,thumbIcon);
                thumbFileModified = true;
                ui->thumbWidget->addItem(new QListWidgetItem(thumbIcon,base));
            }
            else
            {
                qDebug() << __LINE__ << "Could not load image from file: " + file;
                ui->thumbWidget->addItem(new QListWidgetItem(QIcon(defaultThumb),base));
            }
        }
        else
        {
            ui->thumbWidget->addItem(new QListWidgetItem(QIcon(defaultThumb),base));
        }
        thumbWidgetItems.append(base);
    }

    if (!db.commit() && !fromEditExisting)
    {
        qDebug() << "In iconify(): Problem committing changes to database: " + db.lastError().text();
        db.rollback();
    }

    emit iconifyDone();

    // now let's save the thumbnail cache if any changes were made
    if (thumbFileModified)
    {
        if (imageURIList.size() != iconList.size())
        {
            qDebug() << "Error saving thumbnails: imageURIList and iconList are different sizes.";
            return;
        }
        thumbFile.open(QIODevice::WriteOnly);
        QDataStream thumbOut(&thumbFile);
        thumbOut << imageURIList << iconList;
        thumbFile.close();
    }

}

QString DataEntry::modifiedNow()
{
    // Find and set the current time for lastModified
    QDateTime localTime = QDateTime::currentDateTime();
    QDateTime UTCTime = localTime;
    UTCTime.setTimeSpec(Qt::UTC);

    QString currentDateTime = localTime.toString("yyyy-MM-dd'T'hh:mm:ss");
    int offset = localTime.secsTo(UTCTime);
    int tzHourOffset = abs(offset / 3600);
    int tzMinOffset = abs(offset % 3600) / 60;

    QString timezoneOffset = "+";
    if (offset < 0)
        timezoneOffset = "-";
    if (tzHourOffset < 10)
        timezoneOffset = timezoneOffset + "0" + QString::number(tzHourOffset) + ":";
    else
        timezoneOffset = timezoneOffset + QString::number(tzHourOffset) + ":";
    if (tzMinOffset < 10)
        timezoneOffset = timezoneOffset + "0" + QString::number(tzMinOffset);
    else
        timezoneOffset = timezoneOffset + QString::number(tzMinOffset);

    return currentDateTime + timezoneOffset;
}

void DataEntry::generateThumbnails()
{
    connect(this,SIGNAL(iconifyDone()),this,SLOT(on_iconifyDone()));
    auto future = QtConcurrent::run(this, &DataEntry::iconify, imageFileNames);
}

void DataEntry::refreshImageLabel()
{
    viewedImage = 0;
    QList<QListWidgetItem*> itemList = ui->thumbWidget->selectedItems();
    int numSelect = itemList.count();
    ui->nSelectedLabel->setText(QString::number(numSelect)+ " selected");

    if (numSelect >= 1)
    {
        QString fileName = imageHash.value(itemList.at(0)->text());
        if (!fileName.isEmpty())
        {
            scaleFactor = 1;
            int w = ui->scrollArea->width();
            int h = ui->scrollArea->height();

            QString fileToRead = fileName;
            QFileInfo fileInfo(fileToRead);
            // use the thumbnail if no other image can be found
            if (!fileInfo.isFile())
            {
                // limit dimensions to 300x300 to somewhat reduce pixelation due to upscaling
                if (w > 300)
                    w = 300;
                if (h > 300)
                    h = 300;

                QPixmap pscaled;
                pscaled = itemList.at(0)->icon().pixmap(100,100).scaled(w,h,Qt::KeepAspectRatio, Qt::SmoothTransformation);
                imageLabel->resize(w,h);
                imageLabel->setPixmap(pscaled);
                return;
            }

            QImageReader imageReader(fileToRead);
            imageReader.setScaledSize(QSize(w,h));
            QSize fullSize = imageReader.size();
            int wid = fullSize.width();
            int hei = fullSize.height();

            int setW;
            int setH;

            if (w == h && wid == hei)
            {
                setH = h;
                setW = w;
            }
            else if (float(wid)/hei > float(w)/h)
            {
                setH = (w*hei)/wid;
                setW = w;
            }
            else
            {
                setH = h;
                setW = (h*wid)/hei;
            }

            imageReader.setScaledSize(QSize(setW,setH));

            if (imageReader.canRead())
            {
                QImage img = imageReader.read();
                QPixmap pscaled;
                pscaled = pscaled.fromImage(img);
                imageLabel->resize(setW,setH);
                imageLabel->setPixmap(pscaled);
            }
            else
            {
                qDebug() << __LINE__  << "Could not load image from file: " + fileName;
                // use the thumbnail instead
                // limit dimensions to 300x300 to somewhat reduce pixelation due to upscaling
                if (w > 300)
                    w = 300;
                if (h > 300)
                    h = 300;

                QPixmap pscaled;
                pscaled = itemList.at(0)->icon().pixmap(100,100).scaled(w,h,Qt::KeepAspectRatio, Qt::SmoothTransformation);
                imageLabel->resize(w,h);
                imageLabel->setPixmap(pscaled);
                return;
            }
        }
    }
}

void DataEntry::refreshInputFields()
{
    QList<QListWidgetItem*> itemList = ui->thumbWidget->selectedItems();
    int numSelect = itemList.count();

    ui->tsnIDNotFound->setVisible(false);
    ui->setOrganismIDFirst->setVisible(false);
    ui->identificationRemarks->clear();
    ui->cameoText->clear();
    ui->organismRemarks->clear();
    ui->organismName->clear();
    ui->organismScope->clear();
    ui->htmlNote->clear();
    ui->organismLatLong->clear();
    ui->organismAltitude->clear();
    ui->catalogNumber->clear();

    ui->vernacular->clear();
    ui->kingdom->clear();
    ui->className->clear();
    ui->order->clear();
    ui->family->clear();
    ui->primula->clear();
    ui->specificEpithet->clear();
    ui->infraspecificEpithet->clear();
    ui->taxonRank->clear();
    ui->tsnID->clear();

    ui->organismGeoreferenceRemarks->setToolTip("");
    ui->collectionCode->setToolTip("");

    ui->identifiedBy->setToolTip("");
    ui->dateIdentified->setToolTip("");
    ui->dateIdentified->clear();
    ui->nameAccordingToID->setToolTip("");
    ui->establishmentMeans->setToolTip("");

    ui->image_filename_box->setToolTip("");
    ui->photographer->setToolTip("");
    ui->geonamesAdmin->setToolTip("");
    ui->copyrightOwner->setToolTip("");
    ui->usageTermsBox->setToolTip("");

    currentDeterminations.clear();
    currentDetermination = 0;
    QStringList determList;
    ui->numberOfDeterminations->setText("0 of 0");

    for (auto sim : similarImages)
    {
        ui->thumbWidget->item(sim)->setBackground(Qt::NoBrush);
    }
    similarImages.clear();

    if (numSelect == 1)
    {
        // itemList.at(0)->text() holds the base filename
        int h = imageIndexHash.value(itemList.at(0)->text());
        if (h == -1)
        {
            qDebug() << "Error: hashIndex not found.";
            return;
        }

        QSqlQuery findImages;
        findImages.prepare("SELECT fileName FROM images WHERE foaf_depicts = (?)");
        findImages.addBindValue(images[h].depicts);
        findImages.exec();

        while (findImages.next())
        {
            int row = thumbWidgetItems.indexOf(findImages.value(0).toString());

            // not all of an organism's images are necessarily loaded
            if (row == -1)
                continue;

            similarImages.append(row);
            ui->thumbWidget->item(row)->setBackground(QColor(255,165,79));
            // 255,127,36 (brighter orange), 255,165,79 (lighter), 238,118,33 (darker)
        }

        // let's fetch all the image values from the database
        Image image;
        QSqlQuery imageChanges;
        imageChanges.prepare("SELECT * FROM images WHERE dcterms_identifier = (?) LIMIT 1");
        QString identifier = imageIDFilenameMap.key(itemList.at(0)->text());
        imageChanges.addBindValue(identifier);
        imageChanges.exec();
        if (imageChanges.next())
        {
            image.fileName = imageChanges.value(0).toString();
            image.focalLength = imageChanges.value(1).toString();
            image.georeferenceRemarks = imageChanges.value(2).toString();
            image.decimalLatitude = imageChanges.value(3).toString();
            image.decimalLongitude = imageChanges.value(4).toString();
            image.altitudeInMeters = imageChanges.value(5).toString();
            image.width = imageChanges.value(6).toString();
            image.height = imageChanges.value(7).toString();
            image.occurrenceRemarks = imageChanges.value(8).toString();
            image.geodeticDatum = imageChanges.value(9).toString();

            image.coordinateUncertaintyInMeters = imageChanges.value(10).toString();
            image.locality = imageChanges.value(11).toString();
            image.countryCode = imageChanges.value(12).toString();
            image.stateProvince = imageChanges.value(13).toString();
            image.county = imageChanges.value(14).toString();
            image.informationWithheld = imageChanges.value(15).toString();
            image.dataGeneralizations = imageChanges.value(16).toString();
            image.continent = imageChanges.value(17).toString();
            image.geonamesAdmin = imageChanges.value(18).toString();
            image.geonamesOther = imageChanges.value(19).toString();

            image.identifier = imageChanges.value(20).toString();
            image.lastModified = imageChanges.value(21).toString();
            image.title = imageChanges.value(22).toString();
            image.description = imageChanges.value(23).toString();
            image.caption = imageChanges.value(24).toString();
            image.photographerCode = imageChanges.value(25).toString();
            image.dcterms_created = imageChanges.value(26).toString();
            QStringList dateTimeSplit = image.dcterms_created.split("T");
            if(dateTimeSplit.size() == 2)
            {
                QString date = dateTimeSplit.at(0);
                image.date = date;
                QString time = dateTimeSplit.at(1);
                QString timeNoTZ = time;
                QString tz = "";
                if (time.contains("-"))
                {
                    timeNoTZ = time.split("-").at(0);
                    tz = "-" + time.split("-").at(1);
                }
                else if (time.contains("+"))
                {
                    timeNoTZ = time.split("+").at(0);
                    tz = "+" + time.split("+").at(1);
                }
                image.time = timeNoTZ;
                image.timezone = tz;
            }
            else
                image.date = image.dcterms_created;

            image.credit = imageChanges.value(27).toString();
            image.copyrightOwnerID = imageChanges.value(28).toString();
            image.copyrightYear = imageChanges.value(29).toString();

            image.copyrightStatement = imageChanges.value(30).toString();
            image.copyrightOwnerName = imageChanges.value(31).toString();
            image.attributionLinkURL = imageChanges.value(32).toString();
            image.urlToHighRes = imageChanges.value(33).toString();
            image.usageTermsIndex = imageChanges.value(34).toString();
            image.imageView = imageChanges.value(35).toString();
            image.rating = imageChanges.value(36).toString();
            image.depicts = imageChanges.value(37).toString();
            image.suppress = imageChanges.value(38).toString();

            QStringList groupPartView = numbersToView(image.imageView);
            if (groupPartView.size() == 3)
            {
                image.groupOfSpecimen = groupPartView.at(0);
                image.portionOfSpecimen = groupPartView.at(1);
                image.viewOfSpecimen = groupPartView.at(2);
            }
        }
        else
        {
            // no record of the selected image is in the database. this is not good
            QString warning = "No image record with dcterms_identifier '" + identifier + "' was found in the database.\n\n"
                           "Would you like to remove the thumbnail from the view?";
            if (QMessageBox::Yes == QMessageBox(QMessageBox::Information, "No record found for image",
                                                warning,
                                                QMessageBox::Yes|QMessageBox::No).exec())
            {
                // Get current selected item
                QListWidgetItem *item = ui->thumbWidget->takeItem(ui->thumbWidget->currentRow());
                QString filename = item->text();
                // DON'T remove from "images" QList or it messes up the imageIndexHash!
                // Do remove from imageIndexHash
                imageIndexHash.remove(filename);
                // Remove from imageIDFilenameMap
                imageIDFilenameMap.remove(identifier);
                // Remove from dateFilenameMap
                dateFilenameMap.remove(dateFilenameMap.key(filename));
                // And finally remove the thumbnail
                delete item;
            }
            return;
        }

        // now set all fields based on the image class

        // frame 2 fields
        ui->organismID->setText(image.depicts);
        ui->organismID->setToolTip(image.depicts);

        // get the fields stored in Organism rather than Image
        // make sure to clear combo box fields if no organisms are selected
        QString orgLatLon;
        QString orgAlt;
        if (!image.depicts.isEmpty())
        {
            QSqlQuery query;
            query.prepare("SELECT dwc_establishmentMeans, dwc_organismRemarks, dwc_collectionCode, "
                          "dwc_catalogNumber, dwc_georeferenceRemarks, dwc_decimalLatitude, "
                          "dwc_decimalLongitude, geo_alt, dwc_organismName, dwc_organismScope, "
                          "cameo, notes FROM organisms WHERE dcterms_identifier = (?) LIMIT 1");
            query.addBindValue(image.depicts);
            query.exec();

            if (query.next())
            {
                QString indLat = query.value(5).toString();
                QString indLong = query.value(6).toString();
                orgAlt = query.value(7).toString();

                orgLatLon = indLat + ", " + indLong;
                if (indLat == "" || indLong == "")
                {
                    orgLatLon = "";
                }
                ui->organismLatLong->setText(orgLatLon);
                ui->organismAltitude->setText(orgAlt);

                ui->organismRemarks->setText(query.value(1).toString());
                ui->organismRemarks->setToolTip(query.value(1).toString());
                ui->organismName->setText(query.value(8).toString());
                ui->organismScope->setText(query.value(9).toString());
                ui->htmlNote->setText(query.value(11).toString());
                ui->htmlNote->setToolTip(query.value(11).toString());
                ui->organismGeoreferenceRemarks->setCurrentText(query.value(4).toString());
                ui->organismGeoreferenceRemarks->setToolTip(query.value(4).toString());
                ui->cameoText->setText(query.value(10).toString());
                ui->cameoText->setToolTip(query.value(10).toString());
                ui->collectionCode->setCurrentText(query.value(2).toString());
                ui->collectionCode->setToolTip(singleResult("dc_contributor","agents","dcterms_identifier",query.value(2).toString()));
                ui->catalogNumber->setText(query.value(3).toString());
                ui->establishmentMeans->setCurrentText(query.value(0).toString());
                ui->establishmentMeans->setToolTip(query.value(0).toString());

                currentDeterminations.clear();
                QSqlQuery dQuery;
                dQuery.prepare("SELECT identifiedBy, dwc_dateIdentified, dwc_identificationRemarks, tsnID, "
                               "nameAccordingToID, dcterms_modified, suppress FROM determinations "
                               "WHERE dsw_identified = (?) ORDER BY dwc_dateIdentified DESC");

                dQuery.addBindValue(image.depicts);
                dQuery.exec();

                int numFound = 0;
                while (dQuery.next())
                {
                    numFound++;
                    if (numFound == 1)
                    {
                        ui->identifiedBy->setText(dQuery.value(0).toString());
                        QString identifiedByFull = singleResult("dc_contributor","agents","dcterms_identifier",dQuery.value(0).toString());
                        ui->identifiedBy->setToolTip(identifiedByFull);
                        ui->dateIdentified->setText(dQuery.value(1).toString());
                        QString s = sensuHash.value(dQuery.value(4).toString()) + " (" + dQuery.value(4).toString() + ")";
                        ui->nameAccordingToID->setCurrentText(s);
                        ui->nameAccordingToID->setToolTip(s);
                        ui->identificationRemarks->setText(dQuery.value(2).toString());
                        ui->identificationRemarks->setToolTip(dQuery.value(2).toString());
                        setTaxaFromTSN(dQuery.value(3).toString());
                        currentDetermination = 1;
                    }

                    Determination determination;
                    determination.identified = image.depicts;
                    determination.identifiedBy = dQuery.value(0).toString();
                    determination.dateIdentified = dQuery.value(1).toString();
                    determination.identificationRemarks =  dQuery.value(2).toString();
                    determination.tsnID = dQuery.value(3).toString();
                    determination.nameAccordingToID = dQuery.value(4).toString();
                    determination.lastModified = dQuery.value(5).toString();
                    determination.suppress = dQuery.value(6).toString();

                    currentDeterminations.append(determination);
                }
                if (currentDeterminations.size() > 0)
                {
                    ui->numberOfDeterminations->setText("1 of " + QString::number(currentDeterminations.size()));
                }


            }
            // if no organism was found then certain fields weren't properly cleared. do so now
            else
            {
                ui->identifiedBy->setText("");
                ui->nameAccordingToID->setCurrentText("");
                ui->organismGeoreferenceRemarks->setCurrentText("");
                ui->collectionCode->setCurrentText("");
                ui->establishmentMeans->setCurrentText("");
            }
        }
        else
        {
            ui->identifiedBy->setText("");
            ui->nameAccordingToID->setCurrentText("");
            ui->organismGeoreferenceRemarks->setCurrentText("");
            ui->collectionCode->setCurrentText("");
            ui->establishmentMeans->setCurrentText("");
        }

        //frame 4
        pauseSavingView = true;
        ui->groupOfSpecimenBox->setCurrentText(image.groupOfSpecimen);
        refreshSpecimenGroup(image.groupOfSpecimen);
        ui->partOfSpecimenBox->setCurrentText(image.portionOfSpecimen);
        refreshSpecimenPart(image.portionOfSpecimen);
        ui->viewOfSpecimenBox->setCurrentText(image.viewOfSpecimen);
        refreshSpecimenView(image.viewOfSpecimen);
        pauseSavingView = false;

        ui->imageCaption->setText(image.caption);

        if (image.georeferenceRemarks == "Location inferred from organism coordinates.")
        {
            ui->imageLatAndLongBox->setText(orgLatLon);
            ui->elevation->setText(orgAlt);
        }
        else
        {
            QSqlQuery getImageLoc;
            getImageLoc.prepare("SELECT dwc_decimalLatitude, dwc_decimalLongitude, geo_alt "
                                "FROM images WHERE dcterms_identifier = (?) LIMIT 1");
            getImageLoc.addBindValue(image.identifier);
            getImageLoc.exec();

            if (getImageLoc.next())
            {
                QString lat = getImageLoc.value(0).toString();
                QString lon = getImageLoc.value(1).toString();
                QString alt = getImageLoc.value(2).toString();
                QString latCommaLon = lat + ", " + lon;
                if (lat == "" || lon == "")
                {
                    latCommaLon = "";
                }
                ui->imageLatAndLongBox->setText(latCommaLon);
                ui->elevation->setText(alt);
            }
        }
        ui->coordinateUncertaintyInMetersBox->setCurrentText(image.coordinateUncertaintyInMeters);
        ui->image_county_box->setText(image.county);
        ui->image_stateProvince_box->setText(image.stateProvince);
        ui->image_countryCode_box->setText(image.countryCode);
        ui->image_continent_box->setText(image.continent);

        ui->locality->setText(image.locality);
        ui->imageGeoreferenceRemarks->setCurrentText(image.georeferenceRemarks);
        ui->image_informationWithheld_box->setCurrentText(image.informationWithheld);
        ui->image_dataGeneralization_box->setCurrentText(image.dataGeneralizations);

        ui->image_occurrenceRemarks_box->setText(image.occurrenceRemarks);

        // frame 6 fields
        ui->image_filename_box->setText(image.fileName);
        ui->image_filename_box->setToolTip(image.identifier);
        ui->image_date_box->setText(image.date);
        ui->image_time_box->setText(image.time);
        ui->image_timezone_box->setText(image.timezone);
        ui->image_width_box->setText(image.width);
        ui->image_height_box->setText(image.height);
        ui->image_focalLength_box->setText(image.focalLength);
        ui->photographer->setCurrentText(image.photographerCode);
        ui->photographer->setToolTip(singleResult("dc_contributor","agents","dcterms_identifier",image.photographerCode));

        // lower frame 6
        QSqlQuery getTitle;
        getTitle.prepare("SELECT dcterms_title, dcterms_description "
                            "FROM images WHERE dcterms_identifier = (?) LIMIT 1");
        getTitle.addBindValue(image.identifier);
        getTitle.exec();

        QString title = "";
        QString description = "";
        if (getTitle.next())
        {
            title = getTitle.value(0).toString();
            description = getTitle.value(1).toString();
        }
        ui->imageTitle->setText(title);
        ui->imageTitle->setToolTip(title);
        ui->imageDescription->setText(description);
        ui->imageDescription->setToolTip(description);

        ui->geodedicDatum->setText(image.geodeticDatum);
        ui->geonamesAdmin->setText(image.geonamesAdmin);
        ui->geonamesAdmin->setToolTip(singleResult("dwc_county","geonames_admin","dcterms_identifier",image.geonamesAdmin));
        ui->geonamesOther->setText(image.geonamesOther);
        ui->copyrightOwner->setCurrentText(image.copyrightOwnerID);
        ui->copyrightOwner->setToolTip(singleResult("dc_contributor","agents","dcterms_identifier",image.copyrightOwnerID));
        ui->copyrightYear->setText(image.copyrightYear);
        ui->copyrightStatement->setText(image.copyrightStatement);
        ui->usageTermsBox->setCurrentIndex(image.usageTermsIndex.toInt());
        ui->photoshop_Credit->setText(image.credit);
        ui->photoshop_Credit->setToolTip(image.credit);
        ui->highResURL->setText(image.urlToHighRes);
        ui->highResURL->setToolTip(image.urlToHighRes);
    }
    else if (numSelect > 1)
    {
        // if a field is the same for all selected images, display it; otherwise <multiple> or similar

        //frame 2 fields
        // Organism ID
        QStringList fieldList { };  // list of values for this field
        QStringList depictions { };
        QSqlDatabase db = QSqlDatabase::database();
        db.transaction();
        foreach (QListWidgetItem* img, itemList)
        {
            QSqlQuery depictsQry;
            depictsQry.prepare("SELECT foaf_depicts FROM images WHERE dcterms_identifier = (?)");
            depictsQry.addBindValue(imageIDFilenameMap.key(img->text()));
            depictsQry.exec();
            if (depictsQry.next())
            {
                fieldList.append(depictsQry.value(0).toString());
                depictions.append(depictsQry.value(0).toString());
            }
        }
        if (!db.commit())
        {
            qDebug() << "In refreshInputFields(): Problem with depictsQry.";
            db.rollback();
        }
        fieldList.removeDuplicates();
        depictions.removeDuplicates();
        if (fieldList.length() > 49)
        {
            QStringList tmpList;
            for (int iTrim = 0; iTrim < 50; iTrim++)
            {
                tmpList.append(fieldList.at(iTrim));
            }
            fieldList = tmpList;
            fieldList.append("...and more");
        }
        ui->organismID->setText(simplifyField(fieldList));
        ui->organismID->setToolTip(fieldList.join("\n"));

        //get the fields stored in Organism rather than Image
        QStringList tsnIDList { };
        QStringList identifiedByList { };
        QStringList dateIdentifiedList { };
        QStringList nameAccordingToIDList { };
        QStringList identificationRemarksList { };
        QStringList cameoList { };
        QStringList organismRemarksList { };
        QStringList organismNameList { };
        QStringList organismScopeList { };
        QStringList noteList { };
        QStringList organismLatLongList { };
        QStringList organismAltList { };
        QStringList organismGeorefRemarksList { };
        QStringList collectionCodeList { };
        QStringList catalogNumberList { };
        QStringList establishmentMeansList { };
        QStringList vernacularList { };
        QStringList kingdomList { };
        QStringList classNameList { };
        QStringList orderList { };
        QStringList familyList { };
        QStringList primulaList { };
        QStringList specificEpithetList { };
        QStringList infraspecificEpithetList { };
        QStringList taxonRankList { };

        db.transaction();
        for (auto depiction : depictions)
        {
            QSqlQuery query;
            query.setForwardOnly(true);
            query.prepare("SELECT dwc_establishmentMeans, dwc_organismRemarks, dwc_collectionCode, "
                          "dwc_catalogNumber, dwc_georeferenceRemarks, dwc_decimalLatitude, "
                          "dwc_decimalLongitude, geo_alt, dwc_organismName, dwc_organismScope, "
                          "cameo, notes FROM organisms WHERE dcterms_identifier = (?) LIMIT 1");
            query.addBindValue(depiction);
            query.exec();

            // begin replacement
            if (query.next())
            {
                QString orgLat = query.value(5).toString();
                QString orgLong = query.value(6).toString();
                QString orgAlt = query.value(7).toString();

                cameoList.append(query.value(10).toString());
                organismRemarksList.append(query.value(1).toString());
                organismNameList.append(query.value(8).toString());
                organismScopeList.append(query.value(9).toString());
                noteList.append(query.value(11).toString());

                QString orgLatLon = orgLat + ", " + orgLong;
                if (orgLat == "" || orgLong == "")
                {
                    orgLatLon = "";
                }
                organismLatLongList.append(orgLatLon);
                organismAltList.append(orgAlt);
                organismGeorefRemarksList.append(query.value(4).toString());
                collectionCodeList.append(query.value(2).toString());
                catalogNumberList.append(query.value(3).toString());
                establishmentMeansList.append(query.value(0).toString());

                // determination
                QSqlQuery dQuery;
                dQuery.setForwardOnly(true);
                dQuery.prepare("SELECT identifiedBy, dwc_dateIdentified, dwc_identificationRemarks, tsnID, "
                               "nameAccordingToID, dcterms_modified, suppress FROM determinations "
                               "WHERE dsw_identified = (?) ORDER BY dwc_dateIdentified DESC");
                dQuery.addBindValue(depiction);
                dQuery.exec();

                if (dQuery.next())
                {
                    bool tsnIDFound = false;
                    QString firstDeterm(depiction + "|" + dQuery.value(0).toString() + "|" + dQuery.value(3).toString());
                    if (determList.contains(firstDeterm))
                        continue;
                    determList.append(firstDeterm);
                    identifiedByList.append(dQuery.value(0).toString());
                    dateIdentifiedList.append(dQuery.value(1).toString());
                    QString s = sensuHash.value(dQuery.value(4).toString()) + " (" + dQuery.value(4).toString() + ")";
                    nameAccordingToIDList.append(s);
                    identificationRemarksList.append(dQuery.value(2).toString());
                    QString tsnID = dQuery.value(3).toString();

                    if (!tsnID.isEmpty())
                    {
                        tsnIDList.append(tsnID);
                        QSqlQuery nQuery;
                        nQuery.setForwardOnly(true);
                        nQuery.prepare("SELECT dwc_kingdom, dwc_class, dwc_order, dwc_family, dwc_genus, dwc_specificEpithet, dwc_infraspecificEpithet, dwc_taxonRank, dwc_vernacularName FROM taxa WHERE dcterms_identifier = (?) LIMIT 1");
                        nQuery.addBindValue(tsnID);
                        nQuery.exec();
                        if (nQuery.next())
                        {
                            tsnIDFound = true;
                            QString kingdom = nQuery.value(0).toString();
                            QString className = nQuery.value(1).toString();
                            QString order = nQuery.value(2).toString();
                            QString family = nQuery.value(3).toString();
                            QString genus = nQuery.value(4).toString();
                            QString species = nQuery.value(5).toString();
                            QString infraspecific = nQuery.value(6).toString();
                            QString taxonRank = nQuery.value(7).toString();
                            QString commonName = nQuery.value(8).toString();

                            vernacularList.append(commonName);
                            kingdomList.append(kingdom);
                            classNameList.append(className);
                            orderList.append(order);
                            familyList.append(family);
                            primulaList.append(genus);
                            specificEpithetList.append(species);
                            infraspecificEpithetList.append(infraspecific);
                            taxonRankList.append(taxonRank);
                        }
                    }
                    else
                    {
                        // there was no tsnID so we don't have to look in the taxa table for its information
                        tsnIDFound = true;
                    }

                    while (dQuery.next())
                    {
                        QString determ(depiction + "|" + dQuery.value(0).toString() + "|" + dQuery.value(3).toString());
                        if (!determList.contains(determ))
                            determList.append(determ);
                    }

                    if (!tsnIDFound)
                    {
                        qDebug() << "In refreshInputFields(): tsnID doesn't exist in USDA list. Allow the user to input all necessary information";
                    }

                }

            }
            // End replacement
        }

        if (!db.commit())
        {
            qDebug() << "In refreshInputFields() numSelect > 1: Problem committing changes to database. Data may be lost.";
            db.rollback();
        }

        determList.removeDuplicates();
        if (determList.size() > 1)
            ui->numberOfDeterminations->setText("All of " + QString::number(determList.size()));
        else
            ui->numberOfDeterminations->setText(QString::number(determList.size()) + " of " + QString::number(determList.size()));

        tsnIDList.removeDuplicates();
        identifiedByList.removeDuplicates();
        dateIdentifiedList.removeDuplicates();
        nameAccordingToIDList.removeDuplicates();
        identificationRemarksList.removeDuplicates();
        cameoList.removeDuplicates();
        organismRemarksList.removeDuplicates();
        organismNameList.removeDuplicates();
        organismScopeList.removeDuplicates();
        noteList.removeDuplicates();
        organismLatLongList.removeDuplicates();
        organismAltList.removeDuplicates();
        organismGeorefRemarksList.removeDuplicates();
        collectionCodeList.removeDuplicates();
        catalogNumberList.removeDuplicates();
        establishmentMeansList.removeDuplicates();
        vernacularList.removeDuplicates();
        kingdomList.removeDuplicates();
        classNameList.removeDuplicates();
        orderList.removeDuplicates();
        familyList.removeDuplicates();
        primulaList.removeDuplicates();
        specificEpithetList.removeDuplicates();
        infraspecificEpithetList.removeDuplicates();
        taxonRankList.removeDuplicates();

        if (tsnIDList.length() > 49)
        {
            QStringList tmpList;
            for (int iTrim = 0; iTrim < 50; iTrim++)
            {
                tmpList.append(tsnIDList.at(iTrim));
            }
            tsnIDList = tmpList;
            tsnIDList.append("...and more");
        }

        if (identifiedByList.length() > 49)
        {
            QStringList tmpList;
            for (int iTrim = 0; iTrim < 50; iTrim++)
            {
                tmpList.append(identifiedByList.at(iTrim));
            }
            identifiedByList = tmpList;
            identifiedByList.append("...and more");
        }

        if (dateIdentifiedList.length() > 49)
        {
            QStringList tmpList;
            for (int iTrim = 0; iTrim < 50; iTrim++)
            {
                tmpList.append(dateIdentifiedList.at(iTrim));
            }
            dateIdentifiedList = tmpList;
            dateIdentifiedList.append("...and more");
        }

        if (nameAccordingToIDList.length() > 49)
        {
            QStringList tmpList;
            for (int iTrim = 0; iTrim < 50; iTrim++)
            {
                tmpList.append(nameAccordingToIDList.at(iTrim));
            }
            nameAccordingToIDList = tmpList;
            nameAccordingToIDList.append("...and more");
        }

        if (identificationRemarksList.length() > 49)
        {
            QStringList tmpList;
            for (int iTrim = 0; iTrim < 50; iTrim++)
            {
                tmpList.append(identificationRemarksList.at(iTrim));
            }
            identificationRemarksList = tmpList;
            identificationRemarksList.append("...and more");
        }

        if (cameoList.length() > 49)
        {
            QStringList tmpList;
            for (int iTrim = 0; iTrim < 50; iTrim++)
            {
                tmpList.append(cameoList.at(iTrim));
            }
            cameoList = tmpList;
            cameoList.append("...and more");
        }

        if (organismRemarksList.length() > 49)
        {
            QStringList tmpList;
            for (int iTrim = 0; iTrim < 50; iTrim++)
            {
                tmpList.append(organismRemarksList.at(iTrim));
            }
            organismRemarksList = tmpList;
            organismRemarksList.append("...and more");
        }

        if (organismNameList.length() > 49)
        {
            QStringList tmpList;
            for (int iTrim = 0; iTrim < 50; iTrim++)
            {
                tmpList.append(organismNameList.at(iTrim));
            }
            organismNameList = tmpList;
            organismNameList.append("...and more");
        }

        if (organismScopeList.length() > 49)
        {
            QStringList tmpList;
            for (int iTrim = 0; iTrim < 50; iTrim++)
            {
                tmpList.append(organismScopeList.at(iTrim));
            }
            organismScopeList = tmpList;
            organismScopeList.append("...and more");
        }

        if (noteList.length() > 49)
        {
            QStringList tmpList;
            for (int iTrim = 0; iTrim < 50; iTrim++)
            {
                tmpList.append(noteList.at(iTrim));
            }
            noteList = tmpList;
            noteList.append("...and more");
        }

        if (organismLatLongList.length() > 49)
        {
            QStringList tmpList;
            for (int iTrim = 0; iTrim < 50; iTrim++)
            {
                tmpList.append(organismLatLongList.at(iTrim));
            }
            organismLatLongList = tmpList;
            organismLatLongList.append("...and more");
        }

        if (organismAltList.length() > 49)
        {
            QStringList tmpList;
            for (int iTrim = 0; iTrim < 50; iTrim++)
            {
                tmpList.append(organismAltList.at(iTrim));
            }
            organismAltList = tmpList;
            organismAltList.append("...and more");
        }

        if (organismGeorefRemarksList.length() > 49)
        {
            QStringList tmpList;
            for (int iTrim = 0; iTrim < 50; iTrim++)
            {
                tmpList.append(organismGeorefRemarksList.at(iTrim));
            }
            organismGeorefRemarksList = tmpList;
            organismGeorefRemarksList.append("...and more");
        }

        if (collectionCodeList.length() > 49)
        {
            QStringList tmpList;
            for (int iTrim = 0; iTrim < 50; iTrim++)
            {
                tmpList.append(collectionCodeList.at(iTrim));
            }
            collectionCodeList = tmpList;
            collectionCodeList.append("...and more");
        }

        if (catalogNumberList.length() > 49)
        {
            QStringList tmpList;
            for (int iTrim = 0; iTrim < 50; iTrim++)
            {
                tmpList.append(catalogNumberList.at(iTrim));
            }
            catalogNumberList = tmpList;
            catalogNumberList.append("...and more");
        }

        if (establishmentMeansList.length() > 49)
        {
            QStringList tmpList;
            for (int iTrim = 0; iTrim < 50; iTrim++)
            {
                tmpList.append(establishmentMeansList.at(iTrim));
            }
            establishmentMeansList = tmpList;
            establishmentMeansList.append("...and more");
        }

        if (vernacularList.length() > 49)
        {
            QStringList tmpList;
            for (int iTrim = 0; iTrim < 50; iTrim++)
            {
                tmpList.append(vernacularList.at(iTrim));
            }
            vernacularList = tmpList;
            vernacularList.append("...and more");
        }

        if (kingdomList.length() > 49)
        {
            QStringList tmpList;
            for (int iTrim = 0; iTrim < 50; iTrim++)
            {
                tmpList.append(kingdomList.at(iTrim));
            }
            kingdomList = tmpList;
            kingdomList.append("...and more");
        }

        if (classNameList.length() > 49)
        {
            QStringList tmpList;
            for (int iTrim = 0; iTrim < 50; iTrim++)
            {
                tmpList.append(classNameList.at(iTrim));
            }
            classNameList = tmpList;
            classNameList.append("...and more");
        }

        if (orderList.length() > 49)
        {
            QStringList tmpList;
            for (int iTrim = 0; iTrim < 50; iTrim++)
            {
                tmpList.append(orderList.at(iTrim));
            }
            orderList = tmpList;
            orderList.append("...and more");
        }

        if (familyList.length() > 49)
        {
            QStringList tmpList;
            for (int iTrim = 0; iTrim < 50; iTrim++)
            {
                tmpList.append(familyList.at(iTrim));
            }
            familyList = tmpList;
            familyList.append("...and more");
        }

        if (primulaList.length() > 49)
        {
            QStringList tmpList;
            for (int iTrim = 0; iTrim < 50; iTrim++)
            {
                tmpList.append(primulaList.at(iTrim));
            }
            primulaList = tmpList;
            primulaList.append("...and more");
        }

        if (specificEpithetList.length() > 49)
        {
            QStringList tmpList;
            for (int iTrim = 0; iTrim < 50; iTrim++)
            {
                tmpList.append(specificEpithetList.at(iTrim));
            }
            specificEpithetList = tmpList;
            specificEpithetList.append("...and more");
        }

        if (infraspecificEpithetList.length() > 49)
        {
            QStringList tmpList;
            for (int iTrim = 0; iTrim < 50; iTrim++)
            {
                tmpList.append(infraspecificEpithetList.at(iTrim));
            }
            infraspecificEpithetList = tmpList;
            infraspecificEpithetList.append("...and more");
        }

        if (taxonRankList.length() > 49)
        {
            QStringList tmpList;
            for (int iTrim = 0; iTrim < 50; iTrim++)
            {
                tmpList.append(taxonRankList.at(iTrim));
            }
            taxonRankList = tmpList;
            taxonRankList.append("...and more");
        }

        ui->tsnID->setText(simplifyField(tsnIDList));
        ui->tsnID->setToolTip(tsnIDList.join("\n"));

        ui->identifiedBy->setText(simplifyField(identifiedByList));
        ui->identifiedBy->setToolTip(identifiedByList.join("\n"));

        ui->dateIdentified->setText(simplifyField(dateIdentifiedList));
        ui->dateIdentified->setToolTip(dateIdentifiedList.join("\n"));

        ui->nameAccordingToID->setCurrentText(simplifyField(nameAccordingToIDList));
        ui->nameAccordingToID->setToolTip(nameAccordingToIDList.join("\n"));

        ui->identificationRemarks->setText(simplifyField(identificationRemarksList));
        ui->identificationRemarks->setToolTip(identificationRemarksList.join("\n"));

        ui->cameoText->setText(simplifyField(cameoList));
        ui->cameoText->setToolTip(cameoList.join("\n"));

        ui->organismRemarks->setText(simplifyField(organismRemarksList));
        ui->organismRemarks->setToolTip(organismRemarksList.join("\n"));

        ui->organismName->setText(simplifyField(organismNameList));
        ui->organismName->setToolTip(organismNameList.join("\n"));

        ui->organismScope->setText(simplifyField(organismScopeList));
        ui->organismScope->setToolTip(organismScopeList.join("\n"));

        ui->htmlNote->setText(simplifyField(noteList));
        ui->htmlNote->setToolTip(noteList.join("\n"));

        ui->organismLatLong->setText(simplifyField(organismLatLongList));
        ui->organismLatLong->setToolTip(organismLatLongList.join("\n"));

        ui->organismAltitude->setText(simplifyField(organismAltList));
        ui->organismAltitude->setToolTip(organismAltList.join("\n"));

        ui->organismGeoreferenceRemarks->setCurrentText(simplifyField(organismGeorefRemarksList));
        ui->organismGeoreferenceRemarks->setToolTip(organismGeorefRemarksList.join("\n"));

        ui->collectionCode->setCurrentText(simplifyField(collectionCodeList));
        ui->collectionCode->setToolTip(collectionCodeList.join("\n"));

        ui->catalogNumber->setText(simplifyField(catalogNumberList));
        ui->catalogNumber->setToolTip(catalogNumberList.join("\n"));

        ui->establishmentMeans->setCurrentText(simplifyField(establishmentMeansList));
        ui->establishmentMeans->setToolTip(establishmentMeansList.join("\n"));

        ui->vernacular->setText(simplifyField(vernacularList));
        ui->vernacular->setToolTip(vernacularList.join("\n"));

        ui->kingdom->setText(simplifyField(kingdomList));
        ui->kingdom->setToolTip(kingdomList.join("\n"));

        ui->className->setText(simplifyField(classNameList));
        ui->className->setToolTip(classNameList.join("\n"));

        ui->order->setText(simplifyField(orderList));
        ui->order->setToolTip(orderList.join("\n"));

        ui->family->setText(simplifyField(familyList));
        ui->family->setToolTip(familyList.join("\n"));

        ui->primula->setText(simplifyField(primulaList));
        ui->primula->setToolTip(primulaList.join("\n"));

        ui->specificEpithet->setText(simplifyField(specificEpithetList));
        ui->specificEpithet->setToolTip(specificEpithetList.join("\n"));

        ui->infraspecificEpithet->setText(simplifyField(infraspecificEpithetList));
        ui->infraspecificEpithet->setToolTip(infraspecificEpithetList.join("\n"));

        ui->taxonRank->setText(simplifyField(taxonRankList));
        ui->taxonRank->setToolTip(taxonRankList.join("\n"));

        //frame 4
        pauseSavingView = true;
        // group of specimen box
        fieldList.clear();          // list of values for this field
        foreach (QListWidgetItem* img, itemList)
        {
            int i = imageIndexHash.value(img->text());
            fieldList.append(images[i].groupOfSpecimen);
        }
        fieldList.removeDuplicates();
        if (fieldList.length() > 49)
        {
            QStringList tmpList;
            for (int iTrim = 0; iTrim < 50; iTrim++)
            {
                tmpList.append(fieldList.at(iTrim));
            }
            fieldList = tmpList;
            fieldList.append("...and more");
        }
        ui->groupOfSpecimenBox->setCurrentText(simplifyField(fieldList));
        ui->groupOfSpecimenBox->setToolTip(fieldList.join("\n"));

        // part of specimen box
        fieldList.clear();          // list of values for this field
        foreach (QListWidgetItem* img, itemList)
        {
            int i = imageIndexHash.value(img->text());
            fieldList.append(images[i].portionOfSpecimen);
        }
        fieldList.removeDuplicates();
        if (fieldList.length() > 49)
        {
            QStringList tmpList;
            for (int iTrim = 0; iTrim < 50; iTrim++)
            {
                tmpList.append(fieldList.at(iTrim));
            }
            fieldList = tmpList;
            fieldList.append("...and more");
        }
        ui->partOfSpecimenBox->setCurrentText(simplifyField(fieldList));
        ui->partOfSpecimenBox->setToolTip(fieldList.join("\n"));

        // view of specimen box
        fieldList.clear();          // list of values for this field
        foreach (QListWidgetItem* img, itemList)
        {
            int i = imageIndexHash.value(img->text());
            fieldList.append(images[i].viewOfSpecimen);
        }
        fieldList.removeDuplicates();
        if (fieldList.length() > 49)
        {
            QStringList tmpList;
            for (int iTrim = 0; iTrim < 50; iTrim++)
            {
                tmpList.append(fieldList.at(iTrim));
            }
            fieldList = tmpList;
            fieldList.append("...and more");
        }
        ui->viewOfSpecimenBox->setCurrentText(simplifyField(fieldList));
        ui->viewOfSpecimenBox->setToolTip(fieldList.join("\n"));

        pauseSavingView = false;

        // image caption
        fieldList.clear();          // list of values for this field
        foreach (QListWidgetItem* img, itemList)
        {
            int i = imageIndexHash.value(img->text());
            fieldList.append(images[i].caption);
        }
        fieldList.removeDuplicates();
        if (fieldList.length() > 49)
        {
            QStringList tmpList;
            for (int iTrim = 0; iTrim < 50; iTrim++)
            {
                tmpList.append(fieldList.at(iTrim));
            }
            fieldList = tmpList;
            fieldList.append("...and more");
        }
        ui->imageCaption->setText(simplifyField(fieldList));
        ui->imageCaption->setToolTip(fieldList.join("\n"));

        // Latitude, longitude and Altitude
        fieldList.clear();       // list of values for this field
        QStringList altList { };
        foreach (QListWidgetItem* img, itemList)
        {
            int i = imageIndexHash.value(img->text());

            QSqlQuery getImageLoc;
            getImageLoc.prepare("SELECT dwc_decimalLatitude, dwc_decimalLongitude, geo_alt "
                                "FROM images WHERE dcterms_identifier = (?) LIMIT 1");
            getImageLoc.addBindValue(images[i].identifier);
            getImageLoc.exec();

            if (getImageLoc.next())
            {
                QString lat = getImageLoc.value(0).toString();
                QString lon = getImageLoc.value(1).toString();
                QString alt = getImageLoc.value(2).toString();
                QString latCommaLon = lat + ", " + lon;
                if (lat == "" || lon == "")
                {
                    latCommaLon = "";
                }
                fieldList.append(latCommaLon);
                altList.append(alt);
            }
        }
        fieldList.removeDuplicates();
        altList.removeDuplicates();

        if (fieldList.length() > 49)
        {
            QStringList tmpList;
            for (int iTrim = 0; iTrim < 50; iTrim++)
            {
                tmpList.append(fieldList.at(iTrim));
            }
            fieldList = tmpList;
            fieldList.append("...and more");
        }
        ui->imageLatAndLongBox->setText(simplifyField(fieldList));
        ui->imageLatAndLongBox->setToolTip(fieldList.join("\n"));

        if (altList.length() > 49)
        {
            QStringList tmpList;
            for (int iTrim = 0; iTrim < 50; iTrim++)
            {
                tmpList.append(altList.at(iTrim));
            }
            altList = tmpList;
            altList.append("...and more");
        }
        ui->elevation->setText(simplifyField(altList));
        ui->elevation->setToolTip(altList.join("\n"));

        // Location uncertainty
        fieldList.clear();          // list of values for this field
        foreach (QListWidgetItem* img, itemList)
        {
            int i = imageIndexHash.value(img->text());
            fieldList.append(images[i].coordinateUncertaintyInMeters);
        }
        fieldList.removeDuplicates();
        if (fieldList.length() > 49)
        {
            QStringList tmpList;
            for (int iTrim = 0; iTrim < 50; iTrim++)
            {
                tmpList.append(fieldList.at(iTrim));
            }
            fieldList = tmpList;
            fieldList.append("...and more");
        }
        ui->coordinateUncertaintyInMetersBox->setCurrentText(simplifyField(fieldList));
        ui->coordinateUncertaintyInMetersBox->setToolTip(fieldList.join("\n"));

        // County
        fieldList.clear();          // list of values for this field
        foreach (QListWidgetItem* img, itemList)
        {
            int i = imageIndexHash.value(img->text());
            fieldList.append(images[i].county);
        }
        fieldList.removeDuplicates();
        if (fieldList.length() > 49)
        {
            QStringList tmpList;
            for (int iTrim = 0; iTrim < 50; iTrim++)
            {
                tmpList.append(fieldList.at(iTrim));
            }
            fieldList = tmpList;
            fieldList.append("...and more");
        }
        ui->image_county_box->setText(simplifyField(fieldList));
        ui->image_county_box->setToolTip(fieldList.join("\n"));

        // State/Province
        fieldList.clear();          // list of values for this field
        foreach (QListWidgetItem* img, itemList)
        {
            int i = imageIndexHash.value(img->text());
            fieldList.append(images[i].stateProvince);
        }
        fieldList.removeDuplicates();
        if (fieldList.length() > 49)
        {
            QStringList tmpList;
            for (int iTrim = 0; iTrim < 50; iTrim++)
            {
                tmpList.append(fieldList.at(iTrim));
            }
            fieldList = tmpList;
            fieldList.append("...and more");
        }
        ui->image_stateProvince_box->setText(simplifyField(fieldList));
        ui->image_stateProvince_box->setToolTip(fieldList.join("\n"));

        // Country
        fieldList.clear();          // list of values for this field
        foreach (QListWidgetItem* img, itemList)
        {
            int i = imageIndexHash.value(img->text());
            fieldList.append(images[i].countryCode);
        }
        fieldList.removeDuplicates();
        if (fieldList.length() > 49)
        {
            QStringList tmpList;
            for (int iTrim = 0; iTrim < 50; iTrim++)
            {
                tmpList.append(fieldList.at(iTrim));
            }
            fieldList = tmpList;
            fieldList.append("...and more");
        }
        ui->image_countryCode_box->setText(simplifyField(fieldList));
        ui->image_countryCode_box->setToolTip(fieldList.join("\n"));

        // Continent
        fieldList.clear();          // list of values for this field
        foreach (QListWidgetItem* img, itemList)
        {
            int i = imageIndexHash.value(img->text());
            fieldList.append(images[i].continent);
        }
        fieldList.removeDuplicates();
        if (fieldList.length() > 49)
        {
            QStringList tmpList;
            for (int iTrim = 0; iTrim < 50; iTrim++)
            {
                tmpList.append(fieldList.at(iTrim));
            }
            fieldList = tmpList;
            fieldList.append("...and more");
        }
        ui->image_continent_box->setText(simplifyField(fieldList));
        ui->image_continent_box->setToolTip(fieldList.join("\n"));

        // Locality
        fieldList.clear();          // list of values for this field
        foreach (QListWidgetItem* img, itemList)
        {
            int i = imageIndexHash.value(img->text());
            fieldList.append(images[i].locality);
        }
        fieldList.removeDuplicates();
        if (fieldList.length() > 49)
        {
            QStringList tmpList;
            for (int iTrim = 0; iTrim < 50; iTrim++)
            {
                tmpList.append(fieldList.at(iTrim));
            }
            fieldList = tmpList;
            fieldList.append("...and more");
        }
        ui->locality->setText(simplifyField(fieldList));
        ui->locality->setToolTip(fieldList.join("\n"));

        // image georeference remarks
        fieldList.clear();          // list of values for this field
        foreach (QListWidgetItem* img, itemList)
        {
            int i = imageIndexHash.value(img->text());
            fieldList.append(images[i].georeferenceRemarks);
        }
        fieldList.removeDuplicates();
        if (fieldList.length() > 49)
        {
            QStringList tmpList;
            for (int iTrim = 0; iTrim < 50; iTrim++)
            {
                tmpList.append(fieldList.at(iTrim));
            }
            fieldList = tmpList;
            fieldList.append("...and more");
        }
        ui->imageGeoreferenceRemarks->setCurrentText(simplifyField(fieldList));
        ui->imageGeoreferenceRemarks->setToolTip(fieldList.join("\n"));

        // image information - reason for withholding
        fieldList.clear();          // list of values for this field
        foreach (QListWidgetItem* img, itemList)
        {
            int i = imageIndexHash.value(img->text());
            fieldList.append(images[i].informationWithheld);
        }
        fieldList.removeDuplicates();
        if (fieldList.length() > 49)
        {
            QStringList tmpList;
            for (int iTrim = 0; iTrim < 50; iTrim++)
            {
                tmpList.append(fieldList.at(iTrim));
            }
            fieldList = tmpList;
            fieldList.append("...and more");
        }
        ui->image_informationWithheld_box->setCurrentText(simplifyField(fieldList));
        ui->image_informationWithheld_box->setToolTip(fieldList.join("\n"));

        // image data generalization
        fieldList.clear();          // list of values for this field
        foreach (QListWidgetItem* img, itemList)
        {
            int i = imageIndexHash.value(img->text());
            fieldList.append(images[i].dataGeneralizations);
        }
        fieldList.removeDuplicates();
        if (fieldList.length() > 49)
        {
            QStringList tmpList;
            for (int iTrim = 0; iTrim < 50; iTrim++)
            {
                tmpList.append(fieldList.at(iTrim));
            }
            fieldList = tmpList;
            fieldList.append("...and more");
        }
        ui->image_dataGeneralization_box->setCurrentText(simplifyField(fieldList));
        ui->image_dataGeneralization_box->setToolTip(fieldList.join("\n"));

        // Occurrence remarks
        fieldList.clear();          // list of values for this field
        foreach (QListWidgetItem* img, itemList)
        {
            int i = imageIndexHash.value(img->text());
            fieldList.append(images[i].occurrenceRemarks);
        }
        fieldList.removeDuplicates();
        if (fieldList.length() > 49)
        {
            QStringList tmpList;
            for (int iTrim = 0; iTrim < 50; iTrim++)
            {
                tmpList.append(fieldList.at(iTrim));
            }
            fieldList = tmpList;
            fieldList.append("...and more");
        }
        ui->image_occurrenceRemarks_box->setText(simplifyField(fieldList));
        ui->image_occurrenceRemarks_box->setToolTip(fieldList.join("\n"));

        // frame 6 fields
        // Filename
        fieldList.clear();          // list of values for this field
        foreach (QListWidgetItem* img, itemList)
        {
            int i = imageIndexHash.value(img->text());
            fieldList.append(images[i].fileName);
        }
        fieldList.removeDuplicates();
        if (fieldList.length() > 49)
        {
            QStringList tmpList;
            for (int iTrim = 0; iTrim < 50; iTrim++)
            {
                tmpList.append(fieldList.at(iTrim));
            }
            fieldList = tmpList;
            fieldList.append("...and more");
        }
        ui->image_filename_box->setText(simplifyField(fieldList));
        ui->image_filename_box->setToolTip(fieldList.join("\n"));

        // Date
        fieldList.clear();          // list of values for this field
        foreach (QListWidgetItem* img, itemList)
        {
            int i = imageIndexHash.value(img->text());
            fieldList.append(images[i].date);
        }
        fieldList.removeDuplicates();
        if (fieldList.length() > 49)
        {
            QStringList tmpList;
            for (int iTrim = 0; iTrim < 50; iTrim++)
            {
                tmpList.append(fieldList.at(iTrim));
            }
            fieldList = tmpList;
            fieldList.append("...and more");
        }
        ui->image_date_box->setText(simplifyField(fieldList));
        ui->image_date_box->setToolTip(fieldList.join("\n"));

        // Time
        fieldList.clear();          // list of values for this field
        foreach (QListWidgetItem* img, itemList)
        {
            int i = imageIndexHash.value(img->text());
            fieldList.append(images[i].time);
        }
        fieldList.removeDuplicates();
        if (fieldList.length() > 49)
        {
            QStringList tmpList;
            for (int iTrim = 0; iTrim < 50; iTrim++)
            {
                tmpList.append(fieldList.at(iTrim));
            }
            fieldList = tmpList;
            fieldList.append("...and more");
        }
        ui->image_time_box->setText(simplifyField(fieldList));
        ui->image_time_box->setToolTip(fieldList.join("\n"));

        // Timezone
        fieldList.clear();          // list of values for this field
        foreach (QListWidgetItem* img, itemList)
        {
            int i = imageIndexHash.value(img->text());
            fieldList.append(images[i].timezone);
        }
        fieldList.removeDuplicates();
        if (fieldList.length() > 49)
        {
            QStringList tmpList;
            for (int iTrim = 0; iTrim < 50; iTrim++)
            {
                tmpList.append(fieldList.at(iTrim));
            }
            fieldList = tmpList;
            fieldList.append("...and more");
        }
        ui->image_timezone_box->setText(simplifyField(fieldList));
        ui->image_timezone_box->setToolTip(fieldList.join("\n"));

        // Width
        fieldList.clear();          // list of values for this field
        foreach (QListWidgetItem* img, itemList)
        {
            int i = imageIndexHash.value(img->text());
            fieldList.append(images[i].width);
        }
        fieldList.removeDuplicates();
        if (fieldList.length() > 49)
        {
            QStringList tmpList;
            for (int iTrim = 0; iTrim < 50; iTrim++)
            {
                tmpList.append(fieldList.at(iTrim));
            }
            fieldList = tmpList;
            fieldList.append("...and more");
        }
        ui->image_width_box->setText(simplifyField(fieldList));
        ui->image_width_box->setToolTip(fieldList.join("\n"));

        // Height
        fieldList.clear();          // list of values for this field
        foreach (QListWidgetItem* img, itemList)
        {
            int i = imageIndexHash.value(img->text());
            fieldList.append(images[i].height);
        }
        fieldList.removeDuplicates();
        if (fieldList.length() > 49)
        {
            QStringList tmpList;
            for (int iTrim = 0; iTrim < 50; iTrim++)
            {
                tmpList.append(fieldList.at(iTrim));
            }
            fieldList = tmpList;
            fieldList.append("...and more");
        }
        ui->image_height_box->setText(simplifyField(fieldList));
        ui->image_height_box->setToolTip(fieldList.join("\n"));

        // Focal length
        fieldList.clear();          // list of values for this field
        foreach (QListWidgetItem* img, itemList)
        {
            int i = imageIndexHash.value(img->text());
            fieldList.append(images[i].focalLength);
        }
        fieldList.removeDuplicates();
        if (fieldList.length() > 49)
        {
            QStringList tmpList;
            for (int iTrim = 0; iTrim < 50; iTrim++)
            {
                tmpList.append(fieldList.at(iTrim));
            }
            fieldList = tmpList;
            fieldList.append("...and more");
        }
        ui->image_focalLength_box->setText(simplifyField(fieldList));
        ui->image_focalLength_box->setToolTip(fieldList.join("\n"));

        // Photographer
        fieldList.clear();          // list of values for this field
        foreach (QListWidgetItem* img, itemList)
        {
            int i = imageIndexHash.value(img->text());
            fieldList.append(images[i].photographerCode);
        }
        fieldList.removeDuplicates();
        if (fieldList.length() > 49)
        {
            QStringList tmpList;
            for (int iTrim = 0; iTrim < 50; iTrim++)
            {
                tmpList.append(fieldList.at(iTrim));
            }
            fieldList = tmpList;
            fieldList.append("...and more");
        }
        ui->photographer->setCurrentText(simplifyField(fieldList));
        ui->photographer->setToolTip(fieldList.join("\n"));

        //lower frame 6 fields

        // Copyright year
        fieldList.clear();          // list of values for this field
        foreach (QListWidgetItem* img, itemList)
        {
            int i = imageIndexHash.value(img->text());
            fieldList.append(images[i].copyrightYear);
        }
        fieldList.removeDuplicates();
        if (fieldList.length() > 49)
        {
            QStringList tmpList;
            for (int iTrim = 0; iTrim < 50; iTrim++)
            {
                tmpList.append(fieldList.at(iTrim));
            }
            fieldList = tmpList;
            fieldList.append("...and more");
        }
        ui->copyrightYear->setText(simplifyField(fieldList));
        ui->copyrightYear->setToolTip(fieldList.join("\n"));

        // Copyright owner (agent ID, not full name)
        fieldList.clear();          // list of values for this field
        foreach (QListWidgetItem* img, itemList)
        {
            int i = imageIndexHash.value(img->text());
            fieldList.append(images[i].copyrightOwnerID);
        }
        fieldList.removeDuplicates();
        if (fieldList.length() > 49)
        {
            QStringList tmpList;
            for (int iTrim = 0; iTrim < 50; iTrim++)
            {
                tmpList.append(fieldList.at(iTrim));
            }
            fieldList = tmpList;
            fieldList.append("...and more");
        }
        ui->copyrightOwner->setCurrentText(simplifyField(fieldList));
        ui->copyrightOwner->setToolTip(fieldList.join("\n"));

        //Image copyright statement
        fieldList.clear();          // list of values for this field
        foreach (QListWidgetItem* img, itemList)
        {
            int i = imageIndexHash.value(img->text());
            fieldList.append(images[i].copyrightStatement);
        }
        fieldList.removeDuplicates();
        if (fieldList.length() > 49)
        {
            QStringList tmpList;
            for (int iTrim = 0; iTrim < 50; iTrim++)
            {
                tmpList.append(fieldList.at(iTrim));
            }
            fieldList = tmpList;
            fieldList.append("...and more");
        }
        ui->copyrightStatement->setText(simplifyField(fieldList));
        ui->copyrightStatement->setToolTip(fieldList.join("\n"));

        //Image title and description
        QStringList descList { };
        fieldList.clear();          // list of values for this field
        foreach (QListWidgetItem* img, itemList)
        {
            int i = imageIndexHash.value(img->text());
            QSqlQuery getTitle;
            getTitle.prepare("SELECT dcterms_title, dcterms_description "
                                "FROM images WHERE dcterms_identifier = (?) LIMIT 1");
            getTitle.addBindValue(images[i].identifier);
            getTitle.exec();
            QString title = "";
            QString description = "";
            if (getTitle.next())
            {
                title = getTitle.value(0).toString();
                description = getTitle.value(1).toString();
            }
            fieldList.append(title);
            descList.append(description);
        }
        fieldList.removeDuplicates();
        descList.removeDuplicates();
        if (fieldList.length() > 49)
        {
            QStringList tmpList;
            for (int iTrim = 0; iTrim < 50; iTrim++)
            {
                tmpList.append(fieldList.at(iTrim));
            }
            fieldList = tmpList;
            fieldList.append("...and more");
        }
        if (descList.length() > 49)
        {
            QStringList tmpList;
            for (int iTrim = 0; iTrim < 50; iTrim++)
            {
                tmpList.append(descList.at(iTrim));
            }
            descList = tmpList;
            descList.append("...and more");
        }
        ui->imageTitle->setText(simplifyField(fieldList));
        ui->imageTitle->setToolTip(fieldList.join("\n"));
        ui->imageDescription->setText(simplifyField(descList));
        ui->imageDescription->setToolTip(descList.join("\n"));

        //Geodedic datum
        fieldList.clear();          // list of values for this field
        foreach (QListWidgetItem* img, itemList)
        {
            int i = imageIndexHash.value(img->text());
            fieldList.append(images[i].geodeticDatum);
        }
        fieldList.removeDuplicates();
        if (fieldList.length() > 49)
        {
            QStringList tmpList;
            for (int iTrim = 0; iTrim < 50; iTrim++)
            {
                tmpList.append(fieldList.at(iTrim));
            }
            fieldList = tmpList;
            fieldList.append("...and more");
        }
        ui->geodedicDatum->setText(simplifyField(fieldList));
        ui->geodedicDatum->setToolTip(fieldList.join("\n"));

        //geonames admin
        fieldList.clear();          // list of values for this field
        foreach (QListWidgetItem* img, itemList)
        {
            int i = imageIndexHash.value(img->text());
            fieldList.append(images[i].geonamesAdmin);
        }
        fieldList.removeDuplicates();
        if (fieldList.length() > 49)
        {
            QStringList tmpList;
            for (int iTrim = 0; iTrim < 50; iTrim++)
            {
                tmpList.append(fieldList.at(iTrim));
            }
            fieldList = tmpList;
            fieldList.append("...and more");
        }
        ui->geonamesAdmin->setText(simplifyField(fieldList));
        ui->geonamesAdmin->setToolTip(fieldList.join("\n"));

        //geonames other
        fieldList.clear();          // list of values for this field
        foreach (QListWidgetItem* img, itemList)
        {
            int i = imageIndexHash.value(img->text());
            fieldList.append(images[i].geonamesOther);
        }
        fieldList.removeDuplicates();
        if (fieldList.length() > 49)
        {
            QStringList tmpList;
            for (int iTrim = 0; iTrim < 50; iTrim++)
            {
                tmpList.append(fieldList.at(iTrim));
            }
            fieldList = tmpList;
            fieldList.append("...and more");
        }
        ui->geonamesOther->setText(simplifyField(fieldList));
        ui->geonamesOther->setToolTip(fieldList.join("\n"));

        // usage terms box
        fieldList.clear();          // list of values for this field
        foreach (QListWidgetItem* img, itemList)
        {
            int i = imageIndexHash.value(img->text());
            fieldList.append(ui->usageTermsBox->itemText(images[i].usageTermsIndex.toInt()));
        }
        fieldList.removeDuplicates();
        if (fieldList.length() > 49)
        {
            QStringList tmpList;
            for (int iTrim = 0; iTrim < 50; iTrim++)
            {
                tmpList.append(fieldList.at(iTrim));
            }
            fieldList = tmpList;
            fieldList.append("...and more");
        }
        ui->usageTermsBox->setCurrentText(simplifyField(fieldList));
        ui->usageTermsBox->setToolTip(fieldList.join("\n"));

        // High Res URL
        fieldList.clear();          // list of values for this field
        foreach (QListWidgetItem* img, itemList)
        {
            int i = imageIndexHash.value(img->text());
            fieldList.append(images[i].credit);
        }
        fieldList.removeDuplicates();
        if (fieldList.length() > 49)
        {
            QStringList tmpList;
            for (int iTrim = 0; iTrim < 50; iTrim++)
            {
                tmpList.append(fieldList.at(iTrim));
            }
            fieldList = tmpList;
            fieldList.append("...and more");
        }
        ui->photoshop_Credit->setText(simplifyField(fieldList));
        ui->photoshop_Credit->setToolTip(fieldList.join("\n"));

        // High Res URL
        fieldList.clear();          // list of values for this field
        foreach (QListWidgetItem* img, itemList)
        {
            int i = imageIndexHash.value(img->text());
            fieldList.append(images[i].urlToHighRes);
        }
        fieldList.removeDuplicates();
        if (fieldList.length() > 49)
        {
            QStringList tmpList;
            for (int iTrim = 0; iTrim < 50; iTrim++)
            {
                tmpList.append(fieldList.at(iTrim));
            }
            fieldList = tmpList;
            fieldList.append("...and more");
        }
        ui->highResURL->setText(simplifyField(fieldList));
        ui->highResURL->setToolTip(fieldList.join("\n"));
    }

    // the more interesting locality information is at the front of the text string
    ui->locality->setCursorPosition(0);
}

QString DataEntry::simplifyField(QStringList &fieldList)
{
    int fc = fieldList.count();
    QString field;
    if (fc == 0)
        field = "";
    else if (fc == 1)
        field = fieldList.at(0);
    else if (fc > 1)
        field = "<multiple>";
    return field;
}

void DataEntry::clearInputFields()
{
    ui->tsnIDNotFound->setVisible(false);
    ui->setOrganismIDFirst->setVisible(false);

    for (QLineEdit *lineEditPtr : lineEditNames)
        lineEditPtr->setText("");

    ui->image_informationWithheld_box->setCurrentText("");
    ui->image_dataGeneralization_box->setCurrentText("");
    ui->identifiedBy->setText("");
    ui->nameAccordingToID->setCurrentText("");
    ui->collectionCode->setCurrentText("");
    ui->establishmentMeans->setCurrentText("");
    ui->coordinateUncertaintyInMetersBox->setCurrentText("");
    ui->usageTermsBox->setCurrentText("");
    ui->copyrightOwner->setCurrentText("");

    ui->organismGeoreferenceRemarks->setToolTip("");
    ui->identifiedBy->setToolTip("");
    ui->dateIdentified->setToolTip("");
    ui->nameAccordingToID->setToolTip("");
    ui->establishmentMeans->setToolTip("");
    ui->copyrightOwner->setToolTip("");
    ui->usageTermsBox->setToolTip("");

    currentDeterminations.clear();
    currentDetermination = 0;
    ui->numberOfDeterminations->setText("0 of 0");
}

void DataEntry::clearToolTips()
{
    for (QLineEdit *lineEditPtr : lineEditNames)
        lineEditPtr->setToolTip("");
}

void DataEntry::on_reverseGeocodeButton_clicked()
{
    // 0. gray out "Reverse Geocode" button
    // 1. get a list of all selected images
    // 2. for each image if lat/lon exists add image+lat/lon to new list<map>
    // 3. send this list to a new queue function
    // 4. in new function, go through queue and startRequest()
    // 5. refresh selected county/state/country/etc when completely done

    QList<QListWidgetItem*> itemList = ui->thumbWidget->selectedItems();
    revgeoURLList.clear();
    revgeoHash.clear();

    if (itemList.count() == 0)
    {
        QMessageBox msgBox;
        msgBox.setText("Please select at least one image first.");
        msgBox.exec();
        return;
    }

    QSqlDatabase db = QSqlDatabase::database();
    db.transaction();

    bool geocodeCacheUsed = false;
    for (QListWidgetItem *img : itemList)
    {
        int h = imageIndexHash.value(img->text());
        if (h == -1)
        {
            qDebug() << "Error: hashIndex not found when generating new organism ID.";
            continue;
        }

        QString lat = "";
        QString lon = "";

        // query the images table for the latitude and longitude
        QSqlQuery coordinateQuery;
        coordinateQuery.prepare("SELECT dwc_decimalLatitude, dwc_decimalLongitude FROM images WHERE dcterms_identifier = (?) LIMIT 1");
        coordinateQuery.addBindValue(images[h].identifier);
        coordinateQuery.exec();

        if (coordinateQuery.next())
        {
            lat = coordinateQuery.value(0).toString();
            lon = coordinateQuery.value(1).toString();
        }

        if (lat == "" || lon == "")
            continue;

        bool externalSearch = true;

        if (!ui->bypassCache->isChecked())
        {
            // query the geocode_cache to see if we have the values for these coordinates
            QSqlQuery checkCacheQuery;
            checkCacheQuery.prepare("SELECT expires, continent, country, stateProvince, county, locality, geonamesAdmin "
                                    "FROM geocode_cache WHERE latitude = (?) AND longitude = (?) LIMIT 1");
            checkCacheQuery.addBindValue(lat);
            checkCacheQuery.addBindValue(lon);
            checkCacheQuery.exec();


            // if coordinates found and their values have not expired yet, set using those values
            if (checkCacheQuery.next())
            {
                if (checkCacheQuery.value(0).toString() > QDate::currentDate().toString("yyyy-MM-dd"))
                {
                    qDebug() << "Using geocode_cache for lat=" + lat + " lon=" + lon;
                    externalSearch = false;
                    geocodeCacheUsed = true;
                    // set images[h] values and set images table values
                    images[h].continent = checkCacheQuery.value(1).toString();
                    images[h].countryCode = checkCacheQuery.value(2).toString();
                    images[h].stateProvince = checkCacheQuery.value(3).toString();
                    images[h].county = checkCacheQuery.value(4).toString();
                    images[h].locality = checkCacheQuery.value(5).toString();
                    images[h].geonamesAdmin = checkCacheQuery.value(6).toString();
                    images[h].lastModified = modifiedNow();

                    QSqlQuery updateFromCache;
                    updateFromCache.prepare("UPDATE images SET dwc_locality = (?), dwc_countryCode = (?), "
                                        "dwc_stateProvince = (?), dwc_county = (?), dwc_continent = (?), "
                                        "geonamesAdmin = (?), dcterms_modified = (?) WHERE dcterms_identifier = (?)");
                    updateFromCache.addBindValue(images[h].locality);
                    updateFromCache.addBindValue(images[h].countryCode);
                    updateFromCache.addBindValue(images[h].stateProvince);
                    updateFromCache.addBindValue(images[h].county);
                    updateFromCache.addBindValue(images[h].continent);
                    updateFromCache.addBindValue(images[h].geonamesAdmin);
                    updateFromCache.addBindValue(images[h].lastModified);
                    updateFromCache.addBindValue(images[h].identifier);
                    updateFromCache.exec();
                }
            }
        }
        // otherwise we need to use a reverse geocoding webservice
        if (externalSearch)
        {
            //QString mapquest = "http://open.mapquestapi.com/nominatim/v1/reverse.php?format=json&lat=" + lat + "&lon=" + lon;
            QString nominatim = baseReverseGeocodeURL + "format=json&lat=" + lat + "&lon=" + lon;
            revgeoHash.insertMulti(nominatim,h);
        }
    }

    if (!db.commit())
    {
        qDebug() << "In on_reverseGeocodeButton_clicked(): Problem querying database.";
        db.rollback();
    }

    revgeoURLList = revgeoHash.uniqueKeys();

    if (!revgeoURLList.isEmpty())
    {
        ui->reverseGeocodeButton->setEnabled(false);
        reverseGeocodeQueue();
    }

    if (geocodeCacheUsed)
        refreshInputFields();
}

void DataEntry::reverseGeocodeQueue()
{
    if (revgeoURLList.isEmpty())
    {
        ui->reverseGeocodeButton->setEnabled(true);
        refreshInputFields();
        return;
    }

    QUrl revgeoURL = QUrl(revgeoURLList.first());
    revgeoURLList.removeFirst();

    qDebug() << "Making a request to: " + revgeoURL.toString();
    startRequest(revgeoURL);
}

void DataEntry::startRequest(QUrl url)
{
    reply = qnam.get(QNetworkRequest(url));
    connect( reply, SIGNAL(finished()),
             this, SLOT(httpFinished()) );
}

void DataEntry::httpFinished()
{
    QVariant redirectionTarget = reply->attribute(QNetworkRequest::RedirectionTargetAttribute);
    if (reply->error())
    {
        qDebug() << "Reverse geocoding failed: " + reply->errorString();
    }
    else if (!redirectionTarget.isNull()) {
        QUrl newUrl = url.resolved(redirectionTarget.toUrl());
        qDebug() << "Redirecting reverse geocoding request to: " + newUrl.toString();

        url = newUrl;
        reply->deleteLater();
        startRequest(url);
        return;
    }
    else
    {
        QJsonDocument geolocJSON = QJsonDocument::fromJson(reply->readAll());
        if (geolocJSON.isNull()) qDebug() << "Error: JSON document is NULL.";
        if (geolocJSON.isEmpty()) qDebug() << "Error: JSON document is empty.";

        QJsonObject geolocObject = geolocJSON.object();
        QJsonObject multipartAddress = geolocObject.value("address").toObject();

        QString county = multipartAddress.value("county").toString();
        QString stateProvince = multipartAddress.value("state").toString();
        QString countryCode = multipartAddress.value("country_code").toString().toUpper();

        // set geonamesAdmin based on the country, state and county
        // only U.S. geonamesAdmin have been implemented so far
        QString geonameID;
        bool setGeonamesAdmin = false;
        if (countryCode == "US" && !stateProvince.isEmpty() && !county.isEmpty())
        {
            QString twoLetterState = stateTwoLetter.value(stateProvince);
            QString countyState = county + ", " + twoLetterState;

            // check the hash for countyState, returning the 2-letter
            geonameID = countyGeonameID.value(countyState);
            setGeonamesAdmin = true;
        }
        QString continent;
        // make this more robust
        if (multipartAddress.value("country_code") == "us")
        {
            continent = "NA";
        }
        multipartAddress.remove("county");
        multipartAddress.remove("state");
        multipartAddress.remove("country_code");
        multipartAddress.remove("postcode");
        multipartAddress.remove("city");
        multipartAddress.remove("country");
        QStringList localityList;
        for (auto jIt : multipartAddress)
        {
            localityList << jIt.toString();
        }
        //images[h].locality = geolocObject.value("display_name").toString();
        QString locality = localityList.join(", ");
        QString now = modifiedNow();

        QSqlDatabase db = QSqlDatabase::database();
        db.transaction();

        QString lat;
        QString lon;
        QString upContinent;
        QString upGeonamesAdmin;

        QString requestString = reply->request().url().toString();
        QList<int> hValues = revgeoHash.values(requestString);
        for (int h : hValues)
        {
            images[h].county = county;
            images[h].stateProvince = stateProvince;
            images[h].countryCode = countryCode;
            if (setGeonamesAdmin)
                images[h].geonamesAdmin = geonameID;
            if (!continent.isEmpty())
            {
                images[h].continent = continent;
            }

            images[h].locality = locality;
            images[h].lastModified = now;

            QSqlQuery insertQuery;
            insertQuery.prepare("UPDATE images SET dwc_locality = (?), dwc_countryCode = (?), "
                                "dwc_stateProvince = (?), dwc_county = (?), dwc_continent = (?), "
                                "geonamesAdmin = (?), dcterms_modified = (?) WHERE dcterms_identifier = (?)");
            insertQuery.addBindValue(locality);
            insertQuery.addBindValue(countryCode);
            insertQuery.addBindValue(stateProvince);
            insertQuery.addBindValue(county);
            insertQuery.addBindValue(images[h].continent);
            insertQuery.addBindValue(images[h].geonamesAdmin);
            insertQuery.addBindValue(now);
            insertQuery.addBindValue(images[h].identifier);
            insertQuery.exec();

            lat = images[h].decimalLatitude;
            lon = images[h].decimalLongitude;
            upContinent = images[h].continent;
            upGeonamesAdmin = images[h].geonamesAdmin;
        }

        QSqlQuery cacheQuery;
        cacheQuery.prepare("INSERT OR REPLACE INTO geocode_cache (latitude, longitude, "
                           "expires, continent, country, stateProvince, county, locality, "
                           "geonamesAdmin) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)");
        cacheQuery.addBindValue(lat);
        cacheQuery.addBindValue(lon);
        cacheQuery.addBindValue(QDate::currentDate().addYears(1).toString("yyyy-MM-dd"));
        cacheQuery.addBindValue(upContinent);
        cacheQuery.addBindValue(countryCode);
        cacheQuery.addBindValue(stateProvince);
        cacheQuery.addBindValue(county);
        cacheQuery.addBindValue(locality);
        cacheQuery.addBindValue(upGeonamesAdmin);
        cacheQuery.exec();

        if (!db.commit())
        {
            qDebug() << "In httpFinished(): Problem updating database.";
            db.rollback();
        }
    }

    reply->deleteLater();
    reply = 0;

    reverseGeocodeQueue();
}

void DataEntry::on_actionQuit_triggered()
{
    QApplication::quit();
}

void DataEntry::on_actionSave_changes_triggered()
{
    QString where = " where dcterms_modified > '" + dbLastPublished + "'";

    ExportCSV exportCSV;
    exportCSV.saveData(where,"");
}

void DataEntry::on_thumbWidget_itemSelectionChanged()
{
    if (pauseRefreshing)
        return;
    clearToolTips();
    ui->tsnIDNotFound->setVisible(false);
    ui->setOrganismIDFirst->setVisible(false);

    if (ui->thumbWidget->selectedItems().count() == 0)
        clearInputFields();
    else
        refreshInputFields();

    refreshImageLabel();
}

void DataEntry::on_generateNewOrganismIDButton_clicked()
{
    ui->setOrganismIDFirst->setVisible(false);
    QList<QListWidgetItem*> itemList = ui->thumbWidget->selectedItems();

    if (itemList.count() == 0)
    {
        QMessageBox msgBox;
        msgBox.setText("Please select at least one image first.");
        msgBox.exec();
        return;
    }

    int h = imageIndexHash.value(itemList.at(0)->text());

    if (itemList.length() > 1)
    {
        h = imageIndexHash.value(itemList.at(viewedImage)->text());
    }
    if (h == -1)
    {
        qDebug() << "Error: hashIndex not found when generating new organism ID.";
        return;
    }

    QString newOrganismID;
    QString idNamespace = schemeNamespace;
    if (idNamespace.isEmpty())
        idNamespace = "org-" + images[h].photographerCode;

    // if there's a scheme number, use it
    if (!schemeNumber.isEmpty())
    {
        newOrganismID = "http://bioimages.vanderbilt.edu/" + idNamespace + "/" + schemeText + schemeNumber;

        bool keepgoing = true;
        QSqlDatabase db = QSqlDatabase::database();
        db.transaction();
        while (keepgoing)
        {
            QSqlQuery query;
            query.prepare("SELECT 1 FROM organisms WHERE dcterms_identifier = (?) LIMIT 1");
            query.addBindValue(newOrganismID);
            query.exec();
            keepgoing = false;
            while (query.next())
            {
                keepgoing = true;
                schemeNumber = QString::number(schemeNumber.toInt() + 1);
                newOrganismID = "http://bioimages.vanderbilt.edu/" + idNamespace + "/" + schemeText + schemeNumber;
            }
        }

        // increment the schemeNumber for the next ID
        QSqlQuery qry;
        qry.prepare("INSERT OR REPLACE INTO settings (setting, value) VALUES (?, ?)");
        qry.addBindValue("org.scheme.number");
        qry.addBindValue(schemeNumber);
        qry.exec();

        if (!db.commit())
        {
            qDebug() << "In generateNewOrganismID(): Using scheme. Problem committing changes to database. Data may be lost.";
            db.rollback();
        }
    }
    // if there's no scheme number, try using the last 5 characters of the first selected image
    // even if there's no scheme number, we'll prepend the scheme text if it's there
    else
    {
        QString newEnding = images[h].identifier;
        // only keep alphanumerics, dashes and underscores from the image's identifier then take the rightmost 5
        newEnding = newEnding.remove(QRegExp("[^a-zA-Z\\d_-]")).right(5);
        // to be Utf8 safe maybe use: .remove(QRegExp(QString::fromUtf8("[-`~!@#$%^&*()_—+=|:;<>«»,.?/{}\'\"\\\[\\\]\\\\]")));
        newOrganismID = "http://bioimages.vanderbilt.edu/" + idNamespace + "/" + schemeText + newEnding;

        bool keepgoing = true;
        QSqlDatabase db = QSqlDatabase::database();
        db.transaction();
        while (keepgoing)
        {
            QSqlQuery query;
            query.prepare("SELECT 1 FROM organisms WHERE dcterms_identifier = (?) LIMIT 1");
            query.addBindValue(newOrganismID);
            query.exec();
            keepgoing = false;
            while (query.next())
            {
                keepgoing = true;
                lastOrgNum++;
                newOrganismID = "http://bioimages.vanderbilt.edu/" + idNamespace + "/" + schemeText + QString::number(lastOrgNum);
            }
        }

        QSqlQuery qry;
        qry.prepare("INSERT OR REPLACE INTO settings (setting, value) VALUES (?, ?)");
        qry.addBindValue("last.organismnumber");
        qry.addBindValue(lastOrgNum);
        qry.exec();

        if (!db.commit())
        {
            qDebug() << "In generateNewOrganismID(): Not using scheme. Problem committing changes to database. Data may be lost.";
            db.rollback();
        }
    }

    ui->organismID->setToolTip(newOrganismID);

    Organism newOrganism;
    newOrganism.identifier = newOrganismID;

    newOrganism.georeferenceRemarks = "Location calculated as average of its images' coordinates.";
    newOrganism.cameo = images[h].identifier;
    ui->cameoText->setText(images[h].identifier);
    ui->cameoText->setToolTip(images[h].identifier);

    // Add newOrganism to the list of Organisms
    QSqlQuery insert;
    insert.prepare("INSERT INTO organisms (dcterms_identifier, dwc_establishmentMeans, "
                   "dcterms_modified, dwc_organismRemarks, dwc_collectionCode, dwc_catalogNumber, "
                   "dwc_georeferenceRemarks, dwc_decimalLatitude, dwc_decimalLongitude, geo_alt, "
                   "dwc_organismName, dwc_organismScope, cameo, notes, suppress) VALUES "
                   "(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
    insert.addBindValue(newOrganismID);
    insert.addBindValue("uncertain");
    insert.addBindValue(modifiedNow());
    insert.addBindValue(newOrganism.organismRemarks);
    insert.addBindValue(newOrganism.collectionCode);
    insert.addBindValue(newOrganism.catalogNumber);
    insert.addBindValue(newOrganism.georeferenceRemarks);

    insert.addBindValue(images[h].decimalLatitude);
    insert.addBindValue(images[h].decimalLongitude);
    insert.addBindValue(images[h].altitudeInMeters);
    insert.addBindValue(newOrganism.organismName);
    insert.addBindValue("multicellular organism");
    insert.addBindValue(images[h].identifier);
    insert.addBindValue(newOrganism.notes);
    insert.addBindValue(newOrganism.suppress);

    if (!insert.exec()) {
        QMessageBox::critical(0, "", "Organism insertion failed: " + insert.lastError().text());
        return;
    }

    ui->organismID->setText(newOrganismID);

    QSqlDatabase db = QSqlDatabase::database();
    db.transaction();

    foreach (QListWidgetItem* img, itemList)
    {
        int i = imageIndexHash.value(img->text());
        if (i == -1)
            return;

        images[i].depicts = newOrganism.identifier;

        QSqlQuery insertQuery;
        insertQuery.prepare("UPDATE images SET foaf_depicts = (?), dcterms_modified = (?) WHERE dcterms_identifier = (?)");
        insertQuery.addBindValue(newOrganism.identifier);
        insertQuery.addBindValue(modifiedNow());
        insertQuery.addBindValue(images[i].identifier);
        insertQuery.exec();
    }

    if (!db.commit())
    {
        qDebug() << "In generateNewOrganismID(): Problem committing changes to database. Data may be lost.";
        db.rollback();
    }

    averageLocations(newOrganismID);
    // the UI needs to be refreshed to show the default values
    refreshInputFields();

    removeUnlinkedOrganisms();
}

void DataEntry::loadUSDANames()
{
    usdaNames.clear();
    allGenera.clear();
    allSpecies.clear();
    allCommonNames.clear();

    QSqlQuery query;
    query.setForwardOnly(true);
    query.prepare("SELECT dcterms_identifier, dwc_kingdom, dwc_class, dwc_order, dwc_family, dwc_genus, dwc_specificEpithet, dwc_infraspecificEpithet, dwc_taxonRank, dwc_scientificNameAuthorship, dwc_vernacularName FROM taxa");
    query.exec();
    while (query.next())
    {
        QString tsnID = query.value(0).toString();
        QString genus = query.value(5).toString();
        QString species = query.value(6).toString();
        QString infraspecific = query.value(7).toString();
        QString commonName = query.value(10).toString();

        if (tsnID.isEmpty())
            continue;

        // add the species and subspecies names in a genus search
        if (!species.isEmpty())
        {
            genus.append(" " + species);

            // only add the subspecies if there was a species name
            if (!infraspecific.isEmpty())
            {
                genus.append(" " + infraspecific);
            }
        }

        if (!infraspecific.isEmpty())
        {
            species.append(" " + infraspecific);
        }

        species.append(" (" + tsnID + ")");
        genus.append(" (" + tsnID + ")");
        commonName.append(" (" + tsnID + ")");

        QString scientificName = query.value(1).toString() + "|" + query.value(2).toString() + "|" + query.value(3).toString() + "|" +
                query.value(4).toString() + "|" + genus + "|" + species + "|" + infraspecific + "|" + query.value(8).toString() + "|" + commonName;

        usdaNames.insert(tsnID,scientificName);
        allGenera.append(genus);
        allSpecies.append(species);
        allCommonNames.append(commonName);
    }
}

void DataEntry::resizeEvent(QResizeEvent *)
{
    viewedImage = 0;
    QList<QListWidgetItem*> itemList = ui->thumbWidget->selectedItems();
    int numSelect = itemList.count();
    ui->nSelectedLabel->setText(QString::number(numSelect)+" selected");

    if (numSelect >= 1)
    {
        scaleFactor = 1;
        imageLabel->resize(imageLabel->pixmap()->size());
        QString fileName = imageHash.value(itemList.at(0)->text());
        if (!fileName.isEmpty()) {
            QImage image(fileName);
            if (image.isNull()) return;
            QPixmap p = QPixmap::fromImage(image);
            int w = imageLabel->width();
            int h = imageLabel->height();
            QPixmap pscaled = p.scaled(w,h, Qt::KeepAspectRatio);
            imageLabel->setPixmap(pscaled);
        }
    }

    QSqlQuery qry;
    qry.prepare("INSERT OR REPLACE INTO settings (setting, value) VALUES (?, ?)");
    qry.addBindValue("view.dataentry.location");
    qry.addBindValue(saveGeometry());
    qry.exec();
}

void DataEntry::moveEvent(QMoveEvent *)
{
    QSqlQuery qry;
    qry.prepare("INSERT OR REPLACE INTO settings (setting, value) VALUES (?, ?)");
    qry.addBindValue("view.dataentry.location");
    qry.addBindValue(saveGeometry());
    qry.exec();
}

void DataEntry::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::WindowStateChange) {
        bool isMax = false;
        if (windowState() == Qt::WindowMaximized)
            isMax = true;

        QSqlQuery qry;
        qry.prepare("INSERT OR REPLACE INTO settings (setting, value) VALUES (?, ?)");
        qry.addBindValue("view.dataentry.fullscreen");
        qry.addBindValue(isMax);
        qry.exec();
    }
}

void DataEntry::loadImageIDs()
{
    imageIDList.clear();
    QSqlQuery query;
    query.prepare("SELECT dwc_identifier from images");
    query.exec();
    while (query.next())
    {
        imageIDList.append(query.value(0).toString());    // dwc_identifier
    }
}

void DataEntry::on_actionHelp_Index_triggered()
{
    // Open Help window
    if (help_w.isNull())
    {
        help_w = new Help();
        help_w->setAttribute(Qt::WA_DeleteOnClose);
        help_w->show();
        help_w->select("Data Entry");
    }
    else
    {
        help_w->show();
        help_w->activateWindow();
        help_w->raise();
    }
}

void DataEntry::on_nextButton_clicked()
{
    QList<QListWidgetItem*> itemList = ui->thumbWidget->selectedItems();
    int numSelect = itemList.count();
    ui->nSelectedLabel->setText(QString::number(numSelect)+" selected");

    if (numSelect < 2)
    {
        return;
    }
    else if (viewedImage >= numSelect-1)
    {
        return;
    }
    else
    {
        viewedImage++;
        QString fileName = imageHash.value(itemList.at(viewedImage)->text());
        if (!fileName.isEmpty()) {
            scaleFactor = 1;
            int w = ui->scrollArea->width();
            int h = ui->scrollArea->height();

            QString fileToRead = fileName;
            QFileInfo fileInfo(fileToRead);
            // use the thumbnail if no other image can be found
            if (!fileInfo.isFile())
            {
                // limit dimensions to 300x300 to somewhat reduce pixelation due to upscaling
                if (w > 300)
                    w = 300;
                if (h > 300)
                    h = 300;

                QPixmap pscaled;
                pscaled = itemList.at(0)->icon().pixmap(100,100).scaled(w,h,Qt::KeepAspectRatio, Qt::SmoothTransformation);
                imageLabel->resize(w,h);
                imageLabel->setPixmap(pscaled);
                return;
            }

            QImageReader imageReader(fileToRead);
            imageReader.setScaledSize(QSize(w,h));
            QSize fullSize = imageReader.size();
            int wid = fullSize.width();
            int hei = fullSize.height();

            int setW;
            int setH;

            if (w == h && wid == hei)
            {
                setH = h;
                setW = w;
            }
            else if (float(wid)/hei > float(w)/h)
            {
                setH = (w*hei)/wid;
                setW = w;
            }
            else
            {
                setH = h;
                setW = (h*wid)/hei;
            }

            imageReader.setScaledSize(QSize(setW,setH));

            if (imageReader.canRead())
            {
                QImage img = imageReader.read();
                QPixmap pscaled;
                pscaled = pscaled.fromImage(img);
                imageLabel->resize(setW,setH);
                imageLabel->setPixmap(pscaled);
            }
            else
            {
                qDebug() << __LINE__ << "Could not load image from file: " + fileName;
                // use the thumbnail instead
                // limit dimensions to 300x300 to somewhat reduce pixelation due to upscaling
                if (w > 300)
                    w = 300;
                if (h > 300)
                    h = 300;

                QPixmap pscaled;
                pscaled = itemList.at(0)->icon().pixmap(100,100).scaled(w,h,Qt::KeepAspectRatio, Qt::SmoothTransformation);
                imageLabel->resize(w,h);
                imageLabel->setPixmap(pscaled);
                return;
            }
        }
    }
}

void DataEntry::on_previousButton_clicked()
{
    QList<QListWidgetItem*> itemList = ui->thumbWidget->selectedItems();
    int numSelect = itemList.count();
    ui->nSelectedLabel->setText(QString::number(numSelect)+" selected");

    if (numSelect < 2)
    {
        return;
    }
    else if (viewedImage <= 0)
    {
        viewedImage = 0;
        return;
    }
    else
    {
        viewedImage--;
        QString fileName = imageHash.value(itemList.at(viewedImage)->text());
        if (!fileName.isEmpty()) {
            scaleFactor = 1;
            int w = ui->scrollArea->width();
            int h = ui->scrollArea->height();

            QString fileToRead = fileName;
            QFileInfo fileInfo(fileToRead);
            // use the thumbnail if no other image can be found
            if (!fileInfo.isFile())
            {
                // limit dimensions to 300x300 to somewhat reduce pixelation due to upscaling
                if (w > 300)
                    w = 300;
                if (h > 300)
                    h = 300;

                QPixmap pscaled;
                pscaled = itemList.at(0)->icon().pixmap(100,100).scaled(w,h,Qt::KeepAspectRatio, Qt::SmoothTransformation);
                imageLabel->resize(w,h);
                imageLabel->setPixmap(pscaled);
                return;
            }

            QImageReader imageReader(fileToRead);
            imageReader.setScaledSize(QSize(w,h));
            QSize fullSize = imageReader.size();
            int wid = fullSize.width();
            int hei = fullSize.height();

            int setW;
            int setH;

            if (w == h && wid == hei)
            {
                setH = h;
                setW = w;
            }
            else if (float(wid)/hei > float(w)/h)
            {
                setH = (w*hei)/wid;
                setW = w;
            }
            else
            {
                setH = h;
                setW = (h*wid)/hei;
            }

            imageReader.setScaledSize(QSize(setW,setH));

            if (imageReader.canRead())
            {
                QImage img = imageReader.read();
                QPixmap pscaled;
                pscaled = pscaled.fromImage(img);
                imageLabel->resize(setW,setH);
                imageLabel->setPixmap(pscaled);
            }
            else
            {
                qDebug() << __LINE__  << "Could not load image from file: " + fileName;
                return;
            }
        }
    }
}

void DataEntry::setupCompleters()
{
    allGenera.sort(Qt::CaseInsensitive);
    allCommonNames.sort(Qt::CaseInsensitive);
    sensus.sort(Qt::CaseInsensitive);

    completeSensu = new QCompleter(sensus, this);
    completeSensu->setCaseSensitivity(Qt::CaseInsensitive);
    completeSensu->popup()->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    completeSensu->popup()->setMinimumWidth(300);
    completeSensu->setFilterMode(Qt::MatchContains);

    ui->nameAccordingToID->setCompleter(completeSensu);
    ui->nameAccordingToID->addItems(sensus);
    ui->nameAccordingToID->setCurrentText("");
    ui->nameAccordingToID->view()->setMinimumWidth(500);
    ui->nameAccordingToID->view()->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    QStringList withheldList;
    withheldList << "" << "Precision of coordinates reduced to protect the organism." << "Precision of coordinates reduced to protect the habitat.";
    ui->image_informationWithheld_box->addItems(withheldList);
    ui->image_informationWithheld_box->setCurrentText("");
    ui->image_informationWithheld_box->view()->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    QStringList generalizationList;
    generalizationList << "" << "Coordinates rounded to nearest .01 degree." << "Coordinates rounded to nearest .1 degree.";
    ui->image_dataGeneralization_box->addItems(generalizationList);
    ui->image_dataGeneralization_box->setCurrentText("");
    ui->image_dataGeneralization_box->view()->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    QStringList coordinateUncertaintyList;
    coordinateUncertaintyList << "10" << "100" << "1000";
    ui->coordinateUncertaintyInMetersBox->addItems(coordinateUncertaintyList);

    QStringList imageGeorefRemarksList;
    imageGeorefRemarksList.append("Location determined from camera GPS.");
    imageGeorefRemarksList.append("Location determined from Google maps.");
    imageGeorefRemarksList.append("Location determined by an independent GPS measurement.");
    imageGeorefRemarksList.append("Location determined from GIS database.");
    imageGeorefRemarksList.append("Location determined from a map.");
    imageGeorefRemarksList.append("Location based on county center.");
    QStringList orgGeorefRemarksList = imageGeorefRemarksList;
    imageGeorefRemarksList.insert(0,"");
    imageGeorefRemarksList.append("Location inferred from organism coordinates.");
    orgGeorefRemarksList.append("Location calculated as average of its images' coordinates.");

    ui->imageGeoreferenceRemarks->addItems(imageGeorefRemarksList);
    ui->imageGeoreferenceRemarks->setCurrentText("");
    ui->organismGeoreferenceRemarks->addItems(orgGeorefRemarksList);
    ui->organismGeoreferenceRemarks->setCurrentText("");
    ui->organismGeoreferenceRemarks->view()->setMinimumWidth(325);
    ui->organismGeoreferenceRemarks->view()->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    QStringList usageTermsOptions;
    usageTermsOptions << "CC0 1.0" << "CC BY 3.0" << "CC BY-SA 3.0" << "CC BY-NC 3.0" << "CC BY-NC-SA 3.0";
    ui->usageTermsBox->addItems(usageTermsOptions);
    ui->usageTermsBox->setCurrentIndex(4);
}

void DataEntry::setTaxaFromTSN(const QString &tsnID)
{
    if (tsnID.isEmpty())
        return;

    QSqlQuery nQuery;
    nQuery.prepare("SELECT dwc_kingdom, dwc_class, dwc_order, dwc_family, dwc_genus, dwc_specificEpithet, dwc_infraspecificEpithet, dwc_taxonRank, dwc_vernacularName FROM taxa WHERE dcterms_identifier = (?) LIMIT 1");
    nQuery.addBindValue(tsnID);
    nQuery.exec();
    if (nQuery.next())
    {
        QString kingdom = nQuery.value(0).toString();
        QString className = nQuery.value(1).toString();
        QString order = nQuery.value(2).toString();
        QString family = nQuery.value(3).toString();
        QString genus = nQuery.value(4).toString();
        QString species = nQuery.value(5).toString();
        QString infraspecific = nQuery.value(6).toString();
        QString taxonRank = nQuery.value(7).toString();
        QString commonName = nQuery.value(8).toString();

        ui->tsnID->setText(tsnID);
        ui->vernacular->setText(commonName);
        ui->vernacular->setToolTip(commonName);
        ui->kingdom->setText(kingdom);
        ui->className->setText(className);
        ui->order->setText(order);
        ui->family->setText(family);
        ui->primula->setText(genus);
        ui->specificEpithet->setText(species);
        ui->infraspecificEpithet->setText(infraspecific);
        ui->taxonRank->setText(taxonRank);
    }
    else
    {
        qDebug() << "In setTaxaFromTSN(): tsnID '" + tsnID + "' doesn't exist in USDA list. Allow the user to input all necessary information";
        ui->vernacular->clear();
        ui->kingdom->clear();
        ui->className->clear();
        ui->order->clear();
        ui->family->clear();
        ui->primula->clear();
        ui->specificEpithet->clear();
        ui->infraspecificEpithet->clear();
        ui->taxonRank->clear();
        return;
    }
}

void DataEntry::on_organismID_textEdited(const QString &arg1)
{
    saveInput("organismID",arg1);
}

void DataEntry::on_tsnID_textEdited(const QString &arg1)
{
    ui->tsnIDNotFound->setVisible(false);
    ui->setOrganismIDFirst->setVisible(false);
    saveInput("tsnID",arg1);
}

void DataEntry::saveInput(const QString &inputField, const QString &inputData)
{
    QList<QListWidgetItem*> itemList = ui->thumbWidget->selectedItems();

    if (itemList.count() == 0)
        return;

    if (inputData == "<multiple>")
        return;

    QSqlDatabase db = QSqlDatabase::database();
    db.transaction();

    QSqlQuery insertQuery;
    QSqlQuery uQuery;
    QSqlQuery nQuery;
    nQuery.setForwardOnly(true);
    QSqlQuery dQuery;
    QSqlQuery oQuery;
    QSqlQuery updateQuery;

    foreach (QListWidgetItem* img, itemList)
    {
        int i = imageIndexHash.value(img->text());

        QString field = inputField;
        QString data = inputData;

        if (inputField == "organismID")
        {
            images[i].depicts = data;
            insertQuery.prepare("UPDATE images SET foaf_depicts = (?), dcterms_modified = (?) WHERE dcterms_identifier = (?)");
            insertQuery.addBindValue(data);
            insertQuery.addBindValue(modifiedNow());
            insertQuery.addBindValue(images[i].identifier);
            insertQuery.exec();
        }
        // the following rely on data stored in Determination/Organism rather than Image
        else if (inputField == "tsnID" ||
                 inputField == "identifiedBy" ||
                 inputField == "dateIdentified" ||
                 inputField == "nameAccordingToID" ||
                 inputField == "dwc_identificationRemarks")
        {
            // save determinations
            if (images[i].depicts == "")
            {
                ui->setOrganismIDFirst->setVisible(true);
                return;
            }

            if (inputField == "tsnID") {
                uQuery.prepare("UPDATE determinations SET tsnID = (?), dcterms_modified = (?) WHERE dsw_identified = (?)");
                uQuery.addBindValue(data);
                uQuery.addBindValue(modifiedNow());
                uQuery.addBindValue(images[i].depicts);
                uQuery.exec();

                nQuery.prepare("SELECT 1 FROM taxa WHERE dcterms_identifier = (?) LIMIT 1");
                nQuery.addBindValue(data);
                nQuery.exec();
                if (nQuery.next())
                {
                    refreshInputFields();
                }
                else if (data != "") // if it's not empty and not in USDA namelist
                {
                    qDebug() << "tsnID '" + data + "' not found. Look into this.";
                }
            }
            else if (inputField == "identifiedBy")
            {
                ui->identifiedBy->setToolTip(singleResult("dc_contributor","agents","dcterms_identifier",data));
            }
            else if (inputField == "nameAccordingToID")
            {
                ui->nameAccordingToID->setToolTip(singleResult("dc_contributor","agents","dcterms_identifier",data));
            }
            else if (inputField == "dwc_identificationRemarks")
            {
                ui->identificationRemarks->setToolTip(data);
            }

            if (!ui->tsnID->text().isEmpty() && !ui->identifiedBy->text().isEmpty())
            {
                dQuery.prepare("UPDATE determinations SET " + field + " = (?), dcterms_modified = (?) WHERE dsw_identified = (?) AND identifiedBy = (?) AND tsnID = (?)");
                dQuery.addBindValue(data);
                dQuery.addBindValue(modifiedNow());
                dQuery.addBindValue(images[i].depicts);
                dQuery.addBindValue(ui->identifiedBy->text());
                dQuery.addBindValue(ui->tsnID->text());
                dQuery.exec();
            }
        }
        else if (inputField == "cameo" ||
                 inputField == "dwc_organismRemarks" ||
                 inputField == "dwc_organismName" ||
                 inputField == "dwc_organismScope" ||
                 inputField == "organismGeorefRemarks" ||
                 inputField == "notes" ||
                 inputField == "dwc_collectionCode" ||
                 inputField == "dwc_catalogNumber" ||
                 inputField == "dwc_establishmentMeans")
        {
            //we can't save organism.tsnID if the image isn't depicting an organism yet
            if (images[i].depicts == "")
            {
                ui->setOrganismIDFirst->setVisible(true);
                return;
            }
            QString column;

            if (inputField == "cameo")
            {
                column = "cameo";
            }
            else if (inputField == "dwc_organismRemarks")
            {
                column = "dwc_organismRemarks";
                ui->organismRemarks->setToolTip(data);
            }
            else if (inputField == "dwc_organismName")
            {
                column = "dwc_organismName";
            }
            else if (inputField == "dwc_organismScope")
            {
                column = "dwc_organismScope";
            }
            else if (inputField == "organismGeorefRemarks")
            {
                column = "dwc_georeferenceRemarks";
                ui->organismGeoreferenceRemarks->setToolTip(data);
            }
            else if (inputField == "notes")
            {
                column = "notes";
                ui->htmlNote->setToolTip(data);
            }
            else if (inputField == "dwc_collectionCode")
            {
                column = "dwc_collectionCode";
            }
            else if (inputField == "dwc_catalogNumber")
            {
                column = "dwc_catalogNumber";
            }
            else if (inputField == "dwc_establishmentMeans")
            {
                column = "dwc_establishmentMeans";
            }
            oQuery.prepare("UPDATE organisms SET " + column + " = (?), dcterms_modified = (?) WHERE dcterms_identifier = (?)");
            oQuery.addBindValue(data);
            oQuery.addBindValue(modifiedNow());
            oQuery.addBindValue(images[i].depicts);
            oQuery.exec();
        }
        else if (inputField == "specimenGroup" || inputField == "specimenPart" || inputField == "specimenView")
        {
            if (inputField == "specimenGroup")
                images[i].groupOfSpecimen = data;
            else if (inputField == "specimenPart")
                images[i].portionOfSpecimen = data;
            else if (inputField == "specimenView")
                images[i].viewOfSpecimen = data;

            // convert these values into #010101 format and save to database
            QString view = viewToNumbers(images[i].groupOfSpecimen, images[i].portionOfSpecimen, images[i].viewOfSpecimen);
            images[i].imageView = view;

            updateQuery.prepare("UPDATE images SET view = (?), dcterms_modified = (?) WHERE dcterms_identifier = (?)");
            updateQuery.addBindValue(view);
            updateQuery.addBindValue(modifiedNow());
            updateQuery.addBindValue(images[i].identifier);
            updateQuery.exec();
        }
        else if (inputField == "organismCoordinates")
        {
            QStringList latlonSplit = data.split(",");
            if (latlonSplit.size() != 2)
                return;
            QSqlQuery findGeoRem;
            findGeoRem.prepare("SELECT dwc_georeferenceRemarks FROM organisms WHERE dcterms_identifier = (?) LIMIT 1");
            findGeoRem.addBindValue(images[i].depicts);
            findGeoRem.exec();
            if (findGeoRem.next())
            {
                // no need to save coordinates if they're calculated from the average; just recalculate the average
                if (findGeoRem.value(0).toString() == "Location calculated as average of its images' coordinates.")
                {
                    averageLocations(images[i].depicts);
                    continue;
                }
            }

            QSqlQuery upImLocQuery;
            upImLocQuery.prepare("UPDATE images SET dwc_decimalLatitude = (?), dwc_decimalLongitude = (?), "
                                "dcterms_modified = (?) WHERE foaf_depicts = (?) and dwc_georeferenceRemarks = (?)");
            upImLocQuery.addBindValue(latlonSplit.at(0).trimmed());
            upImLocQuery.addBindValue(latlonSplit.at(1).trimmed());
            upImLocQuery.addBindValue(modifiedNow());
            upImLocQuery.addBindValue(images[i].depicts);
            upImLocQuery.addBindValue("Location inferred from organism coordinates.");
            upImLocQuery.exec();

            // save organism latitude except when it is derived from its images
            QSqlQuery upOrgLocQuery;
            upOrgLocQuery.prepare("UPDATE organisms SET dwc_decimalLatitude = (?), dwc_decimalLongitude = (?), "
                                  "dcterms_modified = (?) WHERE dcterms_identifier = (?) and dwc_georeferenceRemarks != (?)");
            upOrgLocQuery.addBindValue(latlonSplit.at(0).trimmed());
            upOrgLocQuery.addBindValue(latlonSplit.at(1).trimmed());
            upOrgLocQuery.addBindValue(modifiedNow());
            upOrgLocQuery.addBindValue(images[i].depicts);
            upOrgLocQuery.addBindValue("Location calculated as average of its images' coordinates.");
            upOrgLocQuery.exec();
        }
        else if (inputField == "imageCoordinates")
        {
            QStringList latlonSplit = data.split(",");
            QString lat = "";
            QString lon = "";
            if (!data.isEmpty() && latlonSplit.size() != 2)
                return;
            else if (!data.isEmpty())
            {
                lat = latlonSplit.at(0).trimmed();
                lon = latlonSplit.at(1).trimmed();
                if (lat.isEmpty() || lon.isEmpty())
                    return;
            }
            images[i].decimalLatitude = lat;
            images[i].decimalLongitude = lon;
            images[i].county = "";
            images[i].stateProvince = "";
            images[i].countryCode = "";
            images[i].continent = "";
            images[i].locality = "";

            QSqlQuery upImLocQuery;
            upImLocQuery.prepare("UPDATE images SET dwc_decimalLatitude = (?), dwc_decimalLongitude = (?), "
                                 "dwc_county = (?), dwc_stateProvince = (?), dwc_countryCode = (?), "
                                 "dwc_continent = (?), dwc_locality = (?), dcterms_modified = (?) "
                                 "WHERE dcterms_identifier = (?) and dwc_georeferenceRemarks != (?)");
            upImLocQuery.addBindValue(lat);
            upImLocQuery.addBindValue(lon);
            upImLocQuery.addBindValue("");
            upImLocQuery.addBindValue("");
            upImLocQuery.addBindValue("");
            upImLocQuery.addBindValue("");
            upImLocQuery.addBindValue("");
            upImLocQuery.addBindValue(modifiedNow());
            upImLocQuery.addBindValue(images[i].identifier);
            upImLocQuery.addBindValue("Location inferred from organism coordinates.");
            upImLocQuery.exec();
        }
        else if (inputField == "dwc_county")
        {
            images[i].county = data;

            QString newGeonamesAdmin = "";
            if (images[i].countryCode == "US" && stateTwoLetter.contains(images[i].stateProvince))
            {
                QString twoLetterState = stateTwoLetter.value(images[i].stateProvince);
                QString countyState = images[i].county + ", " + twoLetterState;

                // check the hash for countyState, returning the 2-letter
                if (countyGeonameID.contains(countyState))
                {
                    newGeonamesAdmin = countyGeonameID.value(countyState);
                }
            }

            images[i].geonamesAdmin = newGeonamesAdmin;

            QSqlQuery updateCounty;
            updateCounty.prepare("UPDATE images SET dwc_county = (?), geonamesAdmin = (?), dcterms_modified = (?) WHERE dcterms_identifier = (?)");
            updateCounty.addBindValue(data);
            updateCounty.addBindValue(newGeonamesAdmin);
            updateCounty.addBindValue(modifiedNow());
            updateCounty.addBindValue(images[i].identifier);
            updateCounty.exec();
        }
        else if (inputField == "dcterms_dateCopyrighted")
        {
            images[i].copyrightYear = data;

            QString rights = "";
            if (!data.isEmpty() && !images[i].copyrightOwnerName.isEmpty())
            {
                rights = "(c) " + data + " " + images[i].copyrightOwnerName;
                images[i].copyrightStatement = rights;
            }

            QSqlQuery updateCopyrightYear;
            updateCopyrightYear.prepare("UPDATE images SET dcterms_dateCopyrighted = (?), dc_rights = (?), dcterms_modified = (?) WHERE dcterms_identifier = (?)");
            updateCopyrightYear.addBindValue(data);
            updateCopyrightYear.addBindValue(rights);
            updateCopyrightYear.addBindValue(modifiedNow());
            updateCopyrightYear.addBindValue(images[i].identifier);
            updateCopyrightYear.exec();
        }
        else
        {
            // we have handled saving organism data, now save Image data that isn't in the image table
            if (inputField == "imageDate")
            {
                images[i].date = inputData;
                field = "dcterms_created";
                if (!images[i].date.isEmpty() && !images[i].time.isEmpty())
                {
                    data = images[i].date + "T" + images[i].time + images[i].timezone;
                }
            }
            else if (inputField == "imageTime")
            {
                images[i].time = inputData;
                field = "dcterms_created";
                if (images[i].date.isEmpty())
                {
                    qDebug() << "Odd, images[i].date is empty.";
                    data = "";
                }
                else if (images[i].time.isEmpty())
                {
                    data = images[i].date;
                }
                else
                {
                    data = images[i].date + "T" + images[i].time + images[i].timezone;
                }
            }
            else if (inputField == "imageTimezone")
            {
                images[i].timezone = inputData;
                field = "dcterms_created";
                if (images[i].date.isEmpty())
                {
                    qDebug() << "Odd, images[i].date is empty.";
                    data = "";
                }
                else if (images[i].time.isEmpty())
                {
                    qDebug() << "images[i].time is empty. It was probably photographed a long time ago?";
                    data = images[i].date;
                }
                else
                {
                    data = images[i].date + "T" + images[i].time + images[i].timezone;
                }
            }
            // save data to the Image table
            updateQuery.prepare("UPDATE images SET " + field + " = (?), dcterms_modified = (?) WHERE dcterms_identifier = (?)");
            updateQuery.addBindValue(data);
            updateQuery.addBindValue(modifiedNow());
            updateQuery.addBindValue(images[i].identifier);
            updateQuery.exec();

            if (inputField == "dwc_geodeticDatum")
                images[i].geodeticDatum = data;
            else if (inputField == "geonamesAdmin")
                images[i].geonamesAdmin = data;
            else if (inputField == "geonamesOther")
                images[i].geonamesOther = data;
            else if (inputField == "owner")
                images[i].copyrightOwnerID = data;
            else if (inputField == "dc_rights")
                images[i].copyrightStatement = data;
            else if (inputField == "xmpRights_Owner")
                images[i].copyrightOwnerName = data;
            else if (inputField == "usageTermsIndex")
                images[i].usageTermsIndex = data;
            else if (inputField == "photoshop_Credit")
                images[i].credit = data;
            else if (inputField == "ac_hasServiceAccessPoint")
                images[i].urlToHighRes = data;
            else if (inputField == "ac_caption")
                images[i].caption = data;

            else if (inputField == "dwc_decimalLatitude")
                images[i].decimalLatitude = data;
            else if (inputField == "dwc_decimalLongitude")
                images[i].decimalLongitude = data;
            else if (inputField == "dwc_coordinateUncertaintyInMeters")
                images[i].coordinateUncertaintyInMeters = data;
            else if (inputField == "geo_alt")
                images[i].altitudeInMeters = data;
            else if (inputField == "dwc_stateProvince")
                images[i].stateProvince = data;
            else if (inputField == "dwc_countryCode")
                images[i].countryCode = data;
            else if (inputField == "dwc_continent")
                images[i].continent = data;
            else if (inputField == "dwc_locality")
                images[i].locality = data;
            else if (inputField == "dwc_georeferenceRemarks")
                images[i].georeferenceRemarks = data;
            else if (inputField == "dwc_informationWithheld")
                images[i].informationWithheld = data;
            else if (inputField == "dwc_dataGeneralizations")
                images[i].dataGeneralizations = data;
            else if (inputField == "dwc_occurrenceRemarks")
                images[i].occurrenceRemarks = data;
            else if (inputField == "focalLength")
                images[i].focalLength = data;
            else if (inputField == "photographerCode")
                images[i].photographerCode = data;
            else if (inputField == "dcterms_created")
                images[i].dcterms_created = data;
            else if (inputField == "dcterms_title")
                images[i].title = data;
            else if (inputField == "dcterms_description")
                images[i].description = data;
            else if (inputField == "geonamesOther")
                images[i].geonamesOther = data;
        }
    }

    // commit all QSqlQuery transactions
    if (!db.commit())
    {
        qDebug() << "In saveInput(): Problem committing changes to database. Data may be lost.";
        db.rollback();
    }
}

void DataEntry::on_identificationRemarks_textEdited(const QString &arg1)
{
    if (arg1 == "<multiple>")
        return;

    QList<QListWidgetItem*> itemList = ui->thumbWidget->selectedItems();

    if (itemList.isEmpty())
        return;

    int h = imageIndexHash.value(itemList.at(0)->text());
    if (h == -1)
    {
        qDebug() << "Error: hashIndex not found when setting unique organism ID.";
        return;
    }

    QString depicts = images[h].depicts;
    QSqlQuery qry;
    qry.prepare("SELECT dwc_identificationRemarks FROM determinations WHERE dsw_identified = (?) ORDER BY dwc_dateIdentified DESC LIMIT 1");
    qry.addBindValue(depicts);
    qry.exec();

    if (qry.next())
    {
        if (itemList.length() == 1 && qry.value(0).toString() == arg1)
            return;
        saveInput("dwc_identificationRemarks",arg1);
    }

}

void DataEntry::on_establishmentMeans_activated(const QString &arg1)
{
    if (arg1 == "<multiple>" || arg1 == "")
        return;
    else if (arg1 != "native" && arg1 != "introduced" && arg1 != "naturalised" && arg1 != "invasive" && arg1 != "managed" && arg1 != "uncertain")
        return;

    QList<QListWidgetItem*> itemList = ui->thumbWidget->selectedItems();

    if (itemList.isEmpty())
        return;

    int h = imageIndexHash.value(itemList.at(0)->text());
    if (h == -1)
    {
        qDebug() << "Error: hashIndex not found when setting unique organism ID.";
        return;
    }
    QString depicts = images[h].depicts;
    QSqlQuery qry;
    qry.prepare("SELECT dwc_establishmentMeans FROM organisms WHERE dcterms_identifier = (?) LIMIT 1");
    qry.addBindValue(depicts);
    qry.exec();

    if (qry.next())
    {
        if (itemList.length() == 1 && qry.value(0).toString() == arg1)
            return;
        saveInput("dwc_establishmentMeans",arg1);
    }

}

void DataEntry::on_copyrightStatement_textEdited(const QString &arg1)
{
    if (arg1 == "<multiple>")
        return;

    QList<QListWidgetItem*> itemList = ui->thumbWidget->selectedItems();

    if (itemList.isEmpty())
        return;

    int h = imageIndexHash.value(itemList.at(0)->text());
    if (h == -1)
    {
        qDebug() << "Error: hashIndex not found when setting unique organism ID.";
        return;
    }

    if (itemList.length() == 1 && images[h].copyrightStatement == arg1)
        return;

    saveInput("dc_rights",arg1);
}

void DataEntry::on_geodedicDatum_textEdited(const QString &arg1)
{
    saveInput("dwc_geodeticDatum",arg1);
}

void DataEntry::on_geonamesAdmin_textEdited(const QString &arg1)
{
    saveInput("geonamesAdmin",arg1);
}

void DataEntry::on_geonamesOther_textEdited(const QString &arg1)
{
    saveInput("geonamesOther",arg1);
}

void DataEntry::on_usageTermsBox_currentIndexChanged(int index)
{
    QList<QListWidgetItem*> itemList = ui->thumbWidget->selectedItems();

    if (itemList.isEmpty())
        return;

    int h = imageIndexHash.value(itemList.at(0)->text());
    if (h == -1)
    {
        qDebug() << "Error: hashIndex not found when setting unique organism ID.";
        return;
    }

    if (itemList.length() == 1 && images[h].usageTermsIndex == QString::number(index))
        return;

    saveInput("usageTermsIndex",QString::number(index));
}

void DataEntry::loadSensu()
{
    sensuHash.clear();
    sensus.clear();
    sensus.append("");

    QSqlQuery query("SELECT dcterms_identifier, dcterms_title FROM sensu");
    while (query.next())
    {
        sensuHash.insert(query.value(0).toString(), query.value(1).toString());
        sensus.append(query.value(1).toString() + " (" + query.value(0).toString() + ")");
    }
}

void DataEntry::on_imageCaption_textEdited(const QString &arg1)
{
    saveInput("ac_caption",arg1);
}

void DataEntry::on_imageLatAndLongBox_textEdited(const QString &arg1)
{
    QList<QListWidgetItem*> itemList = ui->thumbWidget->selectedItems();

    if (itemList.length() == 0)
        return;

    // this is a special case. parse arg1 and split into lat/lon
    QStringList latlonSplit = arg1.split(",");

    if (!arg1.isEmpty() && latlonSplit.size() != 2)
        return;
    else
    {
        saveInput("imageCoordinates",arg1);
    }

    // changing the coordinates possibly invalidates any other location information set, so clear it to be safe

    ui->image_county_box->clear();
    ui->image_stateProvince_box->clear();
    ui->image_countryCode_box->clear();
    ui->image_continent_box->clear();

    // should really only clear locality if it was auto-set by reverse geocoding
    ui->locality->clear();

    QStringList organismsToUpdate;
    // update organism locations that depend on the image locations
    for (int i = 0; i < itemList.length(); i++)
    {
        int j = imageIndexHash.value(itemList.at(i)->text());
        if (!images[j].depicts.isEmpty())
            organismsToUpdate.append(images[j].depicts);
    }
    organismsToUpdate.removeDuplicates();

    for (auto organism : organismsToUpdate)
    {
        averageLocations(organism);
    }
}

void DataEntry::on_elevation_textEdited(const QString &arg1)
{
    saveInput("geo_alt",arg1);
}

void DataEntry::on_image_county_box_textEdited(const QString &arg1)
{
    QList<QListWidgetItem*> itemList = ui->thumbWidget->selectedItems();
    if (arg1 == "<multiple>" || itemList.isEmpty())
        return;

    saveInput("dwc_county",arg1);
    refreshInputFields();
}

void DataEntry::on_image_stateProvince_box_textEdited(const QString &arg1)
{
    saveInput("dwc_stateProvince",arg1);
}

void DataEntry::on_image_countryCode_box_textEdited(const QString &arg1)
{
    saveInput("dwc_countryCode",arg1);
}

void DataEntry::on_image_continent_box_textEdited(const QString &arg1)
{
    saveInput("dwc_continent",arg1);
}

void DataEntry::on_locality_textEdited(const QString &arg1)
{
    saveInput("dwc_locality",arg1);
}

void DataEntry::on_image_occurrenceRemarks_box_textEdited(const QString &arg1)
{
    saveInput("dwc_occurrenceRemarks",arg1);
}

void DataEntry::on_image_date_box_textEdited(const QString &arg1)
{
    saveInput("imageDate",arg1);
}

void DataEntry::on_image_time_box_textEdited(const QString &arg1)
{
    saveInput("imageTime",arg1);
}

void DataEntry::on_image_focalLength_box_textEdited(const QString &arg1)
{
    saveInput("focalLength",arg1);
}

void DataEntry::refreshSpecimenGroup(const QString &arg1)
{
    QList<QListWidgetItem*> itemList = ui->thumbWidget->selectedItems();

    if (itemList.count() > 0 && specimenGroups.contains(arg1))
    {
        QStringList newParts;
        if (arg1 == "unspecified") {
            newParts << "unspecified" << "whole organism";
        }
        else if (arg1 == "woody angiosperms") {
            newParts << "unspecified" << "whole tree (or vine)" << "bark" << "twig" << "leaf" << "inflorescence" << "fruit" << "seed";
        }
        else if (arg1 == "herbaceous angiosperms") {
            newParts << "unspecified" << "whole plant" << "stem" << "leaf" << "inflorescence" << "fruit" << "seed";
        }
        else if (arg1 == "gymnosperms") {
            newParts << "unspecified" << "whole tree" << "bark" << "twig" << "leaf" << "cone" << "seed";
        }
        else if (arg1 == "ferns") {
            newParts << "unspecified" << "whole plant";
        }
        else if (arg1 == "cacti") {
            newParts << "unspecified" << "whole plant";
        }
        else if (arg1 == "mosses") {
            newParts << "unspecified" << "whole gametophyte";
        }
        else {
            qDebug() << "A strange value for groupOfSpecimenBox was set: " << arg1;
            return;
        }

        ui->partOfSpecimenBox->clear();
        ui->partOfSpecimenBox->addItems(newParts);
    }
}

void DataEntry::on_groupOfSpecimenBox_currentTextChanged(const QString &arg1)
{
    QList<QListWidgetItem*> itemList = ui->thumbWidget->selectedItems();

    if (arg1 == "<multiple>" || arg1 == "" || itemList.isEmpty())
        return;

    // save these values to the image
    refreshSpecimenGroup(arg1);

    if (!pauseSavingView)
    {
        saveInput("specimenGroup",arg1);

        QSqlDatabase db = QSqlDatabase::database();
        db.transaction();

        for (int i = 0; i < itemList.length(); i++)
        {
            int h = imageIndexHash.value(itemList.at(i)->text());
            QString depicts = images[h].depicts;
            if (depicts.isEmpty())
                continue;

            QSqlQuery qry;
            qry.prepare("SELECT tsnID FROM determinations WHERE dsw_identified = (?) ORDER BY dwc_dateIdentified DESC LIMIT 1");
            qry.addBindValue(depicts);
            qry.exec();

            if (qry.next())
            {
                QString tsnID = qry.value(0).toString();
                if (!tsnID.isEmpty())
                {
                    autosetTitle(tsnID, depicts);
                }
            }
        }
        if (!db.commit())
        {
            qDebug() << "In on_groupOfSpecimenBox...(): Problem selecting from database.";
            db.rollback();
        }
        refreshInputFields();
    }
}

bool DataEntry::refreshSpecimenPart(const QString &arg1)
{
    QList<QListWidgetItem*> itemList = ui->thumbWidget->selectedItems();

    if (itemList.count() > 0)
    {
        QStringList newViews;
        QString group = ui->groupOfSpecimenBox->currentText();

        if (arg1 == "unspecified" || group == "unspecified")
            newViews <<  "unspecified";
        else if ((group == "ferns" || group == "cacti") && arg1 == "whole plant")
            newViews << "unspecified";
        else if (group == "mosses" && arg1 == "whole gametophyte")
            newViews << "unspecified";
        else if (group == "woody angiosperms") {
            if (arg1 == "whole tree (or vine)")
                newViews <<  "unspecified" << "general" << "winter" << "view up trunk";
            else if (arg1 == "bark")
                newViews <<  "unspecified" << "of a large tree" << "of a medium tree or large branch" << "of a small tree or small branch";
            else if (arg1 == "twig")
                newViews <<  "unspecified" << "orientation of petioles" << "winter overall" << "close-up winter leaf scar/bud" << "close-up winter terminal bud";
            else if (arg1 == "leaf")
                newViews <<  "unspecified" << "whole upper surface" << "margin of upper + lower surface" << "showing orientation on twig";
            else if (arg1 == "inflorescence")
                newViews << "unspecified" << "whole - unspecified" << "whole - female" << "whole - male" << "lateral view of flower" << "frontal view of flower"
                             << "ventral view of flower + perianth" << "close-up of flower interior";
            else if (arg1 == "fruit")
                newViews <<  "unspecified" << "as borne on the plant" << "lateral or general close-up" << "section or open" << "immature";
            else if (arg1 == "seed")
                newViews <<  "unspecified" << "general view";
        }
        else if (group == "herbaceous angiosperms") {
            if (arg1 == "whole plant")
                newViews <<  "unspecified" << "juvenile" << "in flower - general view" << "in fruit";
            else if (arg1 == "stem")
                newViews <<  "unspecified" << "showing leaf bases";
            else if (arg1 == "leaf")
                newViews <<  "unspecified" << "basal or on lower stem" << "on upper stem" << "margin of upper + lower surface";
            else if (arg1 == "inflorescence")
                newViews << "unspecified" << "whole - unspecified" << "whole - female" << "whole - male" << "whole - male" << "lateral view of flower"
                         << "frontal view of flower" << "ventral view of flower + perianth" << "close-up of flower interior";
            else if (arg1 == "fruit")
                newViews <<  "unspecified" << "as borne on the plant" << "lateral or general close-up" << "section or open" << "immature";
            else if (arg1 == "seed")
                newViews <<  "unspecified" << "general view";
        }
        else if (group == "gymnosperms") {
            if (arg1 == "whole tree")
                newViews <<  "unspecified" << "general" << "view up trunk";
            else if (arg1 == "bark")
                newViews <<  "unspecified" << "of a large tree" << "of a medium tree or large branch" << "of a small tree or small branch";
            else if (arg1 == "twig")
                newViews <<  "unspecified" << "after fallen needles" << "showing attachment of needles";
            else if (arg1 == "leaf")
                newViews <<  "unspecified" << "entire needle" << "showing orientation of twig";
            else if (arg1 == "cone")
                newViews <<  "unspecified" << "male" << "female - mature open" << "female - closed" << "female - receptive" << "one year-old female";
            else if (arg1 == "seed")
                newViews <<  "unspecified" << "general view";
        }
        else {
            qDebug() << "Somehow we set a strange part of a specimen: " << arg1;
            return false;
        }

        if (!newViews.isEmpty()) {
            ui->viewOfSpecimenBox->clear();
            ui->viewOfSpecimenBox->addItems(newViews);
            ui->viewOfSpecimenBox->setCurrentText("unspecified");
            return true;
        }
    }
    return false;
}

void DataEntry::on_partOfSpecimenBox_currentTextChanged(const QString &arg1)
{
    QList<QListWidgetItem*> itemList = ui->thumbWidget->selectedItems();

    if (arg1 == "<multiple>" || arg1 == "" || itemList.isEmpty())
        return;

    // save these values to the image
    if (refreshSpecimenPart(arg1) && !pauseSavingView)
    {
        saveInput("specimenPart",arg1);

        QSqlDatabase db = QSqlDatabase::database();
        db.transaction();

        for (int i = 0; i < itemList.length(); i++)
        {
            int h = imageIndexHash.value(itemList.at(i)->text());
            QString depicts = images[h].depicts;
            if (depicts.isEmpty())
                continue;

            QSqlQuery qry;
            qry.prepare("SELECT tsnID FROM determinations WHERE dsw_identified = (?) ORDER BY dwc_dateIdentified DESC LIMIT 1");
            qry.addBindValue(depicts);
            qry.exec();

            if (qry.next())
            {
                QString tsnID = qry.value(0).toString();
                if (!tsnID.isEmpty())
                {
                    autosetTitle(tsnID, depicts);
                }
            }
        }
        if (!db.commit())
        {
            qDebug() << "In on_partOfSpecimenBox...(): Problem selecting from database.";
            db.rollback();
        }
        refreshInputFields();
    }
}

bool DataEntry::refreshSpecimenView(const QString &arg1)
{
    QList<QListWidgetItem*> itemList = ui->thumbWidget->selectedItems();

    if (itemList.count() > 0 && ui->viewOfSpecimenBox->findText(arg1) != -1)
    {
        // If there has been no change in view, do nothing
        int i = imageIndexHash.value(itemList.at(0)->text());
        if (itemList.count() == 1 && arg1 == images[i].viewOfSpecimen)
            return false;

        return true;
    }
    return false;
}

void DataEntry::on_viewOfSpecimenBox_currentTextChanged(const QString &arg1)
{
    QList<QListWidgetItem*> itemList = ui->thumbWidget->selectedItems();

    if (arg1 == "<multiple>" || arg1 == "" || itemList.isEmpty())
        return;

    // save to image
    if (refreshSpecimenView(arg1) && !pauseSavingView)
    {
        saveInput("specimenView",arg1);

        QSqlDatabase db = QSqlDatabase::database();
        db.transaction();

        for (int i = 0; i < itemList.length(); i++)
        {
            int h = imageIndexHash.value(itemList.at(i)->text());
            QString depicts = images[h].depicts;
            if (depicts.isEmpty())
                continue;

            QSqlQuery qry;
            qry.prepare("SELECT tsnID FROM determinations WHERE dsw_identified = (?) ORDER BY dwc_dateIdentified DESC LIMIT 1");
            qry.addBindValue(depicts);
            qry.exec();

            if (qry.next())
            {
                QString tsnID = qry.value(0).toString();
                if (!tsnID.isEmpty())
                {
                    autosetTitle(tsnID, depicts);
                }
            }
        }
        if (!db.commit())
        {
            qDebug() << "In on_viewOfSpecimenBox...(): Problem selecting from database.";
            db.rollback();
        }
        refreshInputFields();
    }
}

void DataEntry::on_manualIDOverride_clicked()
{
    ui->setOrganismIDFirst->setVisible(false);
    QList<QListWidgetItem*> itemList = ui->thumbWidget->selectedItems();

    if (itemList.isEmpty())
    {
        QMessageBox msgBox;
        msgBox.setText("Please select at least one image first.");
        msgBox.exec();
        return;
    }

    int h = imageIndexHash.value(itemList.at(0)->text());
    if (h == -1)
    {
        qDebug() << "Error: hashIndex not found when setting unique organism ID.";
        return;
    }

    QString idNamespace = schemeNamespace;
    if (idNamespace.isEmpty())
        idNamespace = "org-" + images[h].photographerCode;

    QString prepend = "http://bioimages.vanderbilt.edu/" + idNamespace + "/";
    QString newOrganismID = QInputDialog::getText(this, "Organism ID", "Enter a unique ID for the organism. Your input will be prepended by:\n" + prepend);

    newOrganismID = newOrganismID.trimmed();
    if (newOrganismID.isEmpty())
        return;

    if (newOrganismID.startsWith("http://") || newOrganismID.startsWith("https://"))
    {
        newOrganismID = newOrganismID.remove(" ");
    }
    else
    {
        // to be Utf8 safe, maybe use: .remove(QRegExp(QString::fromUtf8("[-`~!@#$%^&*()_—+=|:;<>«»,.?/{}\'\"\\\[\\\]\\\\]")));
        newOrganismID = newOrganismID.remove(QRegExp("[^a-zA-Z\\d_-]"));
        newOrganismID = prepend + newOrganismID;
    }

    QSqlQuery query;
    query.setForwardOnly(true);
    query.prepare("SELECT 1 FROM organisms WHERE dcterms_identifier = (?) LIMIT 1");
    query.addBindValue(newOrganismID);
    query.exec();
    if (query.next())
    {
        bool cancel = true;

        if (QMessageBox::Yes == QMessageBox(QMessageBox::Information, "Organism ID already exists",
                                            "That organism ID already exists.\nAre you sure you want to use this ID?",
                                            QMessageBox::Yes|QMessageBox::No).exec())
        {
            cancel = false;
        }

        if (cancel)
            return;
    }

    ui->organismID->setToolTip(newOrganismID);

    Organism newOrganism;
    newOrganism.identifier = newOrganismID;                     // dcterms_identifier

    newOrganism.decimalLatitude = images[h].decimalLatitude;     // dwc_decimalLatitude
    newOrganism.decimalLongitude = images[h].decimalLongitude;   // dwc_decimalLongitude
    newOrganism.altitudeInMeters = images[h].altitudeInMeters;  // geo_alt

    newOrganism.cameo = images[h].identifier;             // cameo
    ui->cameoText->setText(images[h].identifier);
    ui->cameoText->setToolTip(images[h].identifier);

    // Add new organisms to the organisms table
    QSqlDatabase db = QSqlDatabase::database();
    db.transaction();

    QSqlQuery insert;
    insert.prepare("INSERT INTO organisms (dcterms_identifier, dwc_establishmentMeans, "
                   "dcterms_modified, dwc_organismRemarks, dwc_collectionCode, dwc_catalogNumber, "
                   "dwc_georeferenceRemarks, dwc_decimalLatitude, dwc_decimalLongitude, geo_alt, "
                   "dwc_organismName, dwc_organismScope, cameo, notes, suppress) VALUES "
                   "(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
    insert.addBindValue(newOrganismID);
    insert.addBindValue("uncertain");
    insert.addBindValue(modifiedNow());
    insert.addBindValue(newOrganism.organismRemarks);
    insert.addBindValue(newOrganism.collectionCode);
    insert.addBindValue(newOrganism.catalogNumber);
    insert.addBindValue(newOrganism.georeferenceRemarks);

    insert.addBindValue(images[h].decimalLatitude);
    insert.addBindValue(images[h].decimalLongitude);
    insert.addBindValue(images[h].altitudeInMeters);
    insert.addBindValue(newOrganism.organismName);
    insert.addBindValue("multicellular organism");
    insert.addBindValue(images[h].identifier);
    insert.addBindValue(newOrganism.notes);
    insert.addBindValue(newOrganism.suppress);
    insert.exec();

    ui->organismID->setText(newOrganismID);

    foreach (QListWidgetItem* img, itemList)
    {
        int i = imageIndexHash.value(img->text());
        if (i == -1)
            return;

        images[i].depicts = newOrganism.identifier;

        QSqlQuery updateQuery;
        updateQuery.prepare("UPDATE images SET foaf_depicts = (?), dcterms_modified = (?) WHERE dcterms_identifier = (?)");
        updateQuery.addBindValue(newOrganism.identifier);
        updateQuery.addBindValue(modifiedNow());
        updateQuery.addBindValue(images[i].identifier);
        updateQuery.exec();
    }

    if (!db.commit())
    {
        qDebug() << "In manualIDOverride(): Problem committing changes to database. Data may be lost.";
        db.rollback();
    }

    removeUnlinkedOrganisms();
    refreshInputFields();
}

void DataEntry::on_nameAccordingToID_currentIndexChanged(const QString &arg1)
{
    if (arg1 == "<multiple>")
        return;

    QString nameAccordingTo = arg1.split(" ").last();
    nameAccordingTo.remove("(");
    nameAccordingTo.remove(")");
    if (nameAccordingTo.isEmpty())
        nameAccordingTo = "nominal";

    QList<QListWidgetItem*> itemList = ui->thumbWidget->selectedItems();

    if (itemList.isEmpty())
        return;

    int h = imageIndexHash.value(itemList.at(0)->text());
    if (h == -1)
    {
        qDebug() << "Error: hashIndex not found when setting nameAccordingToID";
        return;
    }
    QString depicts = images[h].depicts;
    QSqlQuery qry;
    qry.prepare("SELECT nameAccordingToID FROM determinations WHERE dsw_identified = (?) ORDER BY dwc_dateIdentified DESC LIMIT 1");
    qry.addBindValue(depicts);
    qry.exec();

    if (qry.next())
    {
        if (itemList.length() == 1 && qry.value(0).toString() == nameAccordingTo)
            return;

        saveInput("nameAccordingToID",nameAccordingTo);
    }
}

void DataEntry::on_image_informationWithheld_box_activated(const QString &arg1)
{
    QList<QListWidgetItem*> itemList = ui->thumbWidget->selectedItems();
    if (arg1 == "<multiple>" || itemList.isEmpty())
        return;

    int h = imageIndexHash.value(itemList.at(0)->text());

    if (itemList.count() == 1 && images[h].informationWithheld == arg1)
        return;

    saveInput("dwc_informationWithheld",arg1);
}

void DataEntry::on_image_dataGeneralization_box_activated(const QString &arg1)
{
    QList<QListWidgetItem*> itemList = ui->thumbWidget->selectedItems();
    if (arg1 == "<multiple>" || itemList.isEmpty())
        return;

    int h = imageIndexHash.value(itemList.at(0)->text());

    if (itemList.count() == 1 && images[h].dataGeneralizations == arg1)
        return;

    saveInput("dwc_dataGeneralizations",arg1);
}

void DataEntry::on_cameoButton_clicked()
{
    QList<QListWidgetItem*> itemList = ui->thumbWidget->selectedItems();
    QString currentImage;

    //do nothing if no images are selected
    if (itemList.length() == 0)
    {
        qDebug() << "The 'set cameo' button was clicked but no images are selected.";
        return;
    }
    else if (itemList.length() == 1)
    {
        currentImage = itemList.at(0)->text();
    }
    else if (itemList.length() > 1)
    {
        currentImage = itemList.at(viewedImage)->text();
    }

    if (!currentImage.isEmpty())
    {
        int h = imageIndexHash.value(currentImage);
        if (h == -1)
        {
            qDebug() << "Error: hashIndex not found when setting unique organism ID.";
            return;
        }
        saveInput("cameo", images[h].identifier);
        ui->cameoText->setText(images[h].identifier);
        ui->cameoText->setToolTip(images[h].identifier);
    }
}

void DataEntry::on_thumbnailFilter_activated(int index)
{
    // 0 : Display all thumbnails
    // 1 : Only thumbnails with no organism ID
    // 2 : Only thumbnails not identified (no tsnID)
    // 3 : Only thumbnails without latitude or longitude
    // 4 : Only thumbnails not reverse geocoded (no country)
    // 5 : Only thumbnails with no time zone
    // 6 : Only thumbnails with "unspecified" specimen view
    // 7 : Only thumbnails with nominal source of name
    // 8 : Only show cameos (1 image per organism)

    for (auto sim : similarImages)
    {
        ui->thumbWidget->item(sim)->setBackground(Qt::NoBrush);
    }
    similarImages.clear();
    if (index == 0)
    {
        for (int x = 0; x < ui->thumbWidget->count(); x++)
        {
            ui->thumbWidget->item(x)->setHidden(false);
        }
    }
    else if (index == 1)
    {
        for (int x = 0; x < ui->thumbWidget->count(); x++)
        {
            // first make each thumbnail visible--we don't know its state when this filter was applied
            ui->thumbWidget->item(x)->setHidden(false);

            int h = imageIndexHash.value(ui->thumbWidget->item(x)->text());
            if (h == -1)
            {
                qDebug() << "Error: hashIndex not found when checking imageIndexHash in thumbnailFilter.";
                continue;
            }
            if (images[h].depicts != "")
                ui->thumbWidget->item(x)->setHidden(true);
        }
    }
    else if (index == 2)
    {
        for (int x = 0; x < ui->thumbWidget->count(); x++)
        {
            // first make each thumbnail visible--we don't know its state when this filter was applied
            ui->thumbWidget->item(x)->setHidden(false);

            int h = imageIndexHash.value(ui->thumbWidget->item(x)->text());
            if (h == -1)
            {
                qDebug() << "Error: hashIndex not found when checking imageIndexHash in thumbnailFilter.";
                continue;
            }
            // if the image has an organismID and that organism has a determination, hide
            QString depicted = images[h].depicts;
            if (depicted != "")
            {
                QSqlQuery nQuery;
                nQuery.setForwardOnly(true);
                nQuery.prepare("SELECT 1 FROM determinations where dsw_identified = (?) LIMIT 1");
                nQuery.addBindValue(depicted);
                nQuery.exec();
                if (nQuery.next())
                {
                    ui->thumbWidget->item(x)->setHidden(true);
                }
            }
        }
    }
    else if (index == 3)
    {
        for (int x = 0; x < ui->thumbWidget->count(); x++)
        {
            // first make each thumbnail visible--we don't know its state when this filter was applied
            ui->thumbWidget->item(x)->setHidden(false);

            int h = imageIndexHash.value(ui->thumbWidget->item(x)->text());
            if (h == -1)
            {
                qDebug() << "Error: hashIndex not found when checking imageIndexHash in thumbnailFilter.";
                continue;
            }
            if (images[h].decimalLatitude != "" && images[h].decimalLatitude != "-9999" && images[h].decimalLongitude != "" && images[h].decimalLongitude != "-9999")
                ui->thumbWidget->item(x)->setHidden(true);
        }
    }
    else if (index == 4)
    {
        for (int x = 0; x < ui->thumbWidget->count(); x++)
        {
            // first make each thumbnail visible--we don't know its state when this filter was applied
            ui->thumbWidget->item(x)->setHidden(false);

            int h = imageIndexHash.value(ui->thumbWidget->item(x)->text());
            if (h == -1)
            {
                qDebug() << "Error: hashIndex not found when checking imageIndexHash in thumbnailFilter.";
                continue;
            }
            if (images[h].countryCode != "")
                ui->thumbWidget->item(x)->setHidden(true);
        }
    }
    else if (index == 5)
    {
        for (int x = 0; x < ui->thumbWidget->count(); x++)
        {
            // first make each thumbnail visible--we don't know its state when this filter was applied
            ui->thumbWidget->item(x)->setHidden(false);

            int h = imageIndexHash.value(ui->thumbWidget->item(x)->text());
            if (h == -1)
            {
                qDebug() << "Error: hashIndex not found when checking imageIndexHash in thumbnailFilter.";
                continue;
            }
            if (images[h].timezone != "")
                ui->thumbWidget->item(x)->setHidden(true);
        }
    }
    else if (index == 6)
    {
        for (int x = 0; x < ui->thumbWidget->count(); x++)
        {
            // first make each thumbnail visible--we don't know its state when this filter was applied
            ui->thumbWidget->item(x)->setHidden(false);

            int h = imageIndexHash.value(ui->thumbWidget->item(x)->text());
            if (h == -1)
            {
                qDebug() << "Error: hashIndex not found when checking imageIndexHash in thumbnailFilter.";
                continue;
            }
            if (images[h].viewOfSpecimen != "unspecified")
                ui->thumbWidget->item(x)->setHidden(true);
        }
    }
    else if (index == 7)
    {
        QSqlDatabase db = QSqlDatabase::database();
        db.transaction();
        for (int x = 0; x < ui->thumbWidget->count(); x++)
        {
            // first make each thumbnail hidden--we don't know its state when this filter was applied
            ui->thumbWidget->item(x)->setHidden(true);

            int h = imageIndexHash.value(ui->thumbWidget->item(x)->text());
            if (h == -1)
            {
                qDebug() << "Error: hashIndex not found when checking imageIndexHash in thumbnailFilter.";
                continue;
            }
            // if the image has an organismID and that organism has a genus, hide
            QString depicted = images[h].depicts;
            if (depicted != "")
            {
                QSqlQuery nQuery;
                nQuery.prepare("SELECT nameAccordingToID FROM determinations where dsw_identified = (?) ORDER BY dwc_dateIdentified DESC LIMIT 1");
                nQuery.addBindValue(depicted);
                nQuery.exec();
                if (nQuery.next())
                {
                    if (nQuery.value(0).toString() == "nominal")
                        ui->thumbWidget->item(x)->setHidden(false);
                }
            }
        }
        if (!db.commit())
        {
            qDebug() << "In thumbnailFilterActivated(): Problem committing changes to database. Data may be lost.";
            db.rollback();
        }
    }
    else if (index == 8)
    {
        for (int x = 0; x < ui->thumbWidget->count(); x++)
        {
            // first make each thumbnail hidden--we don't know its state when this filter was applied
            ui->thumbWidget->item(x)->setHidden(true);
        }

        QSqlQuery oQuery;
        oQuery.prepare("SELECT cameo FROM organisms");
        oQuery.exec();
        while (oQuery.next())
        {
            QString cameo = oQuery.value(0).toString();
            if (!cameo.isEmpty())
            {
                if (imageIDFilenameMap.contains(cameo))
                    ui->thumbWidget->item(thumbWidgetItems.indexOf(imageIDFilenameMap.value(cameo)))->setHidden(false);
            }
        }
    }
}

void DataEntry::on_schemeButton_clicked()
{
    QDialog schemeDialog(this);
    QFormLayout schemeForm(&schemeDialog);

    // Add some text above the fields
    schemeForm.addRow(new QLabel("Enter the namespace, the text you want to prepend to each organism ID, and\n" \
                                 "the next number to assign to a new ID.\n\n" \
                                 "If the namespace is left blank 'org-' plus the photographer's ID is used.\n" \
                                 "If both the 'Text to Prepend' and 'Next number' fields are blank then an ID is\n" \
                                 "automatically assign based on the last 5 characters of one of that organism's\n" \
                                 "images. If that ID is not available, an unused ID is found and used."));

    // Add the lineEdits with their respective labels
    QList<QLineEdit *> schemeFields;

    QLineEdit *schemeNamespaceInput = new QLineEdit(&schemeDialog);
    schemeNamespaceInput->setText(schemeNamespace);
    schemeForm.addRow("Namespace", schemeNamespaceInput);

    QLineEdit *schemeTextInput = new QLineEdit(&schemeDialog);
    schemeTextInput->setText(schemeText);
    schemeForm.addRow("Text to prepend", schemeTextInput);

    QLineEdit *schemeNumberInput = new QLineEdit(&schemeDialog);
    schemeNumberInput->setText(schemeNumber);
    schemeForm.addRow("Next number to use", schemeNumberInput);

    schemeFields << schemeNamespaceInput << schemeTextInput << schemeNumberInput;

    // Add some standard buttons (Cancel/Ok) at the bottom of the dialog
    QDialogButtonBox schemeButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, &schemeDialog);
    schemeForm.addRow(&schemeButtonBox);
    QObject::connect(&schemeButtonBox, SIGNAL(accepted()), &schemeDialog, SLOT(accept()));
    QObject::connect(&schemeButtonBox, SIGNAL(rejected()), &schemeDialog, SLOT(reject()));

    // Show the dialog as modal
    if (schemeDialog.exec() == QDialog::Accepted) {
        // If the user didn't dismiss the dialog, do something with the fields
        QString namespaceText = schemeFields.at(0)->text();
        QString prependText = schemeFields.at(1)->text();
        QString nextNumber = schemeFields.at(2)->text();

        namespaceText.remove(QRegExp("[^a-zA-Z\\d_-]"));
        prependText.remove(QRegExp("[^a-zA-Z\\d_-]"));
        nextNumber.remove(QRegExp("[^[0-9]"));

        schemeNamespace = namespaceText;
        schemeText = prependText;
        schemeNumber = nextNumber;

        QSqlDatabase db = QSqlDatabase::database();
        db.transaction();

        QSqlQuery qry;
        qry.prepare("INSERT OR REPLACE INTO settings (setting, value) VALUES (?, ?)");
        qry.addBindValue("org.scheme.namespace");
        qry.addBindValue(schemeNamespace);
        qry.exec();

        qry.prepare("INSERT OR REPLACE INTO settings (setting, value) VALUES (?, ?)");
        qry.addBindValue("org.scheme.text");
        qry.addBindValue(schemeText);
        qry.exec();

        qry.prepare("INSERT OR REPLACE INTO settings (setting, value) VALUES (?, ?)");
        qry.addBindValue("org.scheme.number");
        qry.addBindValue(schemeNumber);
        qry.exec();

        if (!db.commit())
        {
            qDebug() << "Problem committing changes to database when setting up new organism ID scheme";
            db.rollback();
        }
    }
}

void DataEntry::on_copyrightYear_textEdited(const QString &arg1)
{
    QList<QListWidgetItem*> itemList = ui->thumbWidget->selectedItems();

    if (itemList.length() == 0)
    {
        return;
    }

    saveInput("dcterms_dateCopyrighted",arg1);
    refreshInputFields();
}

void DataEntry::on_copyrightOwner_activated(const QString &arg1)
{
    if (arg1 == "<multiple>" || arg1 == "")
        return;

    QList<QListWidgetItem*> itemList = ui->thumbWidget->selectedItems();

    if (itemList.isEmpty())
        return;

    int i = imageIndexHash.value(itemList.at(0)->text());
    if (itemList.length() == 1 && images[i].copyrightOwnerID == arg1)
    {
        return;
    }
    else if (agentHash.contains(arg1))
    {
        ui->copyrightOwner->setToolTip(singleResult("dc_contributor","agents","dcterms_identifier",arg1));
        saveInput("owner",arg1);
        if (itemList.length() > 1 || images[i].copyrightOwnerName != agentHash.value(arg1))
            saveInput("xmpRights_Owner",agentHash.value(arg1));

        // for each selected image, find its copyrightYear and save img.copyrightYear + arg1 as its copyrightStatement
        // then update the UI so the new statement shows
        for (auto item : itemList)
        {
            int h = imageIndexHash.value(item->text());
            if (h == -1)
            {
                qDebug() << "Error: hashIndex not found when setting unique organism ID.";
                return;
            }

            if (!images[h].copyrightYear.isEmpty() && !images[h].copyrightOwnerName.isEmpty())
            {
                QString dc_rights = "(c) " + images[h].copyrightYear + " " + images[h].copyrightOwnerName;
                if (images[h].copyrightStatement == dc_rights)
                    continue;
                saveInput("dc_rights",dc_rights);
            }
            else
            {
                if (images[h].copyrightStatement == "")
                    continue;
                saveInput("dc_rights","");
            }
        }
        refreshInputFields();
    }
}

void DataEntry::on_collectionCode_activated(const QString &arg1)
{
    QList<QListWidgetItem*> itemList = ui->thumbWidget->selectedItems();

    if (arg1 == "<multiple>" || itemList.isEmpty())
        return;

    int h = imageIndexHash.value(itemList.at(0)->text());
    if (h == -1)
    {
        qDebug() << "Error: hashIndex not found when setting collection code.";
        return;
    }
    QString depicts = images[h].depicts;
    QSqlQuery qry;
    qry.prepare("SELECT dwc_collectionCode FROM organisms WHERE dcterms_identifier = (?) LIMIT 1");
    qry.addBindValue(depicts);
    qry.exec();

    if (qry.next())
    {
        if (itemList.length() == 1 && qry.value(0).toString() == arg1)
            return;

        if (arg1.isEmpty())
            saveInput("dwc_collectionCode","");
        else if (agentHash.contains(arg1))
        {
            ui->collectionCode->setToolTip(singleResult("dc_contributor","agents","dcterms_identifier",arg1));
            saveInput("dwc_collectionCode",arg1);
        }
    }
}

void DataEntry::on_catalogNumber_textEdited(const QString &arg1)
{
    saveInput("dwc_catalogNumber",arg1);
}

void DataEntry::on_imageGeoreferenceRemarks_activated(const QString &arg1)
{
    QList<QListWidgetItem*> itemList = ui->thumbWidget->selectedItems();

    if (arg1 == "<multiple>" || itemList.isEmpty())
        return;

    int h = imageIndexHash.value(itemList.at(0)->text());
    if (h == -1)
    {
        qDebug() << "Error: hashIndex not found when setting unique organism ID.";
        return;
    }

    // return if nothing has changed
    if (itemList.length() == 1 && images[h].georeferenceRemarks == arg1)
        return;

    saveInput("dwc_georeferenceRemarks",arg1);

    QStringList organismsToUpdate;
    QSqlDatabase db = QSqlDatabase::database();
    db.transaction();
    foreach (QListWidgetItem* img, itemList)
    {
        QSqlQuery depictsQry;
        depictsQry.prepare("SELECT foaf_depicts FROM images WHERE dcterms_identifier = (?)");
        depictsQry.addBindValue(imageIDFilenameMap.key(img->text()));
        depictsQry.exec();
        if (depictsQry.next())
        {
            organismsToUpdate.append(depictsQry.value(0).toString());
        }
    }
    if (!db.commit())
    {
        qDebug() << "In on_imageGeoreferenceRemarks(): Problem with depictsQry.";
        db.rollback();
    }
    organismsToUpdate.removeDuplicates();

    for (auto organism : organismsToUpdate)
    {
        averageLocations(organism);
    }

    if (arg1 == "Location inferred from organism coordinates.")
    {
        for (int i = 0; i < itemList.length(); i++)
        {
            int j = imageIndexHash.value(itemList.at(i)->text());

            QSqlQuery query;
            query.prepare("SELECT dwc_decimalLatitude, dwc_decimalLongitude, geo_alt "
                          "FROM organisms WHERE dcterms_identifier = (?) LIMIT 1");
            query.addBindValue(images[j].depicts);
            query.exec();

            if (query.next())
            {
                QString indLat = query.value(0).toString();
                QString indLong = query.value(1).toString();
                QString indAlt = query.value(2).toString();

                QString oldLat = images[j].decimalLatitude;
                QString oldLong = images[j].decimalLongitude;

                images[j].decimalLatitude = indLat;
                images[j].decimalLongitude = indLong;
                images[j].altitudeInMeters = indAlt;

                QSqlQuery insertQuery;
                // if the coordinates changed we need to clear other location information
                if (oldLat != indLat || oldLong != indLong)
                {
                    ui->image_county_box->clear();
                    ui->image_stateProvince_box->clear();
                    ui->image_countryCode_box->clear();
                    ui->image_continent_box->clear();
                    ui->locality->clear();

                    insertQuery.prepare("UPDATE images SET dwc_decimalLatitude = (?), dwc_decimalLongitude = (?), "
                                        "geo_alt = (?), dcterms_modified = (?), dwc_county = (?), dwc_stateProvince = (?), "
                                        "dwc_countryCode = (?), dwc_continent = (?), dwc_locality = (?) WHERE dcterms_identifier = (?)");
                    insertQuery.addBindValue(indLat);
                    insertQuery.addBindValue(indLong);
                    insertQuery.addBindValue(indAlt);
                    insertQuery.addBindValue(modifiedNow());
                    insertQuery.addBindValue(""); // clear county
                    insertQuery.addBindValue(""); // clear state/province
                    insertQuery.addBindValue(""); // clear country
                    insertQuery.addBindValue(""); // clear continent
                    insertQuery.addBindValue(""); // clear locality
                    insertQuery.addBindValue(images[j].identifier);
                }
                else
                {
                    insertQuery.prepare("UPDATE images SET dwc_decimalLatitude = (?), dwc_decimalLongitude = (?), "
                                        "geo_alt = (?), dcterms_modified = (?) WHERE dcterms_identifier = (?)");
                    insertQuery.addBindValue(indLat);
                    insertQuery.addBindValue(indLong);
                    insertQuery.addBindValue(indAlt);
                    insertQuery.addBindValue(modifiedNow());
                    insertQuery.addBindValue(images[j].identifier);
                }
                insertQuery.exec();
            }
        }
    }
    refreshInputFields();
}

void DataEntry::on_organismGeoreferenceRemarks_activated(const QString &arg1)
{
    QList<QListWidgetItem*> itemList = ui->thumbWidget->selectedItems();
    if (arg1 == "<multiple>" || itemList.isEmpty())
        return;

    // for each foaf_depicts, see if its current georeferenceRemarks matches arg1, if so return.
    // otherwise update its georefRemarks and if applicable call averageLocations

    QStringList organismsToUpdate;
    // update organism locations that depend on the image locations
    for (int i = 0; i < itemList.length(); i++)
    {
        int j = imageIndexHash.value(itemList.at(i)->text());
        if (images[j].depicts.isEmpty())
            continue;
        organismsToUpdate.append(images[j].depicts);
    }
    organismsToUpdate.removeDuplicates();

    for (auto organism : organismsToUpdate)
    {
        QSqlQuery qry;
        qry.prepare("SELECT dwc_georeferenceRemarks FROM organisms WHERE dcterms_identifier = (?) LIMIT 1");
        qry.addBindValue(organism);
        qry.exec();

        if (qry.next())
        {
            if (qry.value(0).toString() == arg1)
                continue;
            saveInput("organismGeorefRemarks",arg1);
            if (qry.value(0).toString() == "Location calculated as average of its images' coordinates." || arg1 == "Location calculated as average of its images' coordinates.")
            {
                averageLocations(organism);
            }
        }
    }

    refreshInputFields();
}

void DataEntry::on_organismLatLong_textEdited(const QString &arg1)
{
    // this is a special case. parse &arg1 and split into lat/lon
    QString lat;
    QString lon;
    QStringList latlonSplit = arg1.split(",");
    if (arg1.isEmpty())
    {
        lat = "";
        lon = "";
    }
    else if (latlonSplit.size() > 1)
    {
        lat = latlonSplit.at(0).trimmed();
        lon = latlonSplit.at(1).trimmed();
        if (latlonSplit.size() != 2 || lat.isEmpty() || lon.isEmpty())
            return;
    }
    else
    {
        return;
    }

    QList<QListWidgetItem*> itemList = ui->thumbWidget->selectedItems();
    if (arg1 == "<multiple>" || itemList.isEmpty())
        return;

    // for each foaf_depicts, see if its current altitude matches arg1, if so continue
    // otherwise update its georefRemarks and if applicable call averageLocations
    QStringList organismsToUpdate;
    // update organism locations that depend on the image locations
    for (int i = 0; i < itemList.length(); i++)
    {
        int j = imageIndexHash.value(itemList.at(i)->text());
        if (images[j].depicts.isEmpty())
            continue;
        organismsToUpdate.append(images[j].depicts);
    }
    organismsToUpdate.removeDuplicates();

    QSqlDatabase db = QSqlDatabase::database();
    db.transaction();

    for (auto organism : organismsToUpdate)
    {
        QSqlQuery findGeoRem;
        findGeoRem.prepare("SELECT dwc_georeferenceRemarks FROM organisms WHERE dcterms_identifier = (?) LIMIT 1");
        findGeoRem.addBindValue(organism);
        findGeoRem.exec();
        if (findGeoRem.next())
        {
            // no need to save coordinates if they're calculate from the average; just recalculate the average
            if (findGeoRem.value(0).toString() == "Location calculated as average of its images' coordinates.")
            {
                averageLocations(organism);
                continue;
            }
        }

        QSqlQuery upImLocQuery;
        upImLocQuery.prepare("UPDATE images SET dwc_decimalLatitude = (?), dwc_decimalLongitude = (?), "
                            "dcterms_modified = (?), dwc_county = (?), dwc_stateProvince = (?), "
                            "dwc_countryCode = (?), dwc_continent = (?), dwc_locality = (?) "
                            "WHERE foaf_depicts = (?) and dwc_georeferenceRemarks = (?)");
        upImLocQuery.addBindValue(lat);
        upImLocQuery.addBindValue(lon);
        upImLocQuery.addBindValue(modifiedNow());
        upImLocQuery.addBindValue(""); // clear county
        upImLocQuery.addBindValue(""); // clear state/province
        upImLocQuery.addBindValue(""); // clear country
        upImLocQuery.addBindValue(""); // clear continent
        upImLocQuery.addBindValue(""); // clear locality
        upImLocQuery.addBindValue(organism);
        upImLocQuery.addBindValue("Location inferred from organism coordinates.");
        upImLocQuery.exec();

        QSqlQuery updatedImagesQuery;
        updatedImagesQuery.prepare("SELECT fileName FROM images WHERE foaf_depicts = (?) AND "
                                   "dwc_georeferenceRemarks = (?)");
        updatedImagesQuery.addBindValue(organism);
        updatedImagesQuery.addBindValue("Location inferred from organism coordinates.");
        updatedImagesQuery.exec();
        while (updatedImagesQuery.next())
        {
            int u = imageIndexHash.value(updatedImagesQuery.value(0).toString());
            if (u == -1)
            {
                qDebug() << "Error: hashIndex not found.";
                continue;
            }

            images[u].decimalLatitude = lat;
            images[u].decimalLongitude = lon;
            images[u].county = "";
            images[u].stateProvince = "";
            images[u].countryCode = "";
            images[u].continent = "";
            images[u].locality = "";
        }

        // save organism latitude except when it is derived from its images
        QSqlQuery upOrgLocQuery;
        upOrgLocQuery.prepare("UPDATE organisms SET dwc_decimalLatitude = (?), dwc_decimalLongitude = (?), "
                              "dcterms_modified = (?) WHERE dcterms_identifier = (?)");
        upOrgLocQuery.addBindValue(lat);
        upOrgLocQuery.addBindValue(lon);
        upOrgLocQuery.addBindValue(modifiedNow());
        upOrgLocQuery.addBindValue(organism);
        upOrgLocQuery.exec();
    }

    if (!db.commit())
    {
        qDebug() << "Problem committing changes to database. Data may be lost.";
        db.rollback();
    }

    refreshInputFields();
}

void DataEntry::on_organismRemarks_textEdited(const QString &arg1)
{
    saveInput("dwc_organismRemarks",arg1);
}

void DataEntry::on_organismName_textEdited(const QString &arg1)
{
    saveInput("dwc_organismName",arg1);
}

void DataEntry::on_organismScope_textEdited(const QString &arg1)
{
    saveInput("dwc_organismScope",arg1);
}

void DataEntry::on_htmlNote_textEdited(const QString &arg1)
{
    saveInput("notes",arg1);
}

void DataEntry::averageLocations(const QString &orgID)
{
    QSqlDatabase db = QSqlDatabase::database();
    db.transaction();

    QSqlQuery oQuery;
    oQuery.prepare("SELECT dcterms_identifier, dwc_georeferenceRemarks FROM organisms WHERE dcterms_identifier = (?) LIMIT 1");
    oQuery.addBindValue(orgID);
    oQuery.exec();

    if (oQuery.next())
    {
        if (oQuery.value(1).toString() != "Location calculated as average of its images' coordinates.")
            return;
    }
    else
    {
        return;
    }

    QList<double> iLats;
    QList<double> iLongs;
    QList<double> iAlts;
    QSqlQuery iQuery;
    iQuery.prepare("SELECT dwc_decimalLatitude, dwc_decimalLongitude, geo_alt, dwc_georeferenceRemarks FROM images WHERE foaf_depicts = (?)");
    iQuery.addBindValue(orgID);
    iQuery.exec();
    while (iQuery.next())
    {
        if (iQuery.value(3).toString() == "Location inferred from organism coordinates.")
            continue;
        if (iQuery.value(0).toString().isEmpty() || iQuery.value(1).toString().isEmpty())
            continue;
        double iLat = iQuery.value(0).toDouble();
        double iLong = iQuery.value(1).toDouble();
        iLats.append(iLat);
        iLongs.append(iLong);
        if (!iQuery.value(2).toString().isEmpty())
        {
            double iAlt = iQuery.value(2).toDouble();
            iAlts.append(iAlt);
        }
    }

    // Average the location values and update the organism record
    QString latString = "";
    QString longString = "";
    QString altString = "";
    int numLatValues = iLats.size();
    if (numLatValues > 0)
    {
        double latAverage;
        double latSum = 0;
        for (double value : iLats)
        {
            latSum += value;
        }
        latAverage = latSum / numLatValues;
        latString = QString::number(latAverage);
    }

    int numLongValues = iLongs.size();
    if (numLongValues > 0)
    {
        double longAverage;
        double longSum = 0;
        for (double value : iLongs)
        {
            longSum += value;
        }
        longAverage = longSum / numLongValues;
        longString = QString::number(longAverage);
    }

    int numAltValues = iAlts.size();
    if (numAltValues > 0)
    {
        double AltAverage;
        double AltSum = 0;
        for (double value : iAlts)
        {
            AltSum += value;
        }
        AltAverage = AltSum / numAltValues;
        altString = QString::number(qRound(AltAverage));
    }

    QSqlQuery uQuery;
    uQuery.prepare("UPDATE organisms SET dwc_decimalLatitude = (?), dwc_decimalLongitude = (?), "
                   "geo_alt = (?), dcterms_modified = (?) WHERE dcterms_identifier = (?)");
    uQuery.addBindValue(latString);
    uQuery.addBindValue(longString);
    uQuery.addBindValue(altString);
    uQuery.addBindValue(modifiedNow());
    uQuery.addBindValue(orgID);
    uQuery.exec();

    QSqlQuery u2Query;
    u2Query.prepare("UPDATE images SET dwc_decimalLatitude = (?), dwc_decimalLongitude = (?), "
                    "geo_alt = (?), dcterms_modified = (?) WHERE foaf_depicts = (?) AND "
                    "dwc_georeferenceRemarks = (?)");
    u2Query.addBindValue(latString);
    u2Query.addBindValue(longString);
    u2Query.addBindValue(altString);
    u2Query.addBindValue(modifiedNow());
    u2Query.addBindValue(orgID);
    u2Query.addBindValue("Location inferred from organism coordinates.");
    u2Query.exec();

    QSqlQuery updatedImagesQuery;
    updatedImagesQuery.prepare("SELECT fileName FROM images WHERE foaf_depicts = (?) AND "
                               "dwc_georeferenceRemarks = (?)");
    updatedImagesQuery.addBindValue(orgID);
    updatedImagesQuery.addBindValue("Location inferred from organism coordinates.");
    updatedImagesQuery.exec();
    while (updatedImagesQuery.next())
    {
        int u = imageIndexHash.value(updatedImagesQuery.value(0).toString());
        if (u == -1)
        {
            qDebug() << "Error: hashIndex not found.";
            continue;
        }

        images[u].decimalLatitude = latString;
        images[u].decimalLongitude = longString;
        images[u].altitudeInMeters = altString;
        images[u].county = "";
        images[u].stateProvince = "";
        images[u].countryCode = "";
        images[u].continent = "";
        images[u].locality = "";
    }

    if (!db.commit())
    {
        qDebug() << "In averageLocations(): Problem committing changes to database. Data may be lost.";
        db.rollback();
    }
}

void DataEntry::on_highResURL_textEdited(const QString &arg1)
{
    saveInput("ac_hasServiceAccessPoint",arg1);
}

void DataEntry::on_zoomIn_clicked()
{
    scaleImage(1.25);
}

void DataEntry::on_zoomOut_clicked()
{
    scaleImage(0.8);
}

void DataEntry::scaleImage(double factor)
{
    scaleFactor *= factor;
    imageLabel->resize(scaleFactor * imageLabel->pixmap()->size());

    adjustScrollBar(ui->scrollArea->horizontalScrollBar(), factor);
    adjustScrollBar(ui->scrollArea->verticalScrollBar(), factor);
}

void DataEntry::adjustScrollBar(QScrollBar *scrollBar, double factor)
{
    scrollBar->setValue(int(factor * scrollBar->value() + ((factor - 1) * scrollBar->pageStep()/2)));
}

QString DataEntry::viewToNumbers(const QString &group, const QString &part, const QString &view)
{
    QString pt1 = "00";
    QString pt2 = "00";
    QString pt3 = "00";
    if (group == "unspecified")
    {
        if (part == "whole organism")
            pt2 = "01";
    }
    else if (group == "woody angiosperms")
    {
        pt1 = "01";
        if (part == "whole tree (or vine)")
        {
            pt2 = "01";
            if (view == "general")
                pt3 = "01";
            else if (view == "winter")
                pt3 = "02";
            else if (view == "view up trunk")
                pt3 = "03";
        }
        else if (part == "bark")
        {
            pt2 = "02";
            if (view == "of a large tree")
                pt3 = "01";
            else if (view == "of a medium tree or large branch")
                pt3 = "02";
            else if (view == "of a small tree or small branch")
                pt3 = "03";
        }
        else if (part == "twig")
        {
            pt2 = "03";
            if (view == "orientation of petioles")
                pt3 = "01";
            else if (view == "winter overall")
                pt3 = "02";
            else if (view == "close-up winter leaf scar/bud")
                pt3 = "03";
            else if (view == "close-up winter terminal bud")
                pt3 = "04";
        }
        else if (part == "leaf")
        {
            pt2 = "04";
            if (view == "whole upper surface")
                pt3 = "01";
            else if (view == "margin of upper + lower surface")
                pt3 = "02";
            else if (view == "showing orientation on twig")
                pt3 = "03";
        }
        else if (part == "inflorescence")
        {
            pt2 = "05";
            if (view == "whole - unspecified")
                pt3 = "01";
            else if (view == "whole - female")
                pt3 = "02";
            else if (view == "whole - male")
                pt3 = "03";
            else if (view == "lateral view of flower")
                pt3 = "04";
            else if (view == "frontal view of flower")
                pt3 = "05";
            else if (view == "ventral view of flower + perianth")
                pt3 = "06";
            else if (view == "close-up of flower interior")
                pt3 = "07";
        }
        else if (part == "fruit")
        {
            pt2 = "06";
            if (view == "as borne on the plant")
                pt3 = "01";
            else if (view == "lateral or general close-up")
                pt3 = "02";
            else if (view == "section or open")
                pt3 = "03";
            else if (view == "immature")
                pt3 = "04";
        }
        else if (part == "seed")
        {
            pt2 = "07";
            if (view == "general view")
                pt3 = "01";
        }
    }
    else if (group == "herbaceous angiosperms")
    {
        pt1 = "02";
        if (part == "whole plant")
        {
            pt2 = "01";
            if (view == "juvenile")
                pt3 = "01";
            else if (view == "in flower - general view")
                pt3 = "02";
            else if (view == "in fruit")
                pt3 = "03";
        }
        else if (part == "stem")
        {
            pt2 = "02";
            if (view == "showing leaf bases")
                pt3 = "01";
        }
        else if (part == "leaf")
        {
            pt2 = "03";
            if (view == "basal or on lower stem")
                pt3 = "01";
            else if (view == "on upper stem")
                pt3 = "02";
            else if (view == "margin of upper + lower surface")
                pt3 = "03";
        }
        else if (part == "inflorescence")
        {
            pt2 = "04";
            if (view == "whole - unspecified")
                pt3 = "01";
            else if (view == "whole - female")
                pt3 = "02";
            else if (view == "whole - male")
                pt3 = "03";
            else if (view == "lateral view of flower")
                pt3 = "04";
            else if (view == "frontal view of flower")
                pt3 = "05";
            else if (view == "ventral view of flower + perianth")
                pt3 = "06";
            else if (view == "close-up of flower interior")
                pt3 = "07";
        }
        else if (part == "fruit")
        {
            pt2 = "05";
            if (view == "as borne on the plant")
                pt3 = "01";
            else if (view == "lateral or general close-up")
                pt3 = "02";
            else if (view == "section or open")
                pt3 = "03";
            else if (view == "immature")
                pt3 = "04";
        }
        else if (part == "seed")
        {
            pt2 = "06";
            if (view == "general view")
                pt3 = "01";
        }
    }
    else if (group == "gymnosperms")
    {
        pt1 = "03";
        if (part == "whole tree")
        {
            pt2 = "01";
            if (view == "general")
                pt3 = "01";
            else if (view == "view up trunk")
                pt3 = "02";
        }
        else if (part == "bark")
        {
            pt2 = "02";
            if (view == "of a large tree")
                pt3 = "01";
            else if (view == "of a medium tree or large branch")
                pt3 = "02";
            else if (view == "of a small tree or small branch")
                pt3 = "03";
        }
        else if (part == "twig")
        {
            pt3 = "03";
            if (view == "after fallen needles")
                pt3 = "01";
            else if (view == "showing attachment of needles")
                pt3 = "02";
        }
        else if (part == "leaf")
        {
            pt2 = "04";
            if (view == "entire needle")
                pt3 = "01";
            else if (view == "showing orientation on twig")
                pt3 = "02";
        }
        else if (part == "cone")
        {
            pt2 = "05";
            if (view == "male")
                pt3 = "01";
            else if (view == "female - mature open")
                pt3 = "02";
            else if (view == "female - closed")
                pt3 = "03";
            else if (view == "female - receptive")
                pt3 = "04";
            else if (view == "one year-old female")
                pt3 = "05";
        }
        else if (part == "seed")
        {
            pt2 = "06";
            if (view == "general view")
                pt3 = "01";
        }
    }
    else if (group == "ferns")
    {
        pt1 = "04";
        if (part == "whole plant")
            pt2 = "01";
    }
    else if (group == "cacti")
    {
        pt1 = "05";
        if (part == "whole plant")
            pt2 = "01";
    }
    else if (group == "mosses")
    {
        pt1 = "06";
        if (part == "whole gametophyte")
            pt2 = "01";
    }

    QString imageView = "#" + pt1 + pt2 + pt3;
    return imageView;
}

QString DataEntry::singleResult(const QString result, const QString table, const QString field, const QString value)
{
    QSqlQuery query;
    query.prepare("SELECT " + result + " FROM " + table + " WHERE " + field + " = (?)");
    query.addBindValue(value);
    query.exec();

    if (query.next())
        return query.value(0).toString();
    else
        return "";
}

QStringList DataEntry::numbersToView(const QString &numbers)
{
    // Convert imageview #010203 format to group/part/view QStrings
    QString group = "unspecified";
    QString part = "unspecified";
    QString view = "unspecified";
    QString imageView = numbers;
    imageView.remove("#");
    // make sure imageView is 6 digits long before splitting
    if (imageView.length() == 6)
    {
        QString pt1 = QString(imageView.at(0)) + QString(imageView.at(1));
        QString pt2 = QString(imageView.at(2)) + QString(imageView.at(3));
        QString pt3 = QString(imageView.at(4)) + QString(imageView.at(5));

        if (pt1 == "00" && pt2 == "01")
            part = "whole organism";
        else if (pt1 == "01")
        {
            group = "woody angiosperms";
            if (pt2 == "01")
            {
                part = "whole tree (or vine)";
                if (pt3 == "01")
                    view = "general";
                else if (pt3 == "02")
                    view = "winter";
                else if (pt3 == "03")
                    view = "view up trunk";
            }
            else if (pt2 == "02")
            {
                part = "bark";
                if (pt3 == "01")
                    view = "of a large tree";
                else if (pt3 == "02")
                    view  = "of a medium tree or large branch";
                else if (pt3 == "03")
                    view = "of a small tree or small branch";
            }
            else if (pt2 == "03")
            {
                part = "twig";
                if (pt3 == "01")
                    view = "orientation of petioles";
                else if (pt3 == "02")
                    view = "winter overall";
                else if (pt3 == "03")
                    view = "close-up winter leaf scar/bud";
                else if (pt3 == "04")
                    view = "close-up winter terminal bud";
            }
            else if (pt2 == "04")
            {
                part = "leaf";
                if (pt3 == "01")
                    view = "whole upper surface";
                else if (pt3 == "02")
                    view = "margin of upper + lower surface";
                else if (pt3 == "03")
                    view = "showing orientation on twig";
            }
            else if (pt2 == "05")
            {
                part = "inflorescence";
                if (pt3 == "01")
                    view = "whole - unspecified";
                else if (pt3 == "02")
                    view = "whole - female";
                else if (pt3 == "03")
                    view = "whole - male";
                else if (pt3 == "04")
                    view = "lateral view of flower";
                else if (pt3 == "05")
                    view = "frontal view of flower";
                else if (pt3 == "06")
                    view = "ventral view of flower + perianth";
                else if (pt3 == "07")
                    view = "close-up of flower interior";
            }
            else if (pt2 == "06")
            {
                part = "fruit";
                if (pt3 == "01")
                    view = "as borne on the plant";
                else if (pt3 == "02")
                    view = "lateral or general close-up";
                else if (pt3 == "03")
                    view = "section or open";
                else if (pt3 == "04")
                    view = "immature";
            }
            else if (pt2 == "07")
            {
                part = "seed";
                if (pt3 == "01")
                    view = "general view";
            }
        }
        else if (pt1 == "02")
        {
            group = "herbaceous angiosperms";
            if (pt2 == "01")
            {
                part = "whole plant";
                if (pt3 == "01")
                    view = "juvenile";
                else if (pt3 == "02")
                    view = "in flower - general view";
                else if (pt3 == "03")
                    view = "in fruit";
            }
            else if (pt2 == "02")
            {
                part = "stem";
                if (pt3 == "01")
                    view = "showing leaf bases";
            }
            else if (pt2 == "03")
            {
                part = "leaf";
                if (pt3 == "01")
                    view = "basal or on lower stem";
                else if (pt3 == "02")
                    view = "on upper stem";
                else if (pt3 == "03")
                    view = "margin of upper + lower surface";
            }
            else if (pt2 == "04")
            {
                part = "inflorescence";
                if (pt3 == "01")
                    view = "whole - unspecified";
                else if (pt3 == "02")
                    view = "whole - female";
                else if (pt3 == "03")
                    view = "whole - male";
                else if (pt3 == "04")
                    view = "lateral view of flower";
                else if (pt3 == "05")
                    view = "frontal view of flower";
                else if (pt3 == "06")
                    view = "ventral view of flower + perianth";
                else if (pt3 == "07")
                    view = "close-up of flower interior";
            }
            else if (pt2 == "05")
            {
                part = "fruit";
                if (pt3 == "01")
                    view = "as borne on the plant";
                else if (pt3 == "02")
                    view = "lateral or general close-up";
                else if (pt3 == "03")
                    view = "section or open";
                else if (pt3 == "04")
                    view = "immature";
            }
            else if (pt2 == "06")
            {
                part = "seed";
                if (pt3 == "01")
                    view = "general view";
            }
        }
        else if (pt1 == "03")
        {
            group = "gymnosperms";
            if (pt2 == "01")
            {
                part = "whole tree";
                if (pt3 == "01")
                    view = "general";
                else if (pt3 == "02")
                    view = "view up trunk";
            }
            else if (pt2 == "02")
            {
                part = "bark";
                if (pt3 == "01")
                    view = "of a large tree";
                else if (pt3 == "02")
                    view = "of a medium tree or large branch";
                else if (pt3 == "03")
                    view = "of a small tree or small branch";
            }
            else if (pt2 == "03")
            {
                part = "twig";
                if (pt3 == "01")
                    view = "after fallen needles";
                else if (pt3 == "02")
                    view = "showing attachment of needles";
            }
            else if (pt2 == "04")
            {
                part = "leaf";
                if (pt3 == "01")
                    view = "entire needle";
                else if (pt3 == "02")
                    view = "showing orientation on twig";
            }
            else if (pt2 == "05")
            {
                part = "cone";
                if (pt3 == "01")
                    view = "male";
                else if (pt3 == "02")
                    view = "female - mature open";
                else if (pt3 == "03")
                    view = "female - closed";
                else if (pt3 == "04")
                    view = "female - receptive";
                else if (pt3 == "05")
                    view = "one year-old female";
            }
            else if (pt2 == "06")
            {
                part = "seed";
                if (pt3 == "01")
                    view = "general view";
            }
        }
        else if (pt1 == "04")
        {
            group = "ferns";
            if (pt2 == "01")
                part = "whole plant";
        }
        else if (pt1 == "05")
        {
            group = "cacti";
            if (pt2 == "01")
                part = "whole plant";
        }
        else if (pt1 == "06")
        {
            group = "mosses";
            if (pt2 == "01")
                part = "whole gametophyte";
        }
    }

    QStringList groupPartView;
    groupPartView.append(group);
    groupPartView.append(part);
    groupPartView.append(view);
    return groupPartView;
}

void DataEntry::on_newDeterminationButton_clicked()
{
    if (ui->organismID->text().isEmpty())
    {
        QMessageBox msgBox;
        msgBox.setText("You must set the organism ID before adding a determination.");
        msgBox.exec();
        return;
    }

    if (ui->organismID->text() == "<multiple>")
    {
        QMessageBox msgBox;
        msgBox.setText("Adding determinations for multiple organisms at\n"
                       "the same time is not currently supported. Please\n"
                       "select only one organism then try again.");
        msgBox.exec();
        return;
    }
    NewDeterminationDialog determinationDialog(ui->organismID->text(), allGenera, allCommonNames, agents, sensus, this);
    int dialogResult = determinationDialog.exec();

    if (determinationDialog.newSensu())
    {
        loadSensu();
        QString currentNameAccordingToID = ui->nameAccordingToID->currentText();
        ui->nameAccordingToID->clear();
        sensus.sort(Qt::CaseInsensitive);
        ui->nameAccordingToID->addItems(sensus);
        ui->nameAccordingToID->setCurrentText(currentNameAccordingToID);
        ui->nameAccordingToID->view()->setMinimumWidth(500);
        ui->nameAccordingToID->view()->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    }

    if (dialogResult == QDialog::Rejected)
        return;

    Determination d = determinationDialog.getDetermination();
    d.lastModified = modifiedNow();

    QSqlDatabase db = QSqlDatabase::database();
    db.transaction();

    // if tsnID is not in Determinations::tsnID yet, set Taxa::dcterms_modified to now() for that ID
    QSqlQuery firstQuery;
    firstQuery.prepare("SELECT 1 FROM determinations WHERE tsnID = (?) LIMIT 1");
    firstQuery.addBindValue(d.tsnID);
    firstQuery.exec();
    if (!firstQuery.next())
    {
        QSqlQuery updateModified;
        updateModified.prepare("UPDATE taxa SET dcterms_modified = (?) WHERE dcterms_identifier = (?)");
        updateModified.addBindValue(modifiedNow());
        updateModified.addBindValue(d.tsnID);
        updateModified.exec();
    }

    QString nameAccordingTo = d.nameAccordingToID.split(" ").last();
    nameAccordingTo.remove("(");
    nameAccordingTo.remove(")");
    if (nameAccordingTo.isEmpty())
        nameAccordingTo = "nominal";

    QSqlQuery query;
    query.prepare("INSERT INTO determinations (dsw_identified, identifiedBy, dwc_dateIdentified, "
                  "dwc_identificationRemarks, tsnID, nameAccordingToID, dcterms_modified, suppress) "
                  "VALUES (?, ?, ?, ?, ?, ?, ?, ?)");
    query.addBindValue(d.identified);
    query.addBindValue(d.identifiedBy);
    query.addBindValue(d.dateIdentified);
    query.addBindValue(d.identificationRemarks);
    query.addBindValue(d.tsnID);
    query.addBindValue(nameAccordingTo);
    query.addBindValue(d.lastModified);
    query.addBindValue(d.suppress);
    if (!query.exec()) {
        QMessageBox::critical(0, "", "Determination insertion failed: " + query.lastError().text());
        return;
    }

    // now that a determination is set we need to update all of its images' title and description
    autosetTitle(d.tsnID, d.identified);

    if (!db.commit())
    {
        qDebug() << "In newDetermination(): Problem committing changes to database. Data may be lost.";
        db.rollback();
    }

    refreshInputFields();
}

void DataEntry::autosetTitle(QString tsnID, QString organismID)
{
    // first get the genus, species and family names
    QSqlQuery nQuery;
    nQuery.prepare("SELECT dwc_family, dwc_genus, dwc_specificEpithet, dwc_infraspecificEpithet from taxa where dcterms_identifier = (?) LIMIT 1");
    nQuery.addBindValue(tsnID);
    nQuery.exec();

    QString family;
    QString genus;
    QString species;
    QString infraspecific;
    if (nQuery.next())
    {
        family = nQuery.value(0).toString();
        genus = nQuery.value(1).toString();
        species = nQuery.value(2).toString();
        infraspecific = nQuery.value(3).toString();
    }

    // then set the image titles and descriptions
    QSqlQuery imageQuery;
    imageQuery.prepare("SELECT dcterms_identifier, view FROM images WHERE foaf_depicts = (?)");
    imageQuery.addBindValue(organismID);
    imageQuery.exec();

    while (imageQuery.next())
    {
        QStringList groupPartView = numbersToView(imageQuery.value(1).toString());
        if (groupPartView.size() == 3)
        {
            QString group = groupPartView.at(0);
            QString part = groupPartView.at(1);
            QString view = groupPartView.at(2);
            QString title = genus;
            if (!species.isEmpty())
                title = title + " " + species;
            if (!family.isEmpty())
                title = title + " (" + family + ")";
            if (!part.isEmpty() && part != "unspecified")
            {
                title = title + " - " + part;
                if (!view.isEmpty() && view != "unspecified")
                    title = title + " - " + view;
            }
            QString description = "";
            if (!title.isEmpty())
                description = "Image of " + title;
            QSqlQuery imageUpdate;
            imageUpdate.prepare("UPDATE images SET dcterms_title = (?), dcterms_description = (?), "
                                "dcterms_modified = (?) WHERE dcterms_identifier = (?)");
            imageUpdate.addBindValue(title);
            imageUpdate.addBindValue(description);
            imageUpdate.addBindValue(modifiedNow());
            imageUpdate.addBindValue(imageQuery.value(0).toString());
            imageUpdate.exec();
        }
    }
}

void DataEntry::refreshAgentDropdowns()
{
    agents.sort(Qt::CaseInsensitive);
    QCompleter *completeAgent = new QCompleter(agents, this);
    completeAgent->setCaseSensitivity(Qt::CaseInsensitive);

    QString copyrightOwnerText = ui->copyrightOwner->currentText();
    ui->copyrightOwner->clear();
    ui->copyrightOwner->setCompleter(completeAgent);
    ui->copyrightOwner->addItems(agents);
    ui->copyrightOwner->setCurrentText(copyrightOwnerText);

    QString photographerText = ui->photographer->currentText();
    ui->photographer->clear();
    ui->photographer->setCompleter(completeAgent);
    ui->photographer->addItems(agents);
    ui->photographer->setCurrentText(photographerText);

    QString collectionCodeText = ui->collectionCode->currentText();
    QStringList collection = agents;
    collection.insert(0,"");    // it's ok not to have a collectionCode
    ui->collectionCode->setCompleter(completeAgent);
    ui->collectionCode->addItems(collection);
    ui->collectionCode->setCurrentText(collectionCodeText);
}

void DataEntry::on_nextDetermination_clicked()
{
    if (currentDetermination < 1 || currentDeterminations.size() < 2 || currentDetermination == currentDeterminations.size())
        return;

    currentDetermination++;
    Determination d = currentDeterminations.at(currentDetermination-1);
    ui->numberOfDeterminations->setText(QString::number(currentDetermination) + " of " + QString::number(currentDeterminations.size()));

    ui->identifiedBy->setText(d.identifiedBy);
    QString identifiedByFull = singleResult("dc_contributor","agents","dcterms_identifier",d.identifiedBy);
    ui->identifiedBy->setToolTip(identifiedByFull);
    ui->dateIdentified->setText(d.dateIdentified);
    QString s = sensuHash.value(d.nameAccordingToID) + " (" + d.nameAccordingToID + ")";
    ui->nameAccordingToID->setCurrentText(s);
    ui->nameAccordingToID->setToolTip(s);
    ui->identificationRemarks->setText(d.identificationRemarks);
    ui->identificationRemarks->setToolTip(d.identificationRemarks);
    setTaxaFromTSN(d.tsnID);
}

void DataEntry::on_previousDetermation_clicked()
{
    if (currentDetermination <= 1 || currentDeterminations.size() < 2)
        return;

    currentDetermination--;
    Determination d = currentDeterminations.at(currentDetermination-1);
    ui->numberOfDeterminations->setText(QString::number(currentDetermination) + " of " + QString::number(currentDeterminations.size()));

    ui->identifiedBy->setText(d.identifiedBy);
    QString identifiedByFull = singleResult("dc_contributor","agents","dcterms_identifier",d.identifiedBy);
    ui->identifiedBy->setToolTip(identifiedByFull);
    ui->dateIdentified->setText(d.dateIdentified);
    QString s = sensuHash.value(d.nameAccordingToID) + " (" + d.nameAccordingToID + ")";
    ui->nameAccordingToID->setCurrentText(s);
    ui->nameAccordingToID->setToolTip(s);
    ui->identificationRemarks->setText(d.identificationRemarks);
    ui->identificationRemarks->setToolTip(d.identificationRemarks);
    setTaxaFromTSN(d.tsnID);
}

void DataEntry::on_organismAltitude_textEdited(const QString &arg1)
{
    QList<QListWidgetItem*> itemList = ui->thumbWidget->selectedItems();
    if (arg1 == "<multiple>" || itemList.isEmpty())
        return;

    // for each foaf_depicts, see if its current altitude matches arg1, if so continue
    // otherwise update its georefRemarks and if applicable call averageLocations
    QStringList organismsToUpdate;
    // update organism locations that depend on the image locations
    for (int i = 0; i < itemList.length(); i++)
    {
        int j = imageIndexHash.value(itemList.at(i)->text());
        if (images[j].depicts.isEmpty())
            continue;
        organismsToUpdate.append(images[j].depicts);
    }
    organismsToUpdate.removeDuplicates();

    QSqlDatabase db = QSqlDatabase::database();
    db.transaction();

    for (auto organism : organismsToUpdate)
    {
        QSqlQuery qry;
        qry.prepare("SELECT geo_alt FROM organisms WHERE dcterms_identifier = (?) LIMIT 1");
        qry.addBindValue(organism);
        qry.exec();

        if (qry.next())
        {
            if (qry.value(0).toString() == arg1)
                continue;

            QSqlQuery upAltQuery;
            upAltQuery.prepare("UPDATE images SET geo_alt = (?), dcterms_modified = (?) "
                                "WHERE foaf_depicts = (?) and dwc_georeferenceRemarks = (?)");
            upAltQuery.addBindValue(arg1);
            upAltQuery.addBindValue(modifiedNow());
            upAltQuery.addBindValue(organism);
            upAltQuery.addBindValue("Location inferred from organism coordinates.");
            upAltQuery.exec();

            QSqlQuery oQuery;
            oQuery.prepare("UPDATE organisms SET geo_alt = (?), dcterms_modified = (?) WHERE dcterms_identifier = (?)");
            oQuery.addBindValue(arg1);
            oQuery.addBindValue(modifiedNow());
            oQuery.addBindValue(organism);
            oQuery.exec();
        }
    }

    if (!db.commit())
    {
        qDebug() << "Problem committing changes to database. Data may be lost.";
        db.rollback();
    }

    refreshInputFields();
}

void DataEntry::on_image_timezone_box_textEdited(const QString &arg1)
{
    saveInput("imageTimezone",arg1);
}

void DataEntry::on_photoshop_Credit_textEdited(const QString &arg1)
{
    saveInput("photoshop_Credit",arg1);
}

void DataEntry::removeUnlinkedOrganisms()
{
    // without this WHERE, many organisms that don't have corresponding images loaded as
    // thumbnails would be accidentally deleted...
    QString where = " WHERE dcterms_modified > '" + dbLastPublished + "'";

    QStringList linkedOrganisms;

    QSqlQuery imageQry;
    imageQry.prepare("SELECT foaf_depicts FROM images");
    imageQry.exec();
    while (imageQry.next())
    {
        linkedOrganisms.append(imageQry.value(0).toString());
    }

    QSqlQuery orgChanges;
    orgChanges.prepare("SELECT dcterms_identifier FROM organisms" + where);
    orgChanges.exec();
    QStringList badIDs;
    while (orgChanges.next())
    {
        QString id = orgChanges.value(0).toString();
        if (!linkedOrganisms.contains(id))
        {
            // unlinked organism found; remove it
            badIDs.append(id);
        }
    }

    for (auto id : badIDs)
    {
        QSqlQuery deleteQry;
        deleteQry.prepare("DELETE FROM organisms WHERE dcterms_identifier = (?)");
        deleteQry.addBindValue(id);
        deleteQry.exec();

        qDebug() << "Deleting '" + id + "' from organisms table: not linked to any images.";
    }
}

void DataEntry::on_actionExport_entire_database_to_CSV_files_triggered()
{
    QString where = "";
    QString tablePrefix = "";

    ExportCSV exportCSV;
    exportCSV.saveData(where,tablePrefix);
}

void DataEntry::on_imageTitle_textEdited(const QString &arg1)
{
    saveInput("dcterms_title", arg1);
}

void DataEntry::on_imageDescription_textEdited(const QString &arg1)
{
    saveInput("dcterms_description", arg1);
}

void DataEntry::on_geonamesOtherButton_clicked()
{
    if (ui->thumbWidget->selectedItems().isEmpty())
        return;
    GeonamesOther geonamesOther(this);
    int dialogResult = geonamesOther.exec();

    if (dialogResult == QDialog::Rejected)
        return;

    QString otherID = geonamesOther.getSelectedID();
    saveInput("geonamesOther", otherID);
    ui->geonamesOther->setText(otherID);
}

void DataEntry::on_actionReturn_to_Start_Screen_triggered()
{
    this->hide();
    emit windowClosed();
    this->close();
}

void DataEntry::on_advancedThumbnailFilter_clicked()
{
    AdvancedThumbnailFilter advancedFilter(this);
    int dialogResult = advancedFilter.exec();

    if (dialogResult == QDialog::Rejected)
        return;

    for (auto sim : similarImages)
    {
        ui->thumbWidget->item(sim)->setBackground(Qt::NoBrush);
    }
    similarImages.clear();
    ui->thumbnailFilter->setCurrentIndex(0);

    QString filter = advancedFilter.getAdvancedFilter();
    if (filter.trimmed().isEmpty() || filter.toLower() == "select filename from images where ")
    {
        for (int x = 0; x < ui->thumbWidget->count(); x++)
            ui->thumbWidget->item(x)->setHidden(false);
        return;
    }

    // run the query "filter"
    QSqlQuery filterQuery;
    filterQuery.prepare(filter); // the query needs to be of the form: SELECT fileName FROM images WHERE <something>
    filterQuery.exec();
    QStringList showList;
    while (filterQuery.next())
    {
        showList.append(filterQuery.value(0).toString());
    }
    // save the image::fileNames returned from the query as QStringList showList
    // loop through every image thumbnail. if the showList.contains(thumbnail-filename), show
    for (int x = 0; x < ui->thumbWidget->count(); x++)
    {
        // first make each thumbnail visible--we don't know its state when this filter was applied
        ui->thumbWidget->item(x)->setHidden(true);

        int h = imageIndexHash.value(ui->thumbWidget->item(x)->text());
        if (showList.contains(images[h].fileName))
            ui->thumbWidget->item(x)->setHidden(false);
    }
}

void DataEntry::on_thumbnailSort_activated(const QString &arg1)
{
    if (arg1 == "Sort")
        return;
    ui->thumbWidget->clearSelection();
    for (auto sim : similarImages)
    {
        ui->thumbWidget->item(sim)->setBackground(Qt::NoBrush);
    }
    similarImages.clear();
    currentThumbnailSort = arg1;
    // we want the sort QComboBox to always display the "Sort" header
    ui->thumbnailSort->setCurrentText("Sort");

    // before sorting make all thumbnails visible or sorting takes forever
    for (int x = 0; x < ui->thumbWidget->count(); x++)
    {
        ui->thumbWidget->item(x)->setHidden(false);
    }

    // let's get a list of the dcterms_identifiers of all image thumbnails
    // then sort it, end up with another list of dcterms_identifiers
    QStringList dcterms_identifiers;
    if (arg1 == "Image dcterms_identifier A->Z")
    {
        QMapIterator<QString, QString> i(imageIDFilenameMap);
        while (i.hasNext()) {
            i.next();
            dcterms_identifiers.append(i.value());
        }
    }
    else if (arg1 == "Image filename A->Z")
    {
        dcterms_identifiers = imageIDFilenameMap.values();
        dcterms_identifiers.sort(Qt::CaseInsensitive);
    }
    else if (arg1 == "Image date/time Old->New")
    {
        QMapIterator<QString, QString> i(dateFilenameMap);
        while (i.hasNext()) {
            i.next();
            dcterms_identifiers.append(i.value());
        }
    }
    else if (arg1 == "Image date/time New->Old")
    {
        QMapIterator<QString, QString> i(dateFilenameMap);
        i.toBack();
        while (i.hasPrevious()) {
            i.previous();
            dcterms_identifiers.append(i.value());
        }
    }
    else if (arg1 == "Organism ID that image depicts A->Z")
    {
        // create an ordered QStringList grouped by foaf_depicts
        QMap<QString,QString> imageDepictsMap;
        // so first create a query that gets all image dcterms_identifiers and foaf_depicts
        QSqlQuery imageDepictsQry;
        imageDepictsQry.prepare("SELECT fileName, foaf_depicts FROM images ORDER BY fileName desc");
        imageDepictsQry.exec();
        while (imageDepictsQry.next())
        {
            QString filename = imageDepictsQry.value(0).toString();
            QString depicts = imageDepictsQry.value(1).toString();
            if (filename.isEmpty())
                continue;
            if (!imageIndexHash.contains(filename))
                continue;
            // if the dcterms_identifier is in the thumbnail list, then add to map its id and foaf_depicts
            imageDepictsMap.insertMulti(depicts,filename);
        }
        // when done, iterate through the map like above examples..
        foreach (QString value, imageDepictsMap)
        {
            dcterms_identifiers.append(value);
        }
    }

    // loop through that list, extracting the QListWidget pertaining to it and adding it to reorderedListWidgetList
    QList<QListWidgetItem*> reorderedListWidgetList;

    QStringList oldItems = thumbWidgetItems;
    thumbWidgetItems.clear();
    for (QString id : dcterms_identifiers)
    {
        int row = oldItems.indexOf(id);

        if (row == -1)
        {
            qDebug() << "row == -1 for id: " + id;
            continue;
        }

        reorderedListWidgetList.append(ui->thumbWidget->takeItem(row));
        oldItems.removeOne(id);
        thumbWidgetItems.append(id);
    }

    for (auto item : reorderedListWidgetList)
    {
        ui->thumbWidget->addItem(item);
    }
}

void DataEntry::on_actionNew_agent_triggered()
{
    NewAgentDialog agentDialog(agents, this);
    int dialogResult = agentDialog.exec();

    if (dialogResult == QDialog::Rejected)
        return;

    QString agentCode = agentDialog.getAgentCode();
    QString fullName = agentDialog.getFullName();
    QString agentURI = agentDialog.getAgentURI();
    QString contactURL = agentDialog.getContactURL();
    QString morphbankID = agentDialog.getMorphbankID();
    QString agentType = agentDialog.getAgentType();

    QString lastModified = modifiedNow();

    QSqlQuery query;
    query.prepare("INSERT INTO agents (dcterms_identifier, dc_contributor, iri, "
                  "contactURL, morphbankUserID, dcterms_modified, type) "
                  "VALUES (?, ?, ?, ?, ?, ?, ?)");
    query.addBindValue(agentCode);
    query.addBindValue(fullName);
    query.addBindValue(agentURI);
    query.addBindValue(contactURL);
    query.addBindValue(morphbankID);
    query.addBindValue(lastModified);
    query.addBindValue(agentType);
    if (!query.exec()) {
        QMessageBox::critical(0, "", "Agent insertion failed: " + query.lastError().text());
        return;
    }

    agentHash.insert(agentCode, fullName);
    agents.append(agentCode);
    refreshAgentDropdowns();
}

void DataEntry::on_actionNew_sensu_triggered()
{
    NewSensuDialog sensuDialog(sensus, this);
    int dialogResult = sensuDialog.exec();

    if (dialogResult == QDialog::Rejected)
        return;

    Sensu s = sensuDialog.getSensu();
    s.lastModified = modifiedNow();

    QSqlDatabase db = QSqlDatabase::database();
    db.transaction();

    QSqlQuery query;
    query.prepare("INSERT INTO sensu (dcterms_identifier, dc_creator, tcsSignature, "
                  "dcterms_title, dc_publisher, dcterms_created, iri, dcterms_modified) "
                  "VALUES (?, ?, ?, ?, ?, ?, ?, ?)");
    query.addBindValue(s.identifier);
    query.addBindValue(s.creator);
    query.addBindValue(s.tcsSignature);
    query.addBindValue(s.title);
    query.addBindValue(s.publisher);
    query.addBindValue(s.dcterms_created);
    query.addBindValue(s.iri);
    query.addBindValue(s.lastModified);
    if (!query.exec()) {
        QMessageBox::critical(0, "", "New Source of Name insertion failed: " + query.lastError().text());
        return;
    }

    if (!db.commit())
    {
        qDebug() << "In newSensu(): Problem committing changes to database. Data may be lost.";
        db.rollback();
    }

    loadSensu();

    QString currentNameAccordingToID = ui->nameAccordingToID->currentText();
    ui->nameAccordingToID->clear();
    sensus.sort(Qt::CaseInsensitive);
    ui->nameAccordingToID->addItems(sensus);
    completeSensu = new QCompleter(sensus, this);
    completeSensu->setCaseSensitivity(Qt::CaseInsensitive);
    completeSensu->popup()->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    completeSensu->popup()->setMinimumWidth(300);
    completeSensu->setFilterMode(Qt::MatchContains);
    ui->nameAccordingToID->setCompleter(completeSensu);
    ui->nameAccordingToID->setCurrentText(currentNameAccordingToID);
    ui->nameAccordingToID->view()->setMinimumWidth(500);
    ui->nameAccordingToID->view()->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
}

void DataEntry::on_photographer_activated(const QString &arg1)
{
    if (arg1 == "<multiple>" || arg1 == "")
        return;

    QList<QListWidgetItem*> itemList = ui->thumbWidget->selectedItems();

    if (itemList.isEmpty())
        return;

    int h = imageIndexHash.value(itemList.at(0)->text());
    if (h == -1)
    {
        qDebug() << "Error: hashIndex not found when setting photographer code";
        return;
    }
    QSqlQuery qry;
    qry.prepare("SELECT photographerCode FROM images WHERE dcterms_identifier = (?) LIMIT 1");
    qry.addBindValue(images[h].identifier);
    qry.exec();

    if (qry.next())
    {
        if (itemList.length() == 1 && qry.value(0).toString() == arg1)
            return;

        ui->photographer->setToolTip(agentHash.value(arg1));
        saveInput("photographerCode",arg1);
    }
}

void DataEntry::on_iconifyDone()
{
    ui->thumbnailSort->setEnabled(true);

    for (QListWidgetItem* item : ui->thumbWidget->findItems("*", Qt::MatchWildcard))
    {
        item->setSizeHint(QSize(104,119));
    }

    qDebug() << "Finished creating thumbnails";
}

void DataEntry::showContextMenu(const QPoint &pos)
{
    if (ui->thumbWidget->selectedItems().size() < 1)
        return;

    // Handle global position
    QPoint globalPos = ui->thumbWidget->mapToGlobal(pos);

    // Create menu and insert some actions
    QMenu menu;
    menu.addAction("Delete selected", this, SLOT(deleteThumbnail()));

    // Show context menu at handling position
    menu.exec(globalPos);
}

void DataEntry::deleteThumbnail()
{
    int numSelected = ui->thumbWidget->selectedItems().size();
    QString warning = "Are you sure you want to delete the " + QString::number(numSelected) +
            " selected thumbnails?\nAll record of them will be removed from the database.";
    if (numSelected < 1)
        return;
    else if (numSelected == 1)
        warning = "Are you sure you want to delete the selected thumbnail?\nAll record of it will be removed from the database.";

    if (QMessageBox::Yes == QMessageBox(QMessageBox::Information, "Delete confirmation",
                                        warning,
                                        QMessageBox::Yes|QMessageBox::No).exec())
    {
        pauseRefreshing = true;
        for (auto sim : similarImages)
        {
            ui->thumbWidget->item(sim)->setBackground(Qt::NoBrush);
        }
        similarImages.clear();
        // If multiple items are selected we need to erase all of them
        QStringList identifiers;
        for (int i = 0; i < numSelected; ++i)
        {
            // Get current item on selected row
            QListWidgetItem *item = ui->thumbWidget->takeItem(ui->thumbWidget->currentRow());
            QString filename = item->text();
            int imageIndex = imageIndexHash.value(filename);
            identifiers.append(images[imageIndex].identifier);
            // DON'T remove from "images" QList or it messes up the imageIndexHash!
            // Do remove from imageIndexHash
            imageIndexHash.remove(filename);
            // Remove from imageIDFilenameMap
            imageIDFilenameMap.remove(imageIDFilenameMap.key(filename));
            // Remove from dateFilenameMap
            dateFilenameMap.remove(dateFilenameMap.key(filename));
            // And finally remove the thumbnail
            delete item;
        }
        QSqlDatabase db = QSqlDatabase::database();
        db.transaction();
        for (auto identifier : identifiers)
        {
            // Remove selected from images table in database
            QSqlQuery deleteQuery;
            deleteQuery.prepare("DELETE FROM images WHERE dcterms_identifier = (?)");
            deleteQuery.addBindValue(identifier);
            deleteQuery.exec();

            QSqlQuery revertQuery;
            revertQuery.prepare("INSERT OR REPLACE INTO images SELECT * FROM pub_images "
                                "WHERE dcterms_identifier = (?) LIMIT 1");
            revertQuery.addBindValue(identifier);
            revertQuery.exec();
        }
        if (!db.commit())
        {
            qDebug() << "In deleteThumbnail(): Problem removing rows from images table.";
            db.rollback();
        }
        pauseRefreshing = false;
    }
    else
        return;
}

void DataEntry::on_actionTableview_locally_modified_triggered()
{
    QPointer<TableEditor> editor = new TableEditor("dcterms_modified > '" + dbLastPublished + "'");
    editor->setAttribute(Qt::WA_DeleteOnClose);
    editor->setWindowTitle("Table view - locally modified records");
    editor->show();
}

void DataEntry::on_actionTableview_entire_database_triggered()
{
    QPointer<TableEditor> editor = new TableEditor();
    editor->setAttribute(Qt::WA_DeleteOnClose);
    editor->setWindowTitle("Table view - all records");
    editor->show();
}

void DataEntry::on_coordinateUncertaintyInMetersBox_editTextChanged(const QString &arg1)
{
    QList<QListWidgetItem*> itemList = ui->thumbWidget->selectedItems();

    if (itemList.isEmpty())
        return;

    int h = imageIndexHash.value(itemList.at(0)->text());
    if (h == -1)
    {
        qDebug() << "Error: hashIndex not found when setting unique organism ID.";
        return;
    }

    if (itemList.count() == 1 && images[h].coordinateUncertaintyInMeters == arg1)
        return;

    saveInput("dwc_coordinateUncertaintyInMeters",arg1);
}
