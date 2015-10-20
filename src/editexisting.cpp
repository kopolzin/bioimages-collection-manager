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

#include <QFileDialog>
#include <QFileInfo>
#include <QCompleter>
#include <QShortcut>
#include <QMessageBox>
#include <QSqlQuery>
#include <QSqlError>

#include "newagentdialog.h"
#include "startwindow.h"
#include "editexisting.h"
#include "ui_editexisting.h"

EditExisting::EditExisting(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::EditExisting)
{
    ui->setupUi(this);
    move(QApplication::desktop()->screen()->rect().center() - rect().center());
    QString lastAgent = "";

#ifdef Q_OS_MAC
    this->setStyleSheet("QLabel{font-size: 12px} QCheckBox{font-size: 12px} QComboBox{font-size: 12px} "
                        "QPushButton{font-size:12px; margin-left: -4px; margin-right: -4px; margin-top: -4px; margin-bottom: -6px}");
    ui->backButton->setStyleSheet("font-size: 13px");
    ui->doneButton->setStyleSheet("font-size: 15px");
#endif

    QSqlDatabase db = QSqlDatabase::database();
    db.transaction();

    QSqlQuery qry;
    qry.prepare("SELECT value FROM settings WHERE setting = (?)");
    qry.addBindValue("last.agent");
    qry.exec();
    if (qry.next())
        lastAgent = qry.value(0).toString();

    loadAgents();
    setAgent(lastAgent);

    dbLastPublished = "2015-09-21";
    qry.prepare("SELECT value FROM settings WHERE setting = (?)");
    qry.addBindValue("metadata.version");
    qry.exec();
    if (qry.next())
        dbLastPublished = qry.value(0).toString();

    bool loadHistoricalRecords = false;
    qry.prepare("SELECT value FROM settings WHERE setting = (?)");
    qry.addBindValue("editexisting.load.onlylocallymodified");
    qry.exec();
    if (qry.next())
    {
        if (!qry.value(0).toString().isEmpty())
            loadHistoricalRecords = qry.value(0).toBool();
    }

    photoFolder = "";
    qry.prepare("SELECT value FROM settings WHERE setting = (?)");
    qry.addBindValue("path.photofolder");
    qry.exec();
    if (qry.next())
        photoFolder = qry.value(0).toString();

    bool loadFromRootDir = false;
    qry.prepare("SELECT value FROM settings WHERE setting = (?)");
    qry.addBindValue("editexisting.load.fromrootfolder");
    qry.exec();
    if (qry.next())
    {
        if (!qry.value(0).toString().isEmpty())
            loadFromRootDir = qry.value(0).toBool();
    }

    bool loadAllAgents = false;
    qry.prepare("SELECT value FROM settings WHERE setting = (?)");
    qry.addBindValue("editexisting.load.allagents");
    qry.exec();
    if (qry.next())
    {
        if (!qry.value(0).toString().isEmpty())
            loadAllAgents = qry.value(0).toBool();
    }

    QString loadQuality = "fullQuality";
    qry.prepare("SELECT value FROM settings WHERE setting = (?)");
    qry.addBindValue("editexisting.load.quality");
    qry.exec();
    if (qry.next())
    {
        if (!qry.value(0).toString().isEmpty())
            loadQuality = qry.value(0).toString();
    }

    if (!db.commit())
    {
        qDebug() << "In editExisting(): Problem querying database.";
        db.rollback();
    }

    // Set the previous settings
    pauseLoading = true;
    ui->loadHistoricalRecordsCheckbox->setChecked(loadHistoricalRecords);
    ui->selectedImageFolder->setText(photoFolder);
    ui->rootFolderCheckbox->setChecked(loadFromRootDir);
    ui->loadAllImagesCheckbox->setChecked(loadAllAgents);

    if (loadQuality == "webQuality")
        ui->webQualityButton->setChecked(true);
    else if (loadQuality == "goodQuality")
        ui->webQualityButton->setChecked(true);
    else
        ui->fullQualityButton->setChecked(true);

    // Set enter key to activate push buttons
    ui->selectImageLocationButton->setAutoDefault(true);
    ui->doneButton->setAutoDefault(true);
    ui->backButton->setAutoDefault(true);
    ui->addnewAgentButton->setAutoDefault(true);

    ui->showHideButton->setArrowType(Qt::ArrowType::DownArrow);
    if (loadFromRootDir || loadAllAgents)
        ui->advancedWidget->setVisible(true);
    else
        ui->advancedWidget->setVisible(false);

    ui->webQualityButton->setVisible(false);
    ui->goodQualityButton->setVisible(false);
    ui->fullQualityButton->setVisible(false);
}

