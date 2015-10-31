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

#include <QMessageBox>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QSqlError>
#include <QStandardPaths>
#include <QFileInfo>
#include <QDomElement>
#include <QDomDocument>

#include "startwindow.h"
#include "ui_startwindow.h"
#include "tableeditor.h"
#include "importcsv.h"

StartWindow::StartWindow(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::StartWindow)
{
    ui->setupUi(this);
    screenPosLoaded = false;
    move(QApplication::desktop()->screen()->rect().center() - rect().center());

    qApp->font().setFamily("Verdana");
    QFontMetrics fmLabel(ui->bioimagesLabel->font());
    this->setMinimumWidth(fmLabel.width(ui->bioimagesLabel->text()) + 20);
    QFontMetrics fmButton(ui->addNewImagesButton->font());
    this->setMinimumHeight(fmButton.width("New images")*2.6);

#ifdef Q_OS_MAC
    this->setStyleSheet("QPushButton{border: 1px solid black; background-color: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,stop: 0 #f6f7fa, stop: 1 #dadbde);} "
                        "QPushButton:pressed {background-color: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,stop: 0 #dadbde, stop: 1 #f6f7fa);}");
    QFont font(ui->updatesAvailable->font());
    font.setPointSize(12);
    ui->updatesAvailable->setFont(font);
#endif

    ui->bioimagesLogo->setPixmap(QPixmap(":/icon.png"));
    ui->bioimagesLogo->setScaledContents(true);

    ui->updatesAvailable->setAutoFillBackground(false);
    ui->updatesAvailable->setHidden(true);
    ui->updatesAvailable->setStyleSheet("QPushButton {color: yellow; background-color: black} "
                                        "QPushButton:pressed {background-color: yellow; color:black}");

    ui->addNewImagesButton->setAutoDefault(true);
    ui->editExistingRecordsButton->setAutoDefault(true);
    ui->HelpButton->setAutoDefault(true);
    ui->manageCSVsButton->setAutoDefault(true);
    ui->generateWebsiteButton->setAutoDefault(true);

    this->show();
    this->activateWindow();
    this->raise();
    setupDB();
    QCoreApplication::processEvents();

    QSqlDatabase db = QSqlDatabase::database();
    db.transaction();

    QSqlQuery qry;
    qry.prepare("SELECT value FROM settings WHERE setting = (?)");
    qry.addBindValue("view.startscreen.location");
    qry.exec();
    if (qry.next())
        restoreGeometry(qry.value(0).toByteArray());

    bool wasMaximized = false;
    qry.prepare("SELECT value FROM settings WHERE setting = (?)");
    qry.addBindValue("view.startscreen.fullscreen");
    qry.exec();
    if (qry.next())
        wasMaximized = qry.value(0).toBool();

    if (wasMaximized)
        this->showMaximized();
    screenPosLoaded = true;

    QSqlQuery findLastCSVCheck;
    findLastCSVCheck.prepare("SELECT value FROM settings WHERE setting = (?)");
    findLastCSVCheck.addBindValue("metadata.lastcheck");
    findLastCSVCheck.exec();

    QString lastCSVCheck;
    if (findLastCSVCheck.next())
    {
        lastCSVCheck = findLastCSVCheck.value(0).toString();
        if (!lastCSVCheck.isEmpty())
            qDebug() << "Last CSV check was on " + lastCSVCheck;
    }

    findLastCSVCheck.prepare("SELECT value FROM settings WHERE setting = (?)");
    findLastCSVCheck.addBindValue("metadata.updateavailable");
    findLastCSVCheck.exec();

    if (findLastCSVCheck.next())
    {
        if (findLastCSVCheck.value(0).toBool())
            ui->updatesAvailable->setHidden(false);
    }

    if (!db.commit())
    {
        qDebug() << "In startWindow(): Problem querying database.";
        db.rollback();
    }

    // get first 16 characters of csvVersion and convert to date...
    if (lastCSVCheck.isEmpty() || QDateTime::fromString(lastCSVCheck.left(16),"yyyy-MM-dd'T'hh:mm").addDays(1) < QDateTime::currentDateTime())
    {
        qDebug() << "We have go-ahead. Time to download new CSVs.";
        checkCSVUpdate();
    }
}

StartWindow::~StartWindow()
{
    delete ui;
}

