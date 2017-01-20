// The MIT License (MIT)
//
// Copyright (c) 2014-2017 Ken Polzin
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
#include <QMessageBox>
#include <QSqlQuery>
#include <QtSql>

#include "startwindow.h"
#include "advancedoptions.h"
#include "ui_advancedoptions.h"

AdvancedOptions::AdvancedOptions(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::AdvancedOptions)
{
    ui->setupUi(this);
    move(QApplication::desktop()->screen()->rect().center() - rect().center());

#ifdef Q_OS_MAC
    this->setStyleSheet("QLabel{font-size: 12px} QCheckBox{font-size: 12px} QComboBox{font-size: 12px} "
                        "QPushButton{font-size:12px}");
    ui->headingLabel->setStyleSheet("font: bold 14px");
    ui->resizeImagesButton->setStyleSheet("font-size: 13px");
    ui->backButton->setStyleSheet("font-size: 13px");
#endif

    loadAgents();

    QSqlDatabase db = QSqlDatabase::database();
    db.transaction();

    QSqlQuery qry;
    qry.prepare("SELECT value FROM settings WHERE setting = (?)");
    qry.addBindValue("path.photofolder");
    qry.exec();
    if (qry.next())
        photoFolder = qry.value(0).toString();

    baseFolder = "";

    qry.prepare("SELECT value FROM settings WHERE setting = (?)");
    qry.addBindValue("view.advancedoptions.location");
    qry.exec();
    if (qry.next())
        restoreGeometry(qry.value(0).toByteArray());

    bool wasMaximized = false;
    qry.prepare("SELECT value FROM settings WHERE setting = (?)");
    qry.addBindValue("view.advancedoptions.fullscreen");
    qry.exec();
    if (qry.next())
        wasMaximized = qry.value(0).toBool();

    if (wasMaximized)
        this->showMaximized();

    screenPosLoaded = true;

    if (!db.commit())
    {
        qDebug() << "In AdvancedOptions(): Problem querying database.";
        db.rollback();
    }
}

AdvancedOptions::~AdvancedOptions()
{
    delete ui;
}


void AdvancedOptions::resizeEvent(QResizeEvent *)
{
    if (!screenPosLoaded)
        return;
    QSqlQuery qry;
    qry.prepare("INSERT OR REPLACE INTO settings (setting, value) VALUES (?, ?)");
    qry.addBindValue("view.advancedoptions.location");
    qry.addBindValue(saveGeometry());
    qry.exec();
}

void AdvancedOptions::moveEvent(QMoveEvent *)
{
    if (!screenPosLoaded)
        return;
    QSqlQuery qry;
    qry.prepare("INSERT OR REPLACE INTO settings (setting, value) VALUES (?, ?)");
    qry.addBindValue("view.advancedoptions.location");
    qry.addBindValue(saveGeometry());
    qry.exec();
}

void AdvancedOptions::changeEvent(QEvent* event)
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
        qry.addBindValue("view.advancedoptions.fullscreen");
        qry.addBindValue(isMax);
        qry.exec();
    }
}

QStringList AdvancedOptions::selectImages()
{
    QStringList filesToLoad;

    QFileDialog dialog(this);
    dialog.setDirectory(photoFolder);
    dialog.setFileMode(QFileDialog::ExistingFiles);
    dialog.setNameFilter("JPG (*.jpg)");
    dialog.setViewMode(QFileDialog::Detail);
    if (dialog.exec())
    {
        filesToLoad = dialog.selectedFiles();
    }
    return filesToLoad;
}

void AdvancedOptions::on_selectImagesButton_clicked()
{

    imagesToResize = selectImages();
    if (!imagesToResize.isEmpty()) {
        QString num = QString::number(imagesToResize.size());
        ui->numImagesToResize->setText(num + " selected");
        ui->baseFolderButton->setFocus();
    }
    else
        ui->numImagesToResize->setText("0 selected");
}