void EditExisting::setup()
{
    QFile thumbFile(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/data/thumbnails.dat");
    if (thumbFile.open(QIODevice::ReadOnly))
    {
        ui->currentCacheSizeLabel->setText("Current cache file size: " + QString::number(thumbFile.size()/1048576) + " MB");
        thumbFile.close();
    }

    findFilenamesByAgent();

    QCoreApplication::processEvents();

    if (!fileNames.isEmpty())
        findMatchingFiles();

    QCoreApplication::processEvents();

    pauseLoading = false;
}

EditExisting::~EditExisting()
{
    delete ui;
}

void EditExisting::on_backButton_clicked()
{
    this->hide();
    emit windowClosed();
}

void EditExisting::loadAgents()
{
    agents.clear();
    agentHash.clear();

    QSqlQuery query;
    query.prepare("SELECT dcterms_identifier, dc_contributor, iri, contactURL, morphbankUserID, dcterms_modified, type from agents");
    query.exec();
    while (query.next())
    {
        QString dcterms_identifier = query.value(0).toString();
        QString dc_contributor = query.value(1).toString();
        agents.append(dcterms_identifier);
        agentHash.insert(dcterms_identifier,dc_contributor);
    }
    agents.sort(Qt::CaseInsensitive);
}

void EditExisting::loadDeterminations()
{
    determinations.clear();

    QSqlQuery query;
    query.setForwardOnly(true);
    query.prepare("SELECT dsw_identified, identifiedBy, dwc_dateIdentified, dwc_identificationRemarks, "
                    "tsnID, nameAccordingToID, dcterms_modified, suppress from determinations");
    query.exec();
    while (query.next())
    {
        Determination determination;
        determination.identified = query.value(0).toString();
        determination.identifiedBy = query.value(1).toString();
        determination.dateIdentified = query.value(2).toString();
        determination.identificationRemarks = query.value(3).toString();
        determination.tsnID = query.value(4).toString();
        determination.nameAccordingToID = query.value(5).toString();
        determination.lastModified = query.value(6).toString();
        determination.suppress = query.value(7).toString();

        determinations << determination;
    }
}

void EditExisting::loadImages()
{
    images.clear();

    QString recentlyModified = "";
    QString recentlyModifiedPlusWhere = "";

    if (!ui->loadHistoricalRecordsCheckbox->isChecked())
    {
        recentlyModified = " AND dcterms_modified > '" + dbLastPublished + "'";
        recentlyModifiedPlusWhere = " WHERE dcterms_modified > '" + dbLastPublished + "'";
    }

    QString selection;
    if (ui->loadAllImagesCheckbox->isChecked())
    {
        selection= "SELECT fileName, focalLength, dwc_georeferenceRemarks, dwc_decimalLatitude, "
                            "dwc_decimalLongitude, geo_alt, exif_PixelXDimension, exif_PixelYDimension, "
                            "dwc_occurrenceRemarks, dwc_geodeticDatum, dwc_coordinateUncertaintyInMeters, "
                            "dwc_locality, dwc_countryCode, dwc_stateProvince, dwc_county, dwc_informationWithheld, "
                            "dwc_dataGeneralizations, dwc_continent, geonamesAdmin, geonamesOther, dcterms_identifier, "
                            "dcterms_modified, dcterms_title, dcterms_description, ac_caption, photographerCode, dcterms_created, "
                            "photoshop_Credit, owner, dcterms_dateCopyrighted, dc_rights, xmpRights_Owner, ac_attributionLinkURL, "
                            "ac_hasServiceAccessPoint, usageTermsIndex, view, xmp_Rating, foaf_depicts, "
                            "suppress from images" + recentlyModifiedPlusWhere;
    }
    else
    {
        selection= "SELECT fileName, focalLength, dwc_georeferenceRemarks, dwc_decimalLatitude, "
                        "dwc_decimalLongitude, geo_alt, exif_PixelXDimension, exif_PixelYDimension, "
                        "dwc_occurrenceRemarks, dwc_geodeticDatum, dwc_coordinateUncertaintyInMeters, "
                        "dwc_locality, dwc_countryCode, dwc_stateProvince, dwc_county, dwc_informationWithheld, "
                        "dwc_dataGeneralizations, dwc_continent, geonamesAdmin, geonamesOther, dcterms_identifier, "
                        "dcterms_modified, dcterms_title, dcterms_description, ac_caption, photographerCode, dcterms_created, "
                        "photoshop_Credit, owner, dcterms_dateCopyrighted, dc_rights, xmpRights_Owner, ac_attributionLinkURL, "
                        "ac_hasServiceAccessPoint, usageTermsIndex, view, xmp_Rating, foaf_depicts, "
                        "suppress from images WHERE photographerCode = '" + ui->agentBox->currentText() +"'" + recentlyModified;
    }
    QSqlQuery query;
    query.setForwardOnly(true);
    query.prepare(selection);
    query.exec();

    QCoreApplication::processEvents();
    while (query.next())
    {
        Image image;
        image.fileName = query.value(0).toString();
        image.focalLength = query.value(1).toString();
        image.georeferenceRemarks = query.value(2).toString();
        image.decimalLatitude = query.value(3).toString();
        image.decimalLongitude = query.value(4).toString();
        image.altitudeInMeters = query.value(5).toString();
        image.width = query.value(6).toString();
        image.height = query.value(7).toString();
        image.occurrenceRemarks = query.value(8).toString();
        image.geodeticDatum = query.value(9).toString();
        image.coordinateUncertaintyInMeters = query.value(10).toString();
        image.locality = query.value(11).toString();
        image.countryCode = query.value(12).toString();
        image.stateProvince = query.value(13).toString();
        image.county = query.value(14).toString();
        image.informationWithheld = query.value(15).toString();
        image.dataGeneralizations = query.value(16).toString();
        image.continent = query.value(17).toString();
        image.geonamesAdmin = query.value(18).toString();
        image.geonamesOther = query.value(19).toString();
        image.identifier = query.value(20).toString();
        image.lastModified = query.value(21).toString();
        image.title = query.value(22).toString();
        image.description = query.value(23).toString();
        image.caption = query.value(24).toString();
        image.photographerCode = query.value(25).toString();
        image.dcterms_created = query.value(26).toString();

        QStringList eventDateSplit = image.dcterms_created.split("T");
        if (eventDateSplit.length() == 2)
        {
            image.date = eventDateSplit.at(0);
            QStringList timeSplit;
            if (eventDateSplit.at(1).contains("-"))
            {
                timeSplit = eventDateSplit.at(1).split("-");
                image.time = timeSplit.at(0);
                image.timezone = "-" + timeSplit.at(1);
            }
            else if (eventDateSplit.at(1).contains("+"))
            {
                timeSplit = eventDateSplit.at(1).split("+");
                image.time = timeSplit.at(0);
                image.timezone = "+" + timeSplit.at(1);
            }
            else
                image.time = eventDateSplit.at(1);
        }
        else
            image.date = image.dcterms_created;

        image.credit = query.value(27).toString();
        image.copyrightOwnerID = query.value(28).toString();
        image.copyrightYear = query.value(29).toString();
        image.copyrightStatement = query.value(30).toString();
        image.copyrightOwnerName = query.value(31).toString();
        image.attributionLinkURL = query.value(32).toString();
        image.urlToHighRes = query.value(33).toString();
        image.usageTermsIndex = query.value(34).toString();
        image.imageView = query.value(35).toString();
        image.rating = query.value(36).toString();
        image.depicts = query.value(37).toString();
        image.suppress = query.value(38).toString();

        // Convert imageview #010203 format to group/part/view QStrings
        QString group = "unspecified";
        QString part = "unspecified";
        QString view = "unspecified";
        QString imageView = image.imageView;
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

        image.groupOfSpecimen = group;
        image.portionOfSpecimen = part;
        image.viewOfSpecimen = view;

        images << image;

        QCoreApplication::processEvents();
    }
}

void EditExisting::loadOrganisms()
{
    organisms.clear();

    QSqlQuery query;
    query.prepare("SELECT dcterms_identifier, dwc_establishmentMeans, dcterms_modified, "
                    "dwc_organismRemarks, dwc_collectionCode, dwc_catalogNumber, "
                    "dwc_georeferenceRemarks, dwc_decimalLatitude, dwc_decimalLongitude, geo_alt, "
                    "dwc_organismName, dwc_organismScope, cameo, notes, suppress from organisms");
    query.exec();
    while (query.next())
    {
        Organism organism;
        organism.identifier = query.value(0).toString(); // remove URL from ID http://foo/bar/ID
        organism.establishmentMeans = query.value(1).toString();
        organism.lastModified = query.value(2).toString();
        organism.organismRemarks = query.value(3).toString();
        organism.collectionCode = query.value(4).toString();
        organism.catalogNumber = query.value(5).toString();
        organism.georeferenceRemarks = query.value(6).toString();
        organism.decimalLatitude = query.value(7).toString();
        organism.decimalLongitude = query.value(8).toString();
        organism.altitudeInMeters = query.value(9).toString();
        organism.organismName = query.value(10).toString();
        organism.organismScope = query.value(11).toString();
        organism.cameo = query.value(12).toString();
        organism.notes = query.value(13).toString();
        organism.suppress = query.value(14).toString();

        organisms << organism;
    }
}

void EditExisting::loadSensu()
{
    sensus.clear();

    QSqlQuery query;
    query.setForwardOnly(true);
    query.prepare("SELECT dcterms_identifier, dc_creator, tcsSignature, dcterms_title, "
                    "dc_publisher, dcterms_created, iri, dcterms_modified from sensu");
    query.exec();
    while (query.next())
    {
        Sensu sensu;
        sensu.identifier = query.value(0).toString();
        sensu.creator = query.value(1).toString();
        sensu.tcsSignature = query.value(2).toString();
        sensu.title = query.value(3).toString();
        sensu.publisher = query.value(4).toString();
        sensu.dcterms_created = query.value(5).toString();
        sensu.iri = query.value(6).toString();
        sensu.lastModified = query.value(7).toString();
        sensus << sensu;
    }
}

QString EditExisting::modifiedNow()
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

void EditExisting::setNumFiles()
{
    if (numImagesFound == 1)
    {
        ui->numFilesLabel->setText("1 image found of " + QString::number(numImagesRef) + " referenced");
    }
    else
    {
        ui->numFilesLabel->setText(QString::number(numImagesFound) + " images found of " + QString::number(numImagesRef) + " referenced");
    }
}

void EditExisting::on_selectImageLocationButton_clicked()
{
    QSqlDatabase db = QSqlDatabase::database();
    db.transaction();

    QString appPath = "";
    QString newPhotoFolder = "";

    QSqlQuery qry;
    qry.prepare("SELECT value FROM settings WHERE setting = (?)");
    qry.addBindValue("path.applicationfolder");
    qry.exec();
    if (qry.next())
        appPath = qry.value(0).toString();

    if (!db.commit())
    {
        qDebug() << "Problem committing changes to database in EditExisting::on_selectImageLocationButton_clicked()";
        db.rollback();
    }

    newPhotoFolder = QFileDialog::getExistingDirectory(this,"Select directory containing images",photoFolder,QFileDialog::DontUseNativeDialog);
    if (newPhotoFolder == "")
        return;

    ui->listWidget->clear();

    photoFolder = newPhotoFolder;

    ui->selectedImageFolder->setText(photoFolder);

    qry.prepare("INSERT OR REPLACE INTO settings (setting, value) VALUES (?, ?)");
    qry.addBindValue("path.photofolder");
    qry.addBindValue(photoFolder);
    qry.exec();

    if (!fileNames.isEmpty())
        findMatchingFiles();
    ui->doneButton->setFocus();
}

void EditExisting::findMatchingFiles()
{
    fullPathFileNames.clear();
    numImagesFound = 0;
    ui->listWidget->clear();

//    if (ui->webQualityButton->isChecked())
//        photoFolder = photoFolder + "/lq";
//    else if (ui->goodQualityButton->isChecked())
//        photoFolder = photoFolder + "/gq";

    QCoreApplication::processEvents();
    loadImages();
    QCoreApplication::processEvents();

    for (Image image : images)
    {
        QString file = image.fileName;
        QString folder = photoFolder;

        if (ui->rootFolderCheckbox->isChecked())
        {
            QStringList identifierSplit = image.identifier.split("/");
            QString photographer;
            if (identifierSplit.length() >= 2)
            {
                photographer = identifierSplit.at(identifierSplit.length()-2);
                folder = folder + "/" + photographer;
            }
        }

        QString filepath = folder + "/" + file;

//        QString filepath;
//        if (ui->webQualityButton->isChecked())
//        {
//            image.filename = "w" + image.filename;
//            filepath = folder + "/w" + file;
//        }
//        else if (ui->goodQualityButton->isChecked())
//        {
//            image.filename = "g" + image.filename;
//            filepath = folder + "/g" + file;
//        }
//        else
//            filepath = folder + "/" + file;

        QFileInfo fileInfo(filepath);
        if (!fileInfo.isFile())
        {
            fullPathFileNames.append(filepath);
        }
        else
        {
            numImagesFound++;
            fullPathFileNames.append(filepath);
        }
    }

    ui->listWidget->addItems(fullPathFileNames);
    setNumFiles();
}

void EditExisting::on_doneButton_clicked()
{
    if (ui->selectedImageFolder->text().isEmpty())
    {
        QMessageBox msgBox;
        msgBox.setText("No image folder has been selected. Select one first to proceed.");
        msgBox.exec();
        return;
    }
    else if (ui->listWidget->count() == 0)
    {
        QMessageBox msgBox;
        msgBox.setText("No images have been selected. Select some first to proceed.");
        msgBox.exec();
        return;
    }
    else if (ui->agentBox->currentText().isEmpty())
    {
        QMessageBox msgBox;
        msgBox.setText("Enter your unique agent identifier first.");
        msgBox.exec();
        return;
    }
    else
    {
        QStringList imageFileNames;
        QStringList baseFileNames;
        QHash<QString,QString> baseFullHash;
        for (int i=0; i < ui->listWidget->count(); i++)
        {
            QFileInfo fileInfo(ui->listWidget->item(i)->text());
            QString fullPath = ui->listWidget->item(i)->text();
            imageFileNames.append(fullPath);
            QString base = fileInfo.fileName();
            baseFileNames.append(base);
            baseFullHash.insert(base,fullPath);
        }

        for (Image im : images)
        {
            im.fileAndPath = baseFullHash.value(im.fileName);
            photographerHash.insert(im.fileAndPath, im.photographerCode);
        }

        int duplicates = baseFileNames.removeDuplicates();
        if (duplicates > 0)
        {
            QMessageBox msgBox;
            msgBox.setText("Found duplicates of base filenames.\n"
                           "Ensure filenames are unique then try again.");
            msgBox.exec();
            return;
        }

        QSqlQuery qry;
        qry.prepare("INSERT OR REPLACE INTO settings (setting, value) VALUES (?, ?)");
        qry.addBindValue("last.agent");
        qry.addBindValue(ui->agentBox->currentText());
        qry.exec();

        dataEntry = new DataEntry(imageFileNames, photographerHash, agentHash, images);
        dataEntry->setAttribute(Qt::WA_DeleteOnClose);
        connect(dataEntry,SIGNAL(windowClosed()),this,SLOT(closeDataEntry()));
        this->hide();
        dataEntry->show();
    }
}

void EditExisting::setAgent(QString &agent)
{
    ui->agentBox->addItems(agents);
    ui->agentBox->setCurrentText(agent);
}

void EditExisting::on_addnewAgentButton_clicked()
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

    // Find and set the current time for lastModified
    QString currentDateTime = QDateTime::currentDateTime().toString("yyyy-MM-dd'T'hh:mm:ss");

    QDateTime currentTimeUTC = QDateTime::currentDateTime();
    QDateTime currentTimeLocal = currentTimeUTC;
    currentTimeUTC.setTimeSpec(Qt::UTC);

    int timezoneOffsetInt = currentTimeLocal.secsTo(currentTimeUTC) / 3600;
    QString timezoneOffset = QString::number(timezoneOffsetInt);

    // convert offset to format: +/-hh:mm
    if (timezoneOffset.contains("-"))
    {
        timezoneOffset.remove("-");
        if (timezoneOffset.toInt() < 10)
            timezoneOffset = "0" + timezoneOffset;
        timezoneOffset = "-" + timezoneOffset + ":00";
    }
    else
    {
        if (timezoneOffset.toInt() < 10)
            timezoneOffset = "0" + timezoneOffset;
        timezoneOffset = "+" + timezoneOffset + ":00";
    }

    QString lastModified = currentDateTime + timezoneOffset;

    QString insertion = "INSERT INTO agents (dcterms_identifier, dc_contributor, iri, contactURL, morphbankUserID, dcterms_modified, type) "
            "VALUES ('" + agentCode + "', '" + fullName + "', '" + agentURI + "', '" + contactURL + "', '" + morphbankID + "', '" + lastModified + "', '" + agentType + "')";
    QSqlQuery query;
    query.setForwardOnly(true);
    if (!query.exec(insertion)) {
        QMessageBox::critical(0, "", "Agent insertion failed: " + query.lastError().text());
        return;
    }

    agents.append(agentCode);
    agents.sort(Qt::CaseInsensitive);
    setAgent(agentCode);

    agentHash.insert(agentCode, fullName);

    ui->selectImageLocationButton->setFocus();
}