void StartWindow::resizeEvent(QResizeEvent *)
{
    int buttonW = ui->bioimagesLogo->size().height();
    int screenW = this->width();
    if (screenW < 50)
        return;
    if (buttonW > (screenW/3 - 10))
        buttonW = screenW/3 - 10;

    ui->addNewImagesButton->setFixedWidth(buttonW);
    ui->editExistingRecordsButton->setFixedWidth(buttonW);
    ui->HelpButton->setFixedWidth(buttonW);
    ui->bioimagesLogo->setFixedWidth(buttonW);
    ui->manageCSVsButton->setFixedWidth(buttonW);
    ui->generateWebsiteButton->setFixedWidth(buttonW);
    if (!screenPosLoaded)
        return;

    QSqlQuery qry;
    qry.prepare("INSERT OR REPLACE INTO settings (setting, value) VALUES (?, ?)");
    qry.addBindValue("view.startscreen.location");
    qry.addBindValue(saveGeometry());
    qry.exec();
}

void StartWindow::moveEvent(QMoveEvent *)
{
    if (!screenPosLoaded)
        return;
    QSqlQuery qry;
    qry.prepare("INSERT OR REPLACE INTO settings (setting, value) VALUES (?, ?)");
    qry.addBindValue("view.startscreen.location");
    qry.addBindValue(saveGeometry());
    qry.exec();
}

void StartWindow::changeEvent(QEvent* event)
{
    if (!screenPosLoaded)
        return;
    if (isHidden())
        return;
    if (event->type() == QEvent::WindowStateChange) {
        bool isMax = false;
        if (windowState() == Qt::WindowMaximized)
            isMax = true;

        QSqlQuery qry;
        qry.prepare("INSERT OR REPLACE INTO settings (setting, value) VALUES (?, ?)");
        qry.addBindValue("view.startscreen.fullscreen");
        qry.addBindValue(isMax);
        qry.exec();

        int buttonW = ui->bioimagesLogo->size().height();
        int screenW = this->width();
        if (screenW < 50)
            return;
        if (buttonW > (screenW/3 - 10))
            buttonW = screenW/3 - 10;

        ui->addNewImagesButton->setFixedWidth(buttonW);
        ui->editExistingRecordsButton->setFixedWidth(buttonW);
        ui->HelpButton->setFixedWidth(buttonW);
        ui->bioimagesLogo->setFixedWidth(buttonW);
        ui->manageCSVsButton->setFixedWidth(buttonW);
        ui->generateWebsiteButton->setFixedWidth(buttonW);
    }
}

void StartWindow::on_addNewImagesButton_clicked()
{
    // switch to Process New Images window
    processNewImagesWindow = new ProcessNewImages();
    connect(processNewImagesWindow,SIGNAL(windowClosed()),this,SLOT(closeAddNew()));
    this->hide();
    processNewImagesWindow->show();
}

void StartWindow::on_editExistingRecordsButton_clicked()
{
    // switch to Edit Existing CSV window
    editExistingWindow = new EditExisting();
    connect(editExistingWindow,SIGNAL(windowClosed()),this,SLOT(closeEditExisting()));
    this->hide();
    editExistingWindow->show();
    editExistingWindow->setup();
}

void StartWindow::on_HelpButton_clicked()
{
    // switch to Help window
    if (help_w.isNull())
    {
        help_w = new Help();
        help_w->setAttribute(Qt::WA_DeleteOnClose);
        help_w->show();
        help_w->select("Start Screen");
    }
    else
    {
        help_w->show();
        help_w->activateWindow();
        help_w->raise();
    }
}

void StartWindow::on_manageCSVsButton_clicked()
{
    manageCSVs = new ManageCSVs();
    connect(manageCSVs,SIGNAL(windowClosed()),this,SLOT(closeManageCSVs()));
    this->hide();
    manageCSVs->show();
}

void StartWindow::on_generateWebsiteButton_clicked()
{
    generateWebsite = new GenerateWebsite();
    connect(generateWebsite,SIGNAL(windowClosed()),this,SLOT(closeGenerateWebsite()));
    this->hide();
    generateWebsite->show();
}

void StartWindow::resetEditExistingButton()
{
    ui->editExistingRecordsButton->setText("View/Edit\nExisting\nRecords");
}

void StartWindow::resetManageCSVsButton()
{
    ui->manageCSVsButton->setText("Merge\nRecord\nFiles");
}

void StartWindow::resetGenerateWebsiteButton()
{
    ui->generateWebsiteButton->setText("Generate\nBioimages\nWebsite");
}