void AdvancedOptions::on_baseFolderButton_clicked()
{

    if (baseFolder.isEmpty())
        baseFolder = photoFolder;

    QString folder = QFileDialog::getExistingDirectory(this,"Select base directory for TN, LQ and GQ images",baseFolder,QFileDialog::DontUseNativeDialog);

    if (folder == "")
    {
        ui->baseFolderLabel->setText("No folder selected");
        baseFolder = "";
        return;
    }

    ui->baseFolderLabel->setText(folder);
    baseFolder = folder;
}

void AdvancedOptions::loadAgents()
{
    agentHash.clear();

    QSqlQuery query;
    query.setForwardOnly(true);
    query.prepare("SELECT dcterms_identifier, dc_contributor, iri, contactURL, morphbankUserID, dcterms_modified, type from agents");
    query.exec();
    while (query.next())
    {
        QString dcterms_identifier = query.value(0).toString();
        QString dc_contributor = query.value(1).toString();
        agentHash.insert(dcterms_identifier,dc_contributor);

        Agent newAgent;
        newAgent.setAgentCode(dcterms_identifier);
        newAgent.setFullName(dc_contributor);
        newAgent.setAgentURI(query.value(2).toString());
        newAgent.setContactURL(query.value(3).toString());
        newAgent.setMorphbankID(query.value(4).toString());
        newAgent.setLastModified(query.value(5).toString());
        newAgent.setAgentType(query.value(6).toString());
    }

    QHashIterator<QString, QString> i(agentHash);
    while (i.hasNext()) {
        i.next();
        agentsList.append(i.key());
    }

    QString lastAgent = "";

    QSqlQuery qry;
    qry.prepare("SELECT value FROM settings WHERE setting = (?)");
    qry.addBindValue("last.agent");
    qry.exec();
    if (qry.next())
        lastAgent = qry.value(0).toString();

    agentsList.sort(Qt::CaseInsensitive);
    QPointer<QCompleter> completeAgent = new QCompleter(agentsList, this);
    completeAgent->setCaseSensitivity(Qt::CaseInsensitive);
    ui->agentBox->setCompleter(completeAgent);
    ui->agentBox->addItems(agentsList);
    ui->agentBox->setCurrentText(lastAgent);
    ui->agentBox->view()->setMinimumWidth(ui->agentBox->minimumSizeHint().width());
}