void EditExisting::on_agentBox_activated(const QString &arg1)
{
    findFilenamesByAgent();
    if (!photoFolder.isEmpty())
        findMatchingFiles();
}

void EditExisting::findFilenamesByAgent()
{
    QString currentAgent = ui->agentBox->currentText();
    fileNames.clear();

    QString recentlyModified = "";
    QString recentlyModifiedPlusWhere = "";

    if (!ui->loadHistoricalRecordsCheckbox->isChecked())
    {
        recentlyModified = " AND dcterms_modified > '" + dbLastPublished + "'";
        recentlyModifiedPlusWhere = " WHERE dcterms_modified > '" + dbLastPublished + "'";
    }

    QString selection;
    if (ui->loadAllImagesCheckbox->isChecked())
        selection = "SELECT filename FROM images" + recentlyModifiedPlusWhere;
    else
        selection = "SELECT filename FROM images WHERE photographerCode = '" + currentAgent +"'" + recentlyModified;
    QSqlQuery query;
    query.prepare(selection);
    query.exec();
    while (query.next())
    {
        fileNames.append(query.value(0).toString());
    }

    numImagesRef = fileNames.count();
    setNumFiles();
}

void EditExisting::on_loadAllImagesCheckbox_toggled(bool loadEveryonesChecked)
{
    if (pauseLoading)
        return;
    fileNames.clear();

    QString recentlyModified = "";
    QString recentlyModifiedPlusWhere = "";

    if (!ui->loadHistoricalRecordsCheckbox->isChecked())
    {
        recentlyModified = " AND dcterms_modified > '" + dbLastPublished + "'";
        recentlyModifiedPlusWhere = " WHERE dcterms_modified > '" + dbLastPublished + "'";
    }

    QString selection;
    if (loadEveryonesChecked)
        selection = "SELECT filename FROM images" + recentlyModifiedPlusWhere;
    else
        selection = "SELECT filename FROM images WHERE photographerCode = '" + ui->agentBox->currentText() +"'" + recentlyModified;
    QSqlQuery query;
    query.setForwardOnly(true);
    query.prepare(selection);
    query.exec();
    while (query.next())
    {
        fileNames.append(query.value(0).toString());
    }

    numImagesRef = fileNames.count();
    setNumFiles();

    ui->selectImageLocationButton->setFocus();

    query.prepare("INSERT OR REPLACE INTO settings (setting, value) VALUES (?, ?)");
    query.addBindValue("editexisting.load.allagents");
    query.addBindValue(loadEveryonesChecked);
    query.exec();

    if (!photoFolder.isEmpty())
        findMatchingFiles();
}