void StartWindow::closeAddNew()
{
    processNewImagesWindow->deleteLater();
    this->show();
    this->activateWindow();
    this->raise();
}

void StartWindow::closeEditExisting()
{
    editExistingWindow->deleteLater();
    this->show();
    this->activateWindow();
    this->raise();
}

void StartWindow::closeManageCSVs()
{
    manageCSVs->deleteLater();
    this->show();
    this->activateWindow();
    this->raise();
}

void StartWindow::closeGenerateWebsite()
{
    generateWebsite->deleteLater();
    this->show();
    this->activateWindow();
    this->raise();
}

void StartWindow::setupDB()
{
    // use QStandardPaths::AppDataLocation for Windows, for *NIX use applicationDirPath
    // check for OS first
    QString dbPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/data";
#ifdef Q_OS_WIN
    dbPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/data";
#elif defined(Q_OS_MAC)
    QDir osxpath = QApplication::applicationDirPath();
    osxpath.cdUp();
    osxpath.cdUp();
    osxpath.cdUp();
    dbPath = osxpath.path() + "/data";
#else
    dbPath = QApplication::applicationDirPath() + "/data";
#endif

    QDir dir(dbPath);
    if (!dir.exists())
        dir.mkpath(dbPath);

    // use local.db; if it doesn't exist copy bioimages.db to local.db
    // if bioimages.db doesn't exist either... download CSV files from GitHub
    // and use hardcoded program defaults (which a table will be created from)?
    QString localdbFile = dbPath + "/local-bioimages.db";
    QString dbFile = dbPath + "/bioimages.db";
    if (!QFileInfo::exists(localdbFile))
    {
        if (QFileInfo::exists(dbFile))
        {
            // move dbFile to localdbFile
            QFile::rename(dbFile, localdbFile);
        }
        else
        {
            // there are no database files at all!
            // try to look for CSV files use hardcoded settings to create a new db
            QMessageBox::critical(0, tr("Cannot open database"),
                tr("Unable to open database at 'data/local-bioimages.db'"), QMessageBox::Cancel);
            return;
        }
    }
    else
    {
        QFile localdb(localdbFile);
        if (localdb.size() == 0)
        {
            if (QFileInfo::exists(dbFile))
            {
                // move dbFile to localdbFile
                localdb.close();
                QFile::rename(dbFile, localdbFile);
            }
            else
            {
                // there are no database files at all!
                qDebug() << "local-bioimages.db is empty and there is no bioimages.db in " + dbPath;
                // try to look for CSV files use hardcoded settings to create a new db
            }
        }
    }

    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(localdbFile);
    if (!db.open()) {
        QMessageBox::critical(0, tr("Cannot open database"),
            tr("Unable to establish a database connection to data/local-bioimages.db."), QMessageBox::Cancel);
        return;
    }

    db.transaction();
    QSqlQuery versionQry;
    versionQry.prepare("SELECT value FROM settings WHERE setting = (?)");
    versionQry.addBindValue("metadata.version");
    versionQry.exec();
    if (versionQry.next())
        databaseVersion = versionQry.value(0).toString();

    QSqlQuery qry;
    qry.prepare("INSERT OR REPLACE INTO settings (setting, value) VALUES (?, ?)");
    qry.addBindValue("path.applicationfolder");
    qry.addBindValue(QApplication::applicationDirPath());
    qry.exec();

    if (!db.commit())
    {
        qDebug() << __LINE__ << "Problem with database transaction";
        db.rollback();
    }

    // if bioimages.db still exists we need to merge its contents with local-bioimages.db
    if (QFileInfo::exists(dbFile))
    {
        qDebug() << "bioimages.db still exists. we need to merge its records with local-bioimages.db";
        QPointer<QMessageBox> updatingMsg = new QMessageBox;
        updatingMsg->setAttribute(Qt::WA_DeleteOnClose);
        for (auto child : updatingMsg->findChildren<QDialogButtonBox *>())
            child->hide();
        updatingMsg->setWindowFlags(Qt::CustomizeWindowHint | Qt::WindowTitleHint);
        updatingMsg->setText("The software has been updated. Please wait while maintenance is performed on the database.");
        updatingMsg->setModal(true);
        updatingMsg->show();
        QCoreApplication::processEvents();

        // clear any "tmp_" tables in local-bioimages.db
        QStringList dropTables;
        dropTables << "tmp_agents" << "tmp_determinations" << "tmp_images" << "tmp_organisms" << "tmp_sensu" << "tmp_taxa";

        db.transaction();
        for (auto t : dropTables)
        {
            QSqlQuery dropQry;
            dropQry.prepare("DELETE FROM " + t);
            dropQry.exec();
        }
        if (!db.commit())
        {
            qDebug() << __LINE__ << "Problem with database transaction";
            db.rollback();
        }

        // load mergedb tables into "tmp_" tables in local-bioimages.db
        QSqlQuery attachQry;
        attachQry.prepare("ATTACH '" + dbFile + "' AS newDB");
        attachQry.exec();

        QStringList insertQuery;
        insertQuery << "tmp_agents SELECT * FROM newDB.agents" << "tmp_determinations SELECT * FROM newDB.determinations" <<
                       "tmp_images SELECT * FROM newDB.images" << "tmp_organisms SELECT * FROM newDB.organisms" <<
                       "tmp_sensu SELECT * FROM newDB.sensu" << "tmp_taxa SELECT * FROM newDB.taxa";
        for (auto t : insertQuery)
        {
            QSqlQuery renameQry;
            renameQry.prepare("INSERT INTO " + t);
            renameQry.exec();
            QCoreApplication::processEvents();
        }

        QSqlQuery detachQry;
        detachQry.prepare("DETACH newDB");
        detachQry.exec();

        db.close();
        db.open();

        QFile dbf(dbFile);
        if (!dbf.remove())
            qDebug() << "Problem deleting " + dbFile;

        // call mergetables and merge
        QPointer<MergeTables> newVersionMerge = new MergeTables();
        connect(newVersionMerge,SIGNAL(loaded()),updatingMsg,SLOT(close()));
        newVersionMerge->setAttribute(Qt::WA_DeleteOnClose);
        QCoreApplication::processEvents();
        newVersionMerge->displayChoices();
    }

    // if the database didn't exist or is empty, import data from CSV files

//    QSqlQuery query;

//    query.exec("CREATE TABLE agents (dcterms_identifier TEXT primary key, "
//        "dc_contributor TEXT, iri TEXT, contactURL TEXT, morphbankUserID TEXT, "
//        "dcterms_modified TEXT, type TEXT)");

//    query.exec("CREATE TABLE determinations (dsw_identified TEXT, "
//               "identifiedBy TEXT, dwc_dateIdentified TEXT, dwc_identificationRemarks TEXT, "
//               "tsnID TEXT, nameAccordingToID TEXT, dcterms_modified TEXT, suppress TEXT, "
//               "PRIMARY KEY(dsw_identified, dwc_dateIdentified, tsnID, nameAccordingToID))");

//    query.exec("CREATE TABLE geonames_admin (dwc_county TEXT NOT NULL, dcterms_identifier TEXT primary key)");

//    query.exec("CREATE TABLE images (fileName TEXT, focalLength TEXT, dwc_georeferenceRemarks TEXT, "
//               "dwc_decimalLatitude TEXT, dwc_decimalLongitude TEXT, geo_alt TEXT, exif_PixelXDimension TEXT, "
//               "exif_PixelYDimension TEXT, dwc_occurrenceRemarks TEXT, dwc_geodeticDatum TEXT, "
//               "dwc_coordinateUncertaintyInMeters TEXT, dwc_locality TEXT, dwc_countryCode TEXT, "
//               "dwc_stateProvince TEXT, dwc_county TEXT, dwc_informationWithheld TEXT, dwc_dataGeneralizations TEXT, "
//               "dwc_continent TEXT, geonamesAdmin TEXT, geonamesOther TEXT, dcterms_identifier TEXT primary key, "
//               "dcterms_modified TEXT, dcterms_title TEXT, dcterms_description TEXT, ac_caption TEXT, "
//               "photographerCode TEXT, dcterms_created TEXT, photoshop_Credit TEXT, owner TEXT, "
//               "dcterms_dateCopyrighted TEXT, dc_rights TEXT, xmpRights_Owner TEXT, ac_attributionLinkURL TEXT, "
//               "ac_hasServiceAccessPoint TEXT, usageTermsIndex TEXT, view TEXT, xmp_Rating TEXT, foaf_depicts TEXT, "
//               "suppress TEXT)");

//    query.exec("CREATE TABLE names (ubioID TEXT, dcterms_identifier TEXT primary key, dwc_kingdom TEXT, "
//               "dwc_class TEXT, dwc_order TEXT, dwc_family TEXT, dwc_genus TEXT, dwc_specificEpithet TEXT, "
//               "dwc_infraspecificEpithet TEXT, dwc_taxonRank TEXT, dwc_scientificNameAuthorship TEXT, "
//               "dwc_vernacularName TEXT, dcterms_modified TEXT)");

//    query.exec("CREATE TABLE organisms (dcterms_identifier TEXT primary key, dwc_establishmentMeans TEXT, "
//               "dcterms_modified TEXT, dwc_organismRemarks TEXT, dwc_collectionCode TEXT, dwc_catalogNumber TEXT, "
//               "dwc_georeferenceRemarks TEXT, dwc_decimalLatitude TEXT, dwc_decimalLongitude TEXT, geo_alt TEXT, "
//               "dwc_organismName TEXT, dwc_organismScope TEXT, cameo TEXT, notes TEXT, suppress TEXT)");

//    query.exec("CREATE TABLE plants (ubioID TEXT, dcterms_identifier TEXT primary key, dwc_kingdom TEXT, "
//               "dwc_class TEXT, dwc_order TEXT, dwc_family TEXT, dwc_genus TEXT, dwc_specificEpithet TEXT, "
//               "dwc_infraspecificEpithet TEXT, dwc_taxonRank TEXT, dwc_scientificNameAuthorship TEXT, "
//               "dwc_vernacularName TEXT, dcterms_modified TEXT)");

//    query.exec("CREATE TABLE sensu (dcterms_identifier TEXT primary key, dc_creator TEXT, tcsSignature TEXT, "
//               "dcterms_title TEXT, dc_publisher TEXT, dcterms_created TEXT, iri TEXT, dcterms_modified TEXT)");

}