void AdvancedOptions::on_resizeImagesButton_clicked()
{
    photographer = ui->agentBox->currentText();

    if (baseFolder.isEmpty())
    {
        QMessageBox msgBox;
        msgBox.setText("Please set the base directory.");
        msgBox.exec();
        return;
    }
    else if (imagesToResize.isEmpty())
    {
        QMessageBox msgBox;
        msgBox.setText("Make sure at least one image is selected.");
        msgBox.exec();
        return;
    }
    else if (photographer.isEmpty())
    {
        QMessageBox msgBox;
        msgBox.setText("Please set the photographer first.");
        msgBox.exec();
        return;
    }

    int progressLength;
    int currentProgress;

    progressLength = imagesToResize.size();
    QString progressMsg = "Resizing and saving images";
    QProgressDialog progress(progressMsg, "Cancel", 0, progressLength, this);
    progress.setWindowModality(Qt::WindowModal);
    currentProgress = 0;
    progress.setValue(0);

    // in baseFolder create directories TN, LQ, GQ
    QString tnDir = baseFolder + "/tn/" + photographer;
    QString lqDir = baseFolder + "/lq/" + photographer;
    QString gqDir = baseFolder + "/gq/" + photographer;

    if (!QDir(baseFolder + "/tn").exists())
        QDir().mkdir(baseFolder + "/tn");
    if (!QDir(baseFolder + "/lq").exists())
        QDir().mkdir(baseFolder + "/lq");
    if (!QDir(baseFolder + "/gq").exists())
        QDir().mkdir(baseFolder + "/gq");

    if (!QDir(tnDir).exists())
        QDir().mkdir(tnDir);
    if (!QDir(lqDir).exists())
        QDir().mkdir(lqDir);
    if (!QDir(gqDir).exists())
        QDir().mkdir(gqDir);

    // loop through images, resizing them
    for (QString fileToRead : imagesToResize)
    {
        if (progress.wasCanceled())
            return;

        int gq = 1024;
        int lq = 480;
        int tn = 100;

        QFileInfo fileInfo(fileToRead);
        if (!fileInfo.isFile())
            continue;

        QString gqJpgPath = gqDir + "/g" + fileInfo.fileName();
        QString lqJpgPath = lqDir + "/w" + fileInfo.fileName();
        QString tnJpgPath = tnDir + "/t" + fileInfo.fileName();

        QImageReader imageReader(fileToRead);
        QSize fullSize = imageReader.size();
        int wid;
        int hei;

        // the width and height reported by QImageReader::size() depend upon image orientation
        if (imageReader.transformation() & QImageIOHandler::TransformationRotate90)
        {
            hei = fullSize.width();
            wid = fullSize.height();
        }
        else
        {
            wid = fullSize.width();
            hei = fullSize.height();
        }

        if (wid < 1024 && hei < 1024)
        {
            QMessageBox msgBox;
            msgBox.setText("Largest image dimension is less than 1024 pixels: " + fileInfo.fileName());
            msgBox.exec();
            continue;
        }

        int setW;
        int setH;
        int lqW;
        int lqH;
        int tnW;
        int tnH;

        if (wid == hei)
        {
            imageReader.setScaledSize(QSize(gq,gq));
            lqW = 480;
            lqH = 480;
            tnW = 100;
            tnH = 100;
        }
        else if (wid > hei)
        {
            qreal realGQ = qreal(gq*hei)/wid;
            setH = qCeil(realGQ);
            setW = gq;
            imageReader.setScaledSize(QSize(setW,setH));
            qreal realLQ = qreal(lq*hei)/wid;
            lqH = qCeil(realLQ);
            lqW = lq;
            qreal realTN = qreal(tn*hei)/wid;
            tnH = qCeil(realTN);
            tnW = tn;
        }
        else
        {
            qreal realGQ = qreal(gq*wid)/hei;
            setH = gq;
            setW = qCeil(realGQ);
            imageReader.setScaledSize(QSize(setW,setH));
            qreal realLQ = qreal(lq*wid)/hei;
            lqH = lq;
            lqW = qCeil(realLQ);
            qreal realTN = qreal(tn*wid)/hei;
            tnH = tn;
            tnW = qCeil(realTN);
        }

        if (imageReader.canRead())
        {
            QImage img = imageReader.read();
            QPixmap pscaled;
            pscaled = pscaled.fromImage(img);
            // save to gqDir + "/" + baseFilename;
            pscaled.save(gqJpgPath,"JPG");
            QPixmap lqScaled = pscaled.scaled(lqW,lqH, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            lqScaled.save(lqJpgPath,"JPG");
            QPixmap tnScaled = lqScaled.scaled(tnW,tnH, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            tnScaled.save(tnJpgPath,"JPG");
        }
        else
        {
            qDebug() << "Could not load image from file: " + fileInfo.fileName();
            continue;
        }

        currentProgress++;
        progress.setValue(int(currentProgress/progressLength));
    }

    progress.setValue(progress.maximum());

    QMessageBox msgBox;
    msgBox.setText("All images have been resized.");
    msgBox.exec();

}

void AdvancedOptions::on_backButton_clicked()
{
    this->hide();
    emit windowClosed();
}

void AdvancedOptions::on_resetCheckbox_toggled(bool checked)
{
    ui->resetButton->setEnabled(checked);
}

void AdvancedOptions::on_resetButton_clicked()
{
    ui->resetButton->setEnabled(false);
    ui->resetCheckbox->setChecked(false);
    QCoreApplication::processEvents();

    QStringList modTables;
    QString age = "agents";
    QString det = "determinations";
    QString ima = "images";
    QString org = "organisms";
    QString sen = "sensu";
    QString tax = "taxa";
    QString pub = "pub_";
    // delete all data tables except taxa, which is only partially on GitHub
    modTables << age << det << ima << org << sen; // << tax

    QSqlDatabase db = QSqlDatabase::database();
    db.transaction();
    for (auto t : modTables)
    {
        QSqlQuery dropQry;
        dropQry.prepare("DELETE FROM " + t);
        dropQry.exec();
    }

    // check for remote updates

    // download remote updates, if any

    // copy pub_ tables to agents, images, etc. tables
    for (auto t : modTables)
    {
        QSqlQuery renameQry;
        renameQry.prepare("INSERT INTO " + t + " SELECT * FROM " + pub + t);
        renameQry.exec();
    }

    if (!db.commit())
    {
        qDebug() << __LINE__ << "Problem with transaction while resetting database";
        db.rollback();
    }
    else
    {
        db.close();
        db.open();
        QSqlQuery query;
        query.exec("VACUUM");
    }

    // reload agents list for resizing images
    loadAgents();

    // say everything's finished
    QMessageBox msgBox;
    msgBox.setText("The database has been reset. You may continue using the software.");
    msgBox.exec();
    return;
}

void AdvancedOptions::on_convertITISButton_clicked()
{
    // pop up window to select database file containing ITIS tables, including hierarchy
    QString dbPath;
    QFileDialog dialog(this);
    dialog.setFileMode(QFileDialog::ExistingFile);
    dialog.setNameFilter("SQLite database files (*.db *.sqlite *.sqlite3 *.sql *.db3)");
    dialog.setViewMode(QFileDialog::Detail);
    if (dialog.exec())
    {
        QStringList selectedFiles;
        selectedFiles = dialog.selectedFiles();
        if (!selectedFiles.isEmpty())
            dbPath = selectedFiles.first();
    }

    if (dbPath.isEmpty())
        return;
    convertITIS(dbPath);
}

void AdvancedOptions::convertITIS(const QString dbpath)
{
    QSqlDatabase dbhier = QSqlDatabase::addDatabase("QSQLITE","ITISConnection");
    dbhier.setDatabaseName(dbpath);
    if (!dbhier.open()) {
        QMessageBox::critical(0, tr("Cannot open database"),
            "Unable to open database at: " + dbpath, QMessageBox::Cancel);
        return;
    }

    QPointer<QMessageBox> importingMsg = new QMessageBox;
    importingMsg->setAttribute(Qt::WA_DeleteOnClose);
    for (auto child : importingMsg->findChildren<QDialogButtonBox *>())
        child->hide();
    importingMsg->setText("Please wait while the ITIS database is being converted. This may\n\n"
                          "take several minutes as over 500,000 records are being processed.");
    importingMsg->setModal(true);
    importingMsg->show();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    struct Hierarchy
    {
        QString tsnID;
        QString hierarchyString;
        QString className = "";
        QString orderName = "";
        QString familyName = "";
    };
    QList<Hierarchy> hierarchies;

    dbhier.transaction();

    QSqlQuery del0(dbhier);
    del0.prepare("DELETE FROM taxonomic_units WHERE name_usage != 'valid' AND name_usage != 'accepted';");
    del0.exec();


    QSqlQuery createQry(dbhier);
    createQry.prepare("CREATE TABLE `taxa` ("
                      "`ubioID`	TEXT, `dcterms_identifier`	TEXT,"
                      "`dwc_kingdom`	TEXT, `dwc_class`	TEXT,"
                      "`dwc_order`	TEXT, `dwc_family`	TEXT,"
                      "`dwc_genus`	TEXT, `dwc_specificEpithet`	TEXT,"
                      "`dwc_infraspecificEpithet`	TEXT, `dwc_taxonRank`	TEXT, "
                      "`dwc_scientificNameAuthorship`	TEXT,"
                      "`dwc_vernacularName`	TEXT, `dcterms_modified`	TEXT,"
                      "PRIMARY KEY(`dcterms_identifier`) );");
    createQry.exec();

    QSqlQuery rankIndex(dbhier);
    rankIndex.prepare("CREATE INDEX `rank_id_index` ON `taxonomic_units` (`tsn` ,`rank_id` )");
    rankIndex.exec();

    if (!dbhier.commit())
    {
        qDebug() << __LINE__ << "Problem with database transaction.";
        dbhier.rollback();
    }

    QSqlQuery insertQry(dbhier);
    insertQry.prepare("INSERT INTO taxa SELECT '', taxonomic_units.tsn, kingdoms.kingdom_name, "
                      "'', '', '', '', '', '', '', '', '', '2016-12-22T00:00:00-06:00' FROM taxonomic_units "
                      "INNER JOIN kingdoms ON taxonomic_units.kingdom_id  = kingdoms.kingdom_id;");
    insertQry.exec();

    dbhier.transaction();

    QSqlQuery up1(dbhier);
    up1.prepare("UPDATE taxa SET dwc_taxonRank = (SELECT taxon_unit_types.rank_name FROM "
                "taxonomic_units INNER JOIN taxon_unit_types ON taxonomic_units.rank_id = "
                "taxon_unit_types.rank_id AND taxonomic_units.kingdom_id = "
                "taxon_unit_types.kingdom_id WHERE taxonomic_units.tsn = taxa.dcterms_identifier);");
    up1.exec();

    QSqlQuery up2(dbhier);
    up2.prepare("UPDATE taxa SET dwc_scientificNameAuthorship = (SELECT taxon_authors_lkp.taxon_author "
                "FROM taxonomic_units INNER JOIN taxon_authors_lkp ON taxonomic_units.taxon_author_id = "
                "taxon_authors_lkp.taxon_author_id WHERE taxonomic_units.tsn = taxa.dcterms_identifier);");
    up2.exec();

    QSqlQuery up3(dbhier);
    up3.prepare("UPDATE taxa SET dwc_vernacularName = (SELECT vernaculars.vernacular_name FROM vernaculars "
                "WHERE vernaculars.tsn = taxa.dcterms_identifier AND (vernaculars.language = 'English' OR "
                "vernaculars.language = 'unspecified'));");
    up3.exec();

    QSqlQuery up4(dbhier);
    up4.prepare("UPDATE taxa SET dwc_genus = (SELECT taxonomic_units.unit_name1 FROM taxonomic_units WHERE "
                "taxonomic_units.tsn = taxa.dcterms_identifier AND (taxa.dwc_taxonRank = 'Genus' OR "
                "taxa.dwc_taxonRank = 'Species' OR taxa.dwc_taxonRank = 'Subspecies' OR taxa.dwc_taxonRank = 'Variety'));");
    up4.exec();

    QSqlQuery up5(dbhier);
    up5.prepare("UPDATE taxa SET dwc_specificEpithet = (SELECT taxonomic_units.unit_name2 FROM taxonomic_units WHERE "
                "taxonomic_units.tsn = taxa.dcterms_identifier AND (taxa.dwc_taxonRank = 'Species' OR "
                "taxa.dwc_taxonRank = 'Subspecies' OR taxa.dwc_taxonRank = 'Variety'));");
    up5.exec();

    QSqlQuery up6(dbhier);
    up6.prepare("UPDATE taxa SET dwc_infraspecificEpithet = (SELECT taxonomic_units.unit_name3 FROM taxonomic_units WHERE "
                "taxonomic_units.tsn = taxa.dcterms_identifier AND (taxa.dwc_taxonRank = 'Subspecies' OR "
                "taxa.dwc_taxonRank = 'Variety'));");
    up6.exec();

    QSqlQuery up7(dbhier);
    up7.prepare("UPDATE taxa SET dwc_infraspecificEpithet = (SELECT taxonomic_units.unit_name3 FROM taxonomic_units WHERE "
                "taxonomic_units.tsn = taxa.dcterms_identifier AND taxonomic_units.unit_ind3 IS NULL AND "
                "(taxa.dwc_taxonRank = 'Subspecies' OR taxa.dwc_taxonRank = 'Variety'));");
    up7.exec();

    if (!dbhier.commit())
    {
        qDebug() << __LINE__ << "Problem with database transaction.";
        dbhier.rollback();
    }
    dbhier.transaction();

    QSqlQuery up8(dbhier);
    up8.prepare("UPDATE taxa SET dwc_taxonRank = '' WHERE dwc_taxonRank IS NULL;");
    up8.exec();

    QSqlQuery up9(dbhier);
    up9.prepare("UPDATE taxa SET dwc_scientificNameAuthorship = '' WHERE dwc_scientificNameAuthorship IS NULL;");
    up9.exec();

    QSqlQuery up10(dbhier);
    up10.prepare("UPDATE taxa SET dwc_vernacularName = '' WHERE dwc_vernacularName IS NULL;");
    up10.exec();

    QSqlQuery up11(dbhier);
    up11.prepare("UPDATE taxa SET dwc_genus = '' WHERE dwc_genus IS NULL;");
    up11.exec();

    QSqlQuery up12(dbhier);
    up12.prepare("UPDATE taxa SET dwc_specificEpithet = '' WHERE dwc_specificEpithet IS NULL;");
    up12.exec();

    QSqlQuery up13(dbhier);
    up13.prepare("UPDATE taxa SET dwc_infraspecificEpithet = '' WHERE dwc_infraspecificEpithet IS NULL;");
    up13.exec();

    if (!dbhier.commit())
    {
        qDebug() << __LINE__ << "Problem with database transaction.";
        dbhier.rollback();
    }

    QSqlQuery del1(dbhier);
    del1.prepare("delete from taxa where dwc_taxonRank = "
                 "'Subkingdom' OR 'Phylum' OR 'Subphylum' OR 'Superclass' OR "
                 "'Subclass' OR 'Infraclass' OR 'Superorder' OR 'Suborder' OR "
                 "'Infraorder' OR 'Superfamily' OR 'Subfamily' OR 'Tribe' OR "
                 "'Subtribe' OR 'Subgenus' OR 'Infrakingdom' OR 'Parvphylum' OR "
                 "'Variety' OR 'Superdivision' OR 'Division' OR 'Subdivision' OR "
                 "'Infradivision' OR 'Section' OR 'Subsection' OR 'Subvariety' OR "
                 "'Form' OR 'Subform' OR 'Race' OR 'Stirp' OR 'Morph' OR "
                 "'Aberration' OR 'Unspecified'");
    del1.exec();

    dbhier.transaction();

    QSqlQuery hierarchyQry(dbhier);
    hierarchyQry.prepare("SELECT TSN, hierarchy_string FROM hierarchy");
    hierarchyQry.exec();
    while (hierarchyQry.next())
    {
        Hierarchy h;
        h.tsnID = hierarchyQry.value(0).toString();
        h.hierarchyString = hierarchyQry.value(1).toString();
        hierarchies.append(h);
    }

    if (!dbhier.commit())
    {
        qDebug() << __LINE__ << "Problem with database transaction.";
        dbhier.rollback();
    }

    QCoreApplication::processEvents();

    // add an index on taxonomic_units.rank_id to speed up queries

    QHash<QString,QString> classes;
    QHash<QString,QString> orders;
    QHash<QString,QString> families;

    dbhier.transaction();
    QSqlQuery findClasses(dbhier);
    findClasses.prepare("SELECT tsn,unit_name1 from taxonomic_units WHERE rank_id = '60'");
    findClasses.exec();
    while (findClasses.next())
    {
        QString id = findClasses.value(0).toString();
        QString unitName = findClasses.value(1).toString();
        classes.insert(id,unitName);
    }
    QSqlQuery findOrders(dbhier);
    findOrders.prepare("SELECT tsn,unit_name1 from taxonomic_units WHERE rank_id = '100'");
    findOrders.exec();
    while (findOrders.next())
    {
        QString id = findOrders.value(0).toString();
        QString unitName = findOrders.value(1).toString();
        orders.insert(id,unitName);
    }
    QSqlQuery findFamilies(dbhier);
    findFamilies.prepare("SELECT tsn,unit_name1 from taxonomic_units WHERE rank_id = '140'");
    findFamilies.exec();
    while (findFamilies.next())
    {
        QString id = findFamilies.value(0).toString();
        QString unitName = findFamilies.value(1).toString();
        families.insert(id,unitName);
    }
    if (!dbhier.commit())
    {
        qDebug() << __LINE__ << "Problem with database transaction.";
        dbhier.rollback();
    }

    for (int i = 0; i < hierarchies.size(); ++i)
    {
        // skip over hierarchies that only contain a kingdom
        if (!hierarchies.at(i).hierarchyString.contains("-"))
            continue;
        // remove the first ID since it's always kingdom
        hierarchies[i].hierarchyString = hierarchies.at(i).hierarchyString + "-";
        QString firstID = hierarchies.at(i).hierarchyString.split("-").first();
        hierarchies[i].hierarchyString = hierarchies[i].hierarchyString.remove(0, firstID.length() + 1);
        // loop over the tsnIDs in the hierarchy searching for class, order, and family
        bool foundClass = false;
        bool foundOrder = false;
        bool foundFamily = false;
        for (int hLength = hierarchies.at(i).hierarchyString.count("-"); hLength > 0; hLength--)
        {
            QString hstring = hierarchies.at(i).hierarchyString.split("-").first();
            hierarchies[i].hierarchyString = hierarchies[i].hierarchyString.remove(0, hstring.length() + 1);

            if (!foundClass)
            {
                if (classes.contains(hstring))
                {
                    hierarchies[i].className = classes.value(hstring);
                    foundClass = true;
                }
            }
            else if (!foundOrder)
            {
                if (orders.contains(hstring))
                {
                    hierarchies[i].orderName = orders.value(hstring);
                    foundOrder = true;
                }
            }
            else if (!foundFamily)
            {
                if (families.contains(hstring))
                {
                    hierarchies[i].familyName = families.value(hstring);
                    foundFamily = true;
                }
            }
        }
    }

    dbhier.transaction();

    QCoreApplication::processEvents();

    foreach (Hierarchy h, hierarchies) {
        QSqlQuery updateQry(dbhier);
        updateQry.prepare("UPDATE taxa SET dwc_class = (?), dwc_order = (?), dwc_family = (?) WHERE dcterms_identifier = (?)");
        updateQry.addBindValue(h.className);
        updateQry.addBindValue(h.orderName);
        updateQry.addBindValue(h.familyName);
        updateQry.addBindValue(h.tsnID);
        updateQry.exec();
    }

    if (!dbhier.commit())
    {
        qDebug() << __LINE__ << "Problem with database transaction.";
        dbhier.rollback();
    }

    dbhier.close();
    dbhier.open();
    QSqlQuery query(dbhier);
    query.exec("VACUUM");
    dbhier.close();

    importingMsg->close();
    importingMsg->deleteLater();
    QMessageBox mbox;
    mbox.setText("Hierarchy extraction complete.");
    mbox.exec();
}