void EditExisting::on_loadHistoricalRecordsCheckbox_toggled(bool loadAllHistoricalChecked)
{
    if (pauseLoading)
        return;
    fileNames.clear();

    QString recentlyModified = "";
    QString recentlyModifiedPlusWhere = "";

    if (!loadAllHistoricalChecked)
    {
        recentlyModified = " AND dcterms_modified > '" + dbLastPublished + "'";
        recentlyModifiedPlusWhere = " WHERE dcterms_modified > '" + dbLastPublished + "'";
    }

    QString selection;
    if (ui->loadAllImagesCheckbox->isChecked())
        selection = "SELECT filename FROM images" + recentlyModifiedPlusWhere;
    else
        selection = "SELECT filename FROM images WHERE photographerCode = '" + ui->agentBox->currentText() +"'" + recentlyModified;
    QSqlQuery query;
    query.setForwardOnly(true);
    query.prepare(selection);
    query.exec();
    while (query.next())
    {
        fileNames.append(query.value(0).toString());
    }

    numImagesRef = fileNames.count();
    setNumFiles();

    ui->selectImageLocationButton->setFocus();

    query.prepare("INSERT OR REPLACE INTO settings (setting, value) VALUES (?, ?)");
    query.addBindValue("editexisting.load.onlylocallymodified");
    query.addBindValue(loadAllHistoricalChecked);
    query.exec();

    if (!photoFolder.isEmpty())
    {
        findMatchingFiles();
    }
}