void StartWindow::checkCSVUpdate()
{
    fetchList << QUrl("https://raw.githubusercontent.com/baskaufs/Bioimages/master/last-published.xml");
    if (fetchList.isEmpty())
        return;

    fetchThis();
}

void StartWindow::fetchThis()
{
    if (fetchList.isEmpty())
        return;

    QUrl fetchThis = fetchList.first();
    fetchList.removeFirst();
    qDebug() << "Making a request to: " + fetchThis.toString();
    startRequest(fetchThis);
}

void StartWindow::startRequest(QUrl url)
{
    reply = qnam.get(QNetworkRequest(url));
    connect( reply, SIGNAL(finished()), this, SLOT(httpFinished()) );
}

void StartWindow::httpFinished()
{
    // use QStandardPaths::AppDataLocation for Windows, for *NIX use appPath
    // check for OS first
    QString dlPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/data/CSVs";
    QDir dir(dlPath);
    if (!dir.exists())
        dir.mkpath(dlPath);

    QVariant redirectionTarget = reply->attribute(QNetworkRequest::RedirectionTargetAttribute);
    if (reply->error())
    {
        qDebug() << "Fetching latest CSVs from GitHub failed: " + reply->errorString();
    }
    else if (!redirectionTarget.isNull()) {
        QUrl newUrl = url.resolved(redirectionTarget.toUrl());
        qDebug() << "Redirecting CSV download request to: " + newUrl.toString();

        url = newUrl;
        reply->deleteLater();
        startRequest(url);
        return;
    }
    else
    {
        // check reply->url().toString() to see which response we're dealing with
        QString loc =  reply->url().toString();
        QString responseFrom;
        if (loc == "https://raw.githubusercontent.com/baskaufs/Bioimages/master/last-published.xml")
        {
            responseFrom = "GitHub version XML";
            // there are 3 versions we want to know about:
            // #1 - what version of CSVs is the database based on (see dbLastPublished)
            // ---- this version should be stored in the database itself, not just dbLastPublished variable
            // #2 - what's the latest version of CSVs on GitHub (download last-published.xml and find out)
            // #3 - what's the latest version of CSVs stored in /AppData (there might not be any version)

            // 1. make sure the version in last-published.xml is a new version (compare to local last-published.xml), otherwise abort
            // 2. overwrite local last-published with the downloaded one
            // 3. delete local CSVs
            // 4. download latest CSVs
            // 5. prompt to merge them into the database
            QDomDocument xmlDoc;
            QDomElement docElem;
            QString xmlText = reply->readAll(); // we have #2, just parse it

            xmlDoc.setContent(xmlText);
            docElem = xmlDoc.documentElement();

            QDomNode node = docElem.firstChild();
            while (!node.isNull())
            {
                if (node.nodeName() == "dcterms:modified")
                {
                    lastPublished = node.toElement().text();
                    break;
                }
                node = node.nextSibling();
            }

            QSqlQuery setLastCSVCheck;
            setLastCSVCheck.prepare("INSERT OR REPLACE INTO settings (setting, value) VALUES (?, ?)");
            setLastCSVCheck.addBindValue("metadata.lastcheck");
            setLastCSVCheck.addBindValue(QDateTime::currentDateTime().toString("yyyy-MM-dd'T'hh:mm"));
            setLastCSVCheck.exec();

            // now get the time of the local CSVs (if any)
            QString downloadedVersion; // stores dcterms_modified of locally downloaded CSVs
            QFile downloadedVersionFile(dlPath + "/last-downloaded.xml");
            if (downloadedVersionFile.open(QIODevice::ReadOnly | QIODevice::Text))
            {
                xmlDoc.setContent(&downloadedVersionFile);
                docElem = xmlDoc.documentElement();

                QDomNode node = docElem.firstChild();
                while (!node.isNull())
                {
                    if (node.nodeName() == "dcterms:modified")
                    {
                        downloadedVersion = node.toElement().text();
                        break;
                    }
                    node = node.nextSibling();
                }
                downloadedVersionFile.close();
            }
            else
            {
                if (downloadedVersionFile.exists())
                    qDebug() << "Could not open " + dlPath + "/last-downloaded.xml";
            }

            QStringList csvs;
            csvs << "agents.csv" << "determinations.csv" << "images.csv" << "names.csv" << "organisms.csv" << "sensu.csv";
            bool allCSVsPresent = true;
            for (auto csv : csvs)
            {
                if (!QFile::exists(dlPath + "/" + csv))
                {
                    allCSVsPresent = false;
                    break;
                }
            }

            if (lastPublished.isEmpty())
            {
                // there was a problem parsing the downloaded latest-modified.xml; handle that problem here
                qDebug() << "Problem parsing latest-modified.xml from GitHub";
            }
            else if (lastPublished <= databaseVersion)
            {
                qDebug() << "Not fetching files from GitHub. Database is up to date.";
            }
            else if (lastPublished <= downloadedVersion && !downloadedVersion.isEmpty() && allCSVsPresent)
            {
                qDebug() << "Not fetching files from GitHub. Local CSVs are already up to date.";
            }
            else
            {
                if (downloadedVersionFile.open(QIODevice::WriteOnly | QIODevice::Text))
                {
                    QTextStream xmlOut(&downloadedVersionFile);
                    xmlOut.setCodec("UTF-8");
                    xmlOut << xmlText;
                    downloadedVersionFile.close();
                }

                // delete local CSVs before downloading new versions
                for (auto csv : csvs)
                    QFile::remove(dlPath + "/" + csv);

                QString agentsURL = "https://raw.githubusercontent.com/baskaufs/Bioimages/master/agents.csv";
                QString determURL = "https://raw.githubusercontent.com/baskaufs/Bioimages/master/determinations.csv";
                QString imagesURL = "https://raw.githubusercontent.com/baskaufs/Bioimages/master/images.csv";
                QString namesURL  = "https://raw.githubusercontent.com/baskaufs/Bioimages/master/names.csv";
                QString organsURL = "https://raw.githubusercontent.com/baskaufs/Bioimages/master/organisms.csv";
                QString sensuURL  = "https://raw.githubusercontent.com/baskaufs/Bioimages/master/sensu.csv";

                fetchList << agentsURL << determURL << imagesURL << namesURL << organsURL << sensuURL;
                extractCSVs = true;
            }
        }
        else if (loc == "https://raw.githubusercontent.com/baskaufs/Bioimages/master/agents.csv")
        {
            responseFrom = "GitHub agents CSV";
            QFile agentsCSV(dlPath + "/agents.csv");
            if (agentsCSV.open(QIODevice::WriteOnly | QIODevice::Text))
            {
                agentsCSV.write(reply->readAll());
                agentsCSV.close();
            }
        }
        else if (loc == "https://raw.githubusercontent.com/baskaufs/Bioimages/master/determinations.csv")
        {
            responseFrom = "GitHub determinations CSV";
            QFile determsCSV(dlPath + "/determinations.csv");
            if (determsCSV.open(QIODevice::WriteOnly | QIODevice::Text))
            {
                determsCSV.write(reply->readAll());
                determsCSV.close();
            }
        }
        else if (loc == "https://raw.githubusercontent.com/baskaufs/Bioimages/master/images.csv")
        {
            responseFrom = "GitHub images CSV";
            QFile imagesCSV(dlPath + "/images.csv");
            if (imagesCSV.open(QIODevice::WriteOnly | QIODevice::Text))
            {
                imagesCSV.write(reply->readAll());
                imagesCSV.close();
            }
        }
        else if (loc == "https://raw.githubusercontent.com/baskaufs/Bioimages/master/names.csv")
        {
            responseFrom = "GitHub names CSV";
            QFile namesCSV(dlPath + "/names.csv");
            if (namesCSV.open(QIODevice::WriteOnly | QIODevice::Text))
            {
                namesCSV.write(reply->readAll());
                namesCSV.close();
            }
        }
        else if (loc == "https://raw.githubusercontent.com/baskaufs/Bioimages/master/organisms.csv")
        {
            responseFrom = "GitHub organisms CSV";
            QFile organsCSV(dlPath + "/organisms.csv");
            if (organsCSV.open(QIODevice::WriteOnly | QIODevice::Text))
            {
                organsCSV.write(reply->readAll());
                organsCSV.close();
            }
        }
        else if (loc == "https://raw.githubusercontent.com/baskaufs/Bioimages/master/plants-usda-ubio.csv")
        {
            responseFrom = "GitHub plants-usda-ubio CSV";
            QFile organsCSV(dlPath + "/plants-usda-ubio.csv");
            if (organsCSV.open(QIODevice::WriteOnly | QIODevice::Text))
            {
                organsCSV.write(reply->readAll());
                organsCSV.close();
            }
        }
        else if (loc == "https://raw.githubusercontent.com/baskaufs/Bioimages/master/sensu.csv")
        {
            responseFrom = "GitHub sensu CSV";
            QFile sensuCSV(dlPath + "/sensu.csv");
            if (sensuCSV.open(QIODevice::WriteOnly | QIODevice::Text))
            {
                sensuCSV.write(reply->readAll());
                sensuCSV.close();
            }
        }
        else
        {
            responseFrom = "Not handled: " + loc;
        }

        QString replyText = reply->readAll();
        if (!replyText.isEmpty())
            qDebug() << replyText;

        QStringList csvs;
        csvs << "agents" << "determinations" << "images" << "names" << "organisms" << "sensu";
        bool allCSVsPresent = true;
        for (auto csv : csvs)
        {
            if (!QFile::exists(dlPath + "/" + csv + ".csv"))
            {
                allCSVsPresent = false;
                break;
            }
        }

        if (allCSVsPresent)
        {
            allCSVsPresent = false;
            reply->deleteLater();
            reply = 0;

            ui->updatesAvailable->setHidden(false);
            QSqlQuery setAvailable;
            setAvailable.prepare("INSERT OR REPLACE INTO settings (setting, value) VALUES (?, ?)");
            setAvailable.addBindValue("metadata.updateavailable");
            setAvailable.addBindValue(true);
            setAvailable.exec();
            return;
        }
    }

    reply->deleteLater();
    reply = 0;
    fetchThis();
}

bool StartWindow::loadCSVs(const QString folder)
{
    QStringList csvs;
    csvs << "agents" << "determinations" << "images" << "names" << "organisms" << "sensu";
    for (auto csv : csvs)
    {
        if (!QFile::exists(folder + "/" + csv + ".csv"))
        {
            qDebug() << "Not all CSVs present in folder " + folder;
            return false;
        }
    }

    QSqlDatabase db = QSqlDatabase::database();

    for (auto csv : csvs)
    {
        QString table = csv;
        if (table == "names")
            table = "taxa";
        QFile file(folder + "/" + csv + ".csv");
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            qDebug() << "Problem opening file " + folder + "/" + csv + ".csv";
            return false;
        }

        // read line by line and store everything except the header into the appropriate table ("tmp_" + csv)
        // drop "tmp_" + csv table if it exists

        db.transaction();
        if (db.tables().contains("tmp_" + table))
        {
            QSqlQuery dropQry;
            dropQry.prepare("DELETE FROM tmp_" + table);
            dropQry.exec();
        }
        // also clear the "pub_" tables
        if (db.tables().contains("pub_" + table))
        {
            QSqlQuery dropQry;
            dropQry.prepare("DELETE FROM pub_" + table);
            dropQry.exec();
        }
        if (!db.commit())
        {
            qDebug() << __LINE__ << "Problem with database transaction: " << db.lastError();
            db.rollback();
        }

        ImportCSV importCSV;
        if (csv == "agents")
            importCSV.extractAgents(folder + "/agents.csv", "tmp_agents");
        else if (csv == "determinations")
            importCSV.extractDeterminations(folder + "/determinations.csv", "tmp_determinations");
        else if (csv == "images")
            importCSV.extractImageNames(folder + "/images.csv", "tmp_images");
        else if (csv == "organisms")
            importCSV.extractOrganisms(folder + "/organisms.csv", "tmp_organisms");
        else if (csv == "names")
            importCSV.extractTaxa(folder + "/names.csv", "tmp_taxa");
        // adding plants-usda-ubio is not implemented yet
        //else if (csv == "plants-usda-ubio")
        //    importCSV.extractPlants(folder, "tmp_plants_usda_ubio");
        else if (csv == "sensu")
            importCSV.extractSensu(folder + "/sensu.csv", "tmp_sensu");

        file.close();

        QSqlQuery copyQry;
        copyQry.prepare("INSERT INTO pub_" + table + " SELECT * FROM tmp_" + table);
        copyQry.exec();
    }

    qDebug() << "Finished loading CSV data into tablebase.";
    return true;
}