void EditExisting::closeDataEntry()
{
    emit windowClosed();
}

void EditExisting::on_showHideButton_clicked()
{
    if (ui->advancedWidget->isHidden())
    {
        ui->advancedWidget->setVisible(true);
        ui->showHideButton->setArrowType(Qt::ArrowType::UpArrow);
    }
    else
    {
        ui->advancedWidget->setVisible(false);
        ui->showHideButton->setArrowType(Qt::ArrowType::DownArrow);
    }
}

void EditExisting::on_rootFolderCheckbox_toggled(bool checked)
{
    if (pauseLoading)
        return;

    QSqlQuery query;
    query.prepare("INSERT OR REPLACE INTO settings (setting, value) VALUES (?, ?)");
    query.addBindValue("editexisting.load.fromrootfolder");
    query.addBindValue(checked);
    query.exec();

    if (!photoFolder.isEmpty())
        findMatchingFiles();
}

void EditExisting::on_clearCache_clicked()
{
    if (!ui->confirmCacheClear->isChecked())
    {
        QMessageBox msgBox;
        msgBox.setText("Please check the box first to confirm that you wish to clear the thumbnail cache.");
        msgBox.exec();
        return;
    }

    // delete it
    QString cachePath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/data/thumbnails.dat";
    if (QFile::exists(cachePath))
        QFile::remove(cachePath);
    ui->currentCacheSizeLabel->setText("Current cache file size: 0 MB");
    ui->confirmCacheClear->setChecked(false);

}