QString StartWindow::modifiedNow()
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

void StartWindow::on_updatesAvailable_clicked()
{
    if (loadCSVs(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/data/CSVs"))
    {
        // compare the tmp_ tables with the originals
        // prompt user for their choices of which conflicting rows to keep
        // cleanup by using "delete from" on all tmp_ tables IF the user didn't cancel
        qDebug() << "Downloaded CSVs have been loaded into their tmp_ tables";

        updatesTable = new MergeTables();
        connect(updatesTable,SIGNAL(finished()),this,SLOT(cleanup()));
        updatesTable->setAttribute(Qt::WA_DeleteOnClose);
        updatesTable->displayChoices();
    }
}

void StartWindow::cleanup()
{
    ui->updatesAvailable->setHidden(true);

    // now get the release date of the local CSVs
    if (lastPublished.isEmpty())
    {
        QString dlPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/data/CSVs";
        QFile downloadedVersionFile(dlPath + "/last-downloaded.xml");
        if (downloadedVersionFile.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            QDomDocument xmlDoc;
            QDomElement docElem;
            xmlDoc.setContent(&downloadedVersionFile);
            docElem = xmlDoc.documentElement();

            QDomNode node = docElem.firstChild();
            while (!node.isNull())
            {
                if (node.nodeName() == "dcterms:modified")
                {
                    lastPublished = node.toElement().text();
                    break;
                }
                node = node.nextSibling();
            }
            downloadedVersionFile.close();
        }
        else
        {
            if (downloadedVersionFile.exists())
                qDebug() << "Could not open " + dlPath + "/last-downloaded.xml";
        }
    }
    // update metadata.version in settings table
    if (!lastPublished.isEmpty())
    {
        QSqlQuery qry;
        qry.prepare("INSERT OR REPLACE INTO settings (setting, value) VALUES (?, ?)");
        qry.addBindValue("metadata.version");
        qry.addBindValue(lastPublished);
        qry.exec();
    }

    QSqlQuery setCSVCheck;
    setCSVCheck.prepare("INSERT OR REPLACE INTO settings (setting, value) VALUES (?, ?)");
    setCSVCheck.addBindValue("metadata.updateavailable");
    setCSVCheck.addBindValue(false);
    setCSVCheck.exec();

    // remove CSVs and last-downloaded.xml
    QString folder = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/data/CSVs";
    QStringList csvs;
    csvs << "agents" << "determinations" << "images" << "names" << "organisms" << "sensu";
    for (auto csv : csvs)
    {
        QString file = folder + "/" + csv + ".csv";
        if (QFile::exists(file))
        {
            QFile::remove(file);
        }
    }

    QString xmlFile = folder + "/last-downloaded.xml";
    if (QFile::exists(xmlFile))
        QFile::remove(xmlFile);
}
