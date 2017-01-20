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
#include <QSettings>
#include <QSqlQuery>

#include "managecsvs.h"
#include "ui_managecsvs.h"
#include "startwindow.h"
#include "importcsv.h"
#include "exportcsv.h"

ManageCSVs::ManageCSVs(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ManageCSVs)
{
    ui->setupUi(this);
    move(QApplication::desktop()->screen()->rect().center() - rect().center());

#ifdef Q_OS_MAC
    this->setStyleSheet("QLabel{font-size:12px} QCheckBox{font-size:11px} QRadioButton{font-size:11px} "
                        "QComboBox{font-size:12px} QPushButton{font-size:13px}");
    ui->label_8->setStyleSheet("font-size:11px");
    ui->exportLabel->setStyleSheet("font-size:15px");
    ui->importLabel->setStyleSheet("font-size:15px");
    ui->mergeLabel->setStyleSheet("font-size:15px");
    ui->group1label->setStyleSheet("font-size:13px");
    ui->group2Label->setStyleSheet("font-size:13px");
#endif

    ui->selectFirst->setAutoDefault(true);
    ui->selectSecond->setAutoDefault(true);
    ui->mergeButton->setAutoDefault(true);
    ui->selectCSVFolderButton->setAutoDefault(true);
    ui->backButton->setAutoDefault(true);

    QStringList importFields;
    imageFields << "URL of high-resolution image";
    imageFields << "Image GPS coordinates: latitude, longitude";
    imageFields << "Image latitude only" << "Image longitude only";
    imageFields << "Image altitude in meters" << "Image location uncertainty in meters";
    organismFields << "Organism GPS coordinates: latitude, longitude";
    organismFields << "Organism latitude only" << "Organism longitude only";
    organismFields << "Organism altitude in meters";
    organismFields << "Organism HTML notes";
    importFields.append(imageFields);
    importFields.append(organismFields);
    ui->valueDropdown->addItems(importFields);
    ui->valueDropdown->view()->setMinimumWidth(ui->valueDropdown->minimumSizeHint().width());

    oldAgent = "agent code,Name,URI,contact URL,Morphbank user ID";
    oldDeterm = "\"dwc:identificationID\",\"dwc:identifiedBy\",\"dwc:dateIdentified\",\"dwc:identificationRemarks\",\"TSNID\",\"dwc:nameAccordingToID\",\"changed\"";
    oldImage = "\"imageFileName\",\"imageDate\",\"imageTime\",\"focalLength\",\"dwc:decimalLatitude\",\"dwc:decimalLongitude\",\"elevationInMeters\",\"mix:imageWidth\",\"mix:imageHeight\",\"dwc:occurrenceRemarks\",\"dwc:geodeticDatum\",\"dwc:coordinateUncertaintyInMeters\",\"dwc:locality\",\"dwc:countryCode\",\"dwc:stateProvince\",\"dwc:county\",\"dwc:informationWithheld\",\"dwc:dataGeneralizations\",\"dwc:continent\",\"geonamesAdmin\",\"geonamesOther\",\"dcterms:identifier\",\"xmp:MetadataDate\",\"ImgCollectionCode\",\"dcterms:title\",\"dcterms:description\",\"ac:caption\",\"photographerCode\",\"recordedByCode\",\"dwc:eventDate\",\"photoshop:Credit\",\"dc:rights\",\"xmpRights:Owner\",\"ac:attributionLinkURL\",\"usageTermsIndex\",\"ImageView\",\"SernecCollectionStatus\",\"xmp:Rating\",\"foaf:depicts\"";
    oldOrgan = "\"individualOrganismID\",\"dwc:establishmentMeans\",\"xmp:MetadataDate\",\"individualRemarks\",\"dwc:collectionCode\",\"dwc:catalogNumber\",\"dwc:georeferenceRemarks\",\"dwc:decimalLatitude\",\"dwc:decimalLongitude\",\"elevationInMeters\",\"individualLinks\",\"changed\"";
    oldSensu = "identifier,author,citation following TCS signature fields,name,publisher,Date,identifier,changed";
    oldHeaders.append(oldAgent);
    oldHeaders.append(oldDeterm);
    oldHeaders.append(oldImage);
    oldHeaders.append(oldOrgan);
    oldHeaders.append(oldSensu);

    agent = "dcterms_identifier|dc_contributor|iri|contactURL|morphbankUserID|dcterms_modified|type";
    determ = "dsw_identified|identifiedBy|dwc_dateIdentified|dwc_identificationRemarks|tsnID|nameAccordingToID|dcterms_modified|suppress";
    image = "fileName|focalLength|dwc_georeferenceRemarks|dwc_decimalLatitude|dwc_decimalLongitude|geo_alt|exif_PixelXDimension|exif_PixelYDimension|dwc_occurrenceRemarks|dwc_geodeticDatum|dwc_coordinateUncertaintyInMeters|dwc_locality|dwc_countryCode|dwc_stateProvince|dwc_county|dwc_informationWithheld|dwc_dataGeneralizations|dwc_continent|geonamesAdmin|geonamesOther|dcterms_identifier|dcterms_modified|dcterms_title|dcterms_description|ac_caption|photographerCode|dcterms_created|photoshop_Credit|owner|dcterms_dateCopyrighted|dc_rights|xmpRights_Owner|ac_attributionLinkURL|ac_hasServiceAccessPoint|usageTermsIndex|view|xmp_Rating|foaf_depicts|suppress";
    organ = "dcterms_identifier|dwc_establishmentMeans|dcterms_modified|dwc_organismRemarks|dwc_collectionCode|dwc_catalogNumber|dwc_georeferenceRemarks|dwc_decimalLatitude|dwc_decimalLongitude|geo_alt|dwc_organismName|dwc_organismScope|cameo|notes|suppress";
    sensu = "dcterms_identifier|dc_creator|tcsSignature|dcterms_title|dc_publisher|dcterms_created|iri|dcterms_modified";
    names = "ubioID|dcterms_identifier|dwc_kingdom|dwc_class|dwc_order|dwc_family|dwc_genus|dwc_specificEpithet|dwc_infraspecificEpithet|dwc_taxonRank|dwc_scientificNameAuthorship|dwc_vernacularName|dcterms_modified";
    headers.append(agent);
    headers.append(determ);
    headers.append(image);
    headers.append(organ);
    headers.append(sensu);
    headers.append(names);

    ui->exportButton->setFocus();

    QSqlDatabase db = QSqlDatabase::database();
    db.transaction();

    dbLastPublished = "2015-09-21";
    QSqlQuery versionQry;
    versionQry.prepare("SELECT value FROM settings WHERE setting = (?)");
    versionQry.addBindValue("metadata.version");
    versionQry.exec();
    if (versionQry.next())
        dbLastPublished = versionQry.value(0).toString();

    QSqlQuery qry;
    qry.prepare("SELECT value FROM settings WHERE setting = (?)");
    qry.addBindValue("view.managecsvs.location");
    qry.exec();
    if (qry.next())
        restoreGeometry(qry.value(0).toByteArray());

    bool wasMaximized = false;
    qry.prepare("SELECT value FROM settings WHERE setting = (?)");
    qry.addBindValue("view.managecsvs.fullscreen");
    qry.exec();
    if (qry.next())
        wasMaximized = qry.value(0).toBool();

    if (wasMaximized)
        this->showMaximized();

    screenPosLoaded = true;

    if (!db.commit())
    {
        qDebug() << "In manageCSVs(): Problem querying database.";
        db.rollback();
    }
}

ManageCSVs::~ManageCSVs()
{
    delete ui;
}

void ManageCSVs::resizeEvent(QResizeEvent *)
{
    if (!screenPosLoaded)
        return;
    QSqlQuery qry;
    qry.prepare("INSERT OR REPLACE INTO settings (setting, value) VALUES (?, ?)");
    qry.addBindValue("view.managecsvs.location");
    qry.addBindValue(saveGeometry());
    qry.exec();
}

void ManageCSVs::moveEvent(QMoveEvent *)
{
    if (!screenPosLoaded)
        return;
    QSqlQuery qry;
    qry.prepare("INSERT OR REPLACE INTO settings (setting, value) VALUES (?, ?)");
    qry.addBindValue("view.managecsvs.location");
    qry.addBindValue(saveGeometry());
    qry.exec();
}

void ManageCSVs::changeEvent(QEvent* event)
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
        qry.addBindValue("view.managecsvs.fullscreen");
        qry.addBindValue(isMax);
        qry.exec();
    }
}

QStringList ManageCSVs::selectFiles()
{
    QString folder = "";
    QSqlQuery qry;
    qry.prepare("SELECT value FROM settings WHERE setting = (?)");
    qry.addBindValue("path.saveCSVfolder");
    qry.exec();
    if (qry.next())
        folder = qry.value(0).toString();

    QStringList filesToLoad;

    QFileDialog dialog(this);
    dialog.setDirectory(folder);
    dialog.setFileMode(QFileDialog::ExistingFiles);
    dialog.setNameFilter("CSV (*.csv)");
    dialog.setViewMode(QFileDialog::Detail);
    if (dialog.exec())
    {
        QStringList fileNames = dialog.selectedFiles();

        // make sure we know what to do with each file by reading its header
        for (auto fileName : fileNames)
        {
            QFile file(fileName);
            if(!file.open(QFile::ReadOnly | QFile::Text))
            {
                QMessageBox msgBox;
                msgBox.setText("Could not open " + fileName);
                continue;
            }

            QTextStream in(&file);
            in.setCodec("UTF-8");
            QString line;
            line = in.readLine();

            if (!headers.contains((line)))
            {
                qDebug() << "Error loading " + fileName + ", first line does not match any known header.";
                QMessageBox msgBox;
                msgBox.setText("Error loading " + fileName + ", first line does not match any known header.");
                msgBox.exec();
                continue;
            }

            filesToLoad.append(fileName);
        }
    }
    return filesToLoad;
}

QStringList ManageCSVs::convertToNewFormat(QString file)
{
    // read the first line to determine the column structure
    QFile f(file);

    if(!f.open(QFile::ReadOnly | QFile::Text))
    {
        QMessageBox msgBox;
        msgBox.setText("Could not open " + file);
        return { };
    }

    QTextStream in(&f);
    in.setCodec("UTF-8");
    QString header;
    QString line;
    QStringList lines;
    header = in.readLine();

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

    QString lastModified = currentDateTime + timezoneOffset;

    do
    {
        line = in.readLine();
        if (line.isNull() || line.isEmpty())
            continue;
        lines << line;
    } while (!line.isNull());

    f.close();

    QStringList newLines;

    if (header == oldAgent)
    {
        newLines << agent + "\n";
        for (QString l : lines)
        {
            QStringList splitLine;
            // count the number of commas, if it's 4 just replace the commas with pipes
            if (l.count(",") == 4)
                splitLine = l.split(",");
            else if (l.count(",") > 4)
            {
                int pos2 = 0;
                QRegExp rx2("(?:\"([^\"]*)\",?)|(?:([^,]*),?)");

                while (l.size()>pos2 && (pos2 = rx2.indexIn(l, pos2)) != -1)
                {
                    QString col;
                    if(rx2.cap(1).size()>0)
                        col = rx2.cap(1);
                    else if(rx2.cap(2).size()>0)
                        col = rx2.cap(2);

                    splitLine << col;

                    if(col.size())
                        pos2 += rx2.matchedLength();
                    else
                        pos2++;
                }
            }

            if (splitLine.count() != 5)
            {
                qDebug() << "converting oldAgent didn't work. count=" + QString::number(splitLine.count());
                continue;
            }

            // here is where we make our changes adding and removing columns
            splitLine.append(lastModified);
            splitLine.append("Person");
            newLines << splitLine.join("|") + "\n";
        }
    }
    else if (header == oldDeterm)
    {
        newLines << determ + "\n";

        // first split each line into a QSL split by pipes ("|")
        // check to make sure the QSL has the right number of columns (7)
        // then delete the old columns we don't care about anymore (none)
        // and add in blank spaces (or default values) for new columns (add after the 6th column)

        for (QString l : lines)
        {
            if (l.isEmpty())
                continue;

            QStringList splitLine;
            // there are 11 commas if no extra ones are present in any fields
            if (l.count(",") == 6)
            {
                splitLine = l.split(",");
            }
            else if (l.count(",") > 6)
            {
                int pos2 = 0;
                QRegExp rx2("(?:\"([^\"]*)\",?)|(?:([^,]*),?)");

                while (l.size()>pos2 && (pos2 = rx2.indexIn(l, pos2)) != -1)
                {
                    QString col;
                    if(rx2.cap(1).size()>0)
                        col = rx2.cap(1);
                    else if(rx2.cap(2).size()>0)
                        col = rx2.cap(2);

                    splitLine << col;

                    if(col.size())
                        pos2 += rx2.matchedLength();
                    else
                        pos2++;
                }
            }

            if (splitLine.count() != 7)
            {
                qDebug() << "converting line '" + l + "' of oldDeterminations didn't work. count=" + QString::number(splitLine.count());
                continue;
            }

            // here is where we make our changes adding and removing columns
            splitLine.removeAt(6);
            if (splitLine.at(5).isEmpty() || splitLine.at(5) == "\"\"")
                splitLine.replace(5, "nominal");
            splitLine.append(lastModified);
            splitLine.append("");   // suppress field
            newLines << splitLine.join("|") + "\n";
        }
    }
    else if (header == oldImage)
    {
        newLines << image + "\n";

        // Only load the highres.csv data if an oldImage header is trying to be converted
        QHash<QString,QString> highResHash = loadHighRes();
        QHash<QString,QString> agentsHash = loadAgents();

        for (QString l : lines)
        {
            QStringList splitLine;
            // there are 11 commas if no extra ones are present in any fields
            if (l.count(",") == 38)
            {
                splitLine = l.split(",");
            }
            else if (l.count(",") > 38)
            {
                int pos2 = 0;
                QRegExp rx2("(?:\"([^\"]*)\",?)|(?:([^,]*),?)");

                while (l.size()>pos2 && (pos2 = rx2.indexIn(l, pos2)) != -1)
                {
                    QString col;
                    if(rx2.cap(1).size()>0)
                        col = rx2.cap(1);
                    else if(rx2.cap(2).size()>0)
                        col = rx2.cap(2);

                    splitLine << col;

                    if(col.size())
                        pos2 += rx2.matchedLength();
                    else
                        pos2++;
                }
            }

            if (splitLine.count() != 39)
            {
                qDebug() << "converting oldImages didn't work. count=" + QString::number(splitLine.count());
                continue;
            }

            QString dcterms_identifier = splitLine.at(21);
            int iLastChar = dcterms_identifier.size() - 1;
            // remove quotes if present
            if (dcterms_identifier.left(1) == "\"" && dcterms_identifier.right(1) == "\"")
            {
                dcterms_identifier.remove(iLastChar,1);
                dcterms_identifier.remove(0,1);
            }
            QString local_identifier = dcterms_identifier.remove("http://bioimages.vanderbilt.edu/");
            QString ac_hasServiceAccessPoint = "";
            if (highResHash.contains(local_identifier))
                ac_hasServiceAccessPoint = highResHash.value(local_identifier);

            QString dc_rights = splitLine.at(31);
            iLastChar = dc_rights.size() - 1;
            // remove quotes if present
            if (dc_rights.left(1) == "\"" && dc_rights.right(1) == "\"")
            {
                dc_rights.remove(iLastChar,1);
                dc_rights.remove(0,1);
            }
            QString copyrightYear = "";
            QString agentCode = "";

            if (dc_rights.startsWith("(c) "))
            {
                dc_rights.remove("(c) ");
                copyrightYear = dc_rights.left(4); // capture the year
                dc_rights.remove(0,5); // remove the year and the following space leaving just the copyright holder
                QString fullAgentName = dc_rights;
                if (!fullAgentName.isEmpty() && agentsHash.contains(fullAgentName))
                    agentCode = agentsHash.value((fullAgentName));
            }

            QString dcterms_created = splitLine.at(29);
            iLastChar = dcterms_created.size() - 1;
            // remove quotes if present
            if (dcterms_created.left(1) == "\"" && dcterms_created.right(1) == "\"")
            {
                dcterms_created.remove(iLastChar,1);
                dcterms_created.remove(0,1);
            }
            if (dcterms_created.right(1) == "T")
            {
                dcterms_created.remove(dcterms_created.size()-1,1);
                splitLine.replace(29,dcterms_created);
            }

            splitLine.append(""); // add suppress column
            splitLine.removeAt(36); // remove SernecCollectionCode

            splitLine.insert(34, ac_hasServiceAccessPoint); //ac_hasServiceAccessPoint
            splitLine.insert(31, copyrightYear); //year copyrighted
            splitLine.insert(31, agentCode); //owner
            splitLine.removeAt(28); // remove recordedByCode
            splitLine.removeAt(23); //remove imgCollectionCode
            splitLine.insert(4,""); // add dwc_georeferenceRemarks
            splitLine.removeAt(2); // remove imageTime
            splitLine.removeAt(1); // remove imageDate
            splitLine.replaceInStrings("-9999","");

            newLines << splitLine.join("|") + "\n";
        }

    }
    else if (header == oldOrgan)
    {
        newLines << organ + "\n";

        for (QString l : lines)
        {
            QStringList splitLine;
            // there are 11 commas if no extra ones are present in any fields
            if (l.count(",") == 11)
            {
                splitLine = l.split(",");
            }
            else if (l.count(",") > 11)
            {
                int pos2 = 0;
                QRegExp rx2("(?:\"([^\"]*)\",?)|(?:([^,]*),?)");

                while (l.size()>pos2 && (pos2 = rx2.indexIn(l, pos2)) != -1)
                {
                    QString col;
                    if(rx2.cap(1).size()>0)
                        col = rx2.cap(1);
                    else if(rx2.cap(2).size()>0)
                        col = rx2.cap(2);

                    splitLine << col;

                    if(col.size())
                        pos2 += rx2.matchedLength();
                    else
                        pos2++;
                }
            }

            if (splitLine.count() != 12)
            {
                qDebug() << "converting oldIndividuals didn't work. count=" + QString::number(splitLine.count());
                continue;
            }

            // here is where we make our changes adding and removing columns
            splitLine.removeAt(11); // remove changed
            splitLine.removeAt(10); // remove individualLinks
            splitLine.append(""); // dwc_organismName
            splitLine.append("multicellular organism"); // dwc_organismScope
            splitLine.append(""); // cameo
            splitLine.append(""); // notes
            splitLine.append(""); // suppress
            splitLine.replaceInStrings("-9999","");

            newLines << splitLine.join("|") + "\n";
        }
    }
    else if (header == oldSensu)
    {
        newLines << sensu + "\n";

        for (QString l : lines)
        {
            QStringList splitLine;
            // there are 7 commas if no extra ones are present in any fields
            if (l.count(",") == 7)
            {
                splitLine = l.split(",");
            }
            else if (l.count(",") > 7)
            {
                int pos2 = 0;
                QRegExp rx2("(?:\"([^\"]*)\",?)|(?:([^,]*),?)");

                while (l.size()>pos2 && (pos2 = rx2.indexIn(l, pos2)) != -1)
                {
                    QString col;
                    if(rx2.cap(1).size()>0)
                        col = rx2.cap(1);
                    else if(rx2.cap(2).size()>0)
                        col = rx2.cap(2);

                    splitLine << col;

                    if(col.size())
                        pos2 += rx2.matchedLength();
                    else
                        pos2++;
                }
            }

            if (splitLine.count() != 8)
            {
                qDebug() << "converting oldSensu didn't work. count=" + QString::number(splitLine.count());
                continue;
            }

            // here is where we make our changes adding and removing columns
            splitLine.replace(7,lastModified);

            newLines << splitLine.join("|") + "\n";
        }
    }

    return newLines;
}

QHash<QString, QString> ManageCSVs::loadHighRes()
{
    QHash<QString,QString> highResHash;
    QFile highResFile(":/highres.csv");

    if(!highResFile.open(QFile::ReadOnly | QFile::Text))
    {
        qDebug() << "Could not open high res image location file";
        return highResHash;
    }

    QTextStream in(&highResFile);
    in.setCodec("UTF-8");
    QString line;

    line = in.readLine();

    // ubioID|dcterms_identifier|dwc_kingdom|dwc_class|dwc_order|dwc_family|dwc_genus|dwc_specificEpithet|dwc_infraspecificEpithet|dwc_taxonRank|dwc_scientificNameAuthorship|dwc_vernacularName|dcterms_modified
    if (line != "local identifier,highres sap url")
    {
        qDebug() << "Error loading high res image location file. First line does not match known header.";
        return highResHash;
    }

    do
    {
        line = in.readLine();

        //if somestring is null or empty dont save, otherwise...
        if (line.isNull() || line.isEmpty())
            continue;

        QStringList splitLine;
        // there is 1 comma if no extra ones are present in any fields
        if (line.count(",") == 1)
        {
            splitLine = line.split(",");
        }
        else if (line.count(",") > 1)
        {
            int pos2 = 0;
            QRegExp rx2("(?:\"([^\"]*)\",?)|(?:([^,]*),?)");

            while (line.size()>pos2 && (pos2 = rx2.indexIn(line, pos2)) != -1)
            {
                QString col;
                if(rx2.cap(1).size()>0)
                    col = rx2.cap(1);
                else if(rx2.cap(2).size()>0)
                    col = rx2.cap(2);

                splitLine << col;

                if(col.size())
                    pos2 += rx2.matchedLength();
                else
                    pos2++;
            }
        }

        if (splitLine.count() != 2)
        {
            qDebug() << "Splitting CSV apparently not fixed. count=" + QString::number(splitLine.count());
            continue;
        }

        highResHash.insert(splitLine.at(0),splitLine.at(1));

    } while (!line.isNull());

    highResFile.close();

    qDebug() << "High res image location file successfully loaded. " + QString::number(highResHash.size()) + " items stored in the hash.";

    return highResHash;
}

QHash<QString, QString> ManageCSVs::loadAgents()
{
    QHash<QString,QString> agentsHash;

    QSqlQuery query("SELECT dcterms_identifier, dc_contributor from agents");
    while (query.next())
    {
        QString dcterms_identifier = query.value(0).toString();
        QString dc_contributor = query.value(1).toString();
        agentsHash.insert(dc_contributor,dcterms_identifier);
    }

    return agentsHash;
}

void ManageCSVs::on_selectFirst_clicked()
{
    ui->firstSelected->clear();
    QStringList firstFileNames = selectFiles();
    if (!firstFileNames.isEmpty()) {
        ui->firstSelected->addItems(firstFileNames);
        ui->secondSelected->setFocus();
    }
}

void ManageCSVs::on_selectSecond_clicked()
{
    ui->secondSelected->clear();
    QStringList secondFileNames = selectFiles();
    if (!secondFileNames.isEmpty())
    {
        ui->secondSelected->addItems(secondFileNames);
        ui->mergeButton->setFocus();
    }
}

void ManageCSVs::on_mergeButton_clicked()
{
    // first, clear tables a_* and b_*, if they even exist
    QMessageBox msgBox;
    firstFileNames.clear();

    for (int i=0; i < ui->firstSelected->count(); i++)
    {
        firstFileNames.append(ui->firstSelected->item(i)->text());
    }

    if (firstFileNames.isEmpty())
    {
        msgBox.setText("Please select at least one CSV file to merge data into.");
        msgBox.exec();
        return;
    }

    secondFileNames.clear();
    for (int i=0; i < ui->secondSelected->count(); i++)
    {
        secondFileNames.append(ui->secondSelected->item(i)->text());
    }

    // go through firstFileNames list, for each file...
    for (QString mergeToFileString : firstFileNames)
    {
    // 1. move first.csv to first.csv.bak (if .bak exists, save to .bak1, .bak2, etc)
        QFile mergeToFile(mergeToFileString);

    // 2a. find what type of CSV file mergeToFileString is and if it's an old format, convert to new format
        if(!mergeToFile.open(QFile::ReadOnly | QFile::Text))
        {
            QMessageBox msgBox;
            msgBox.setText("Could not open " + mergeToFileString);
            continue;
        }

        QTextStream in(&mergeToFile);
        in.setCodec("UTF-8");
        QString firstHeader = in.readLine();
        mergeToFile.close();

        ImportCSV importCSV;
        QString tableType;
        if (firstHeader == agent)
        {
            tableType = "agents";
            importCSV.extractAgents(mergeToFileString, "a_" + tableType);
        }
        else if (firstHeader == determ)
        {
            tableType = "determinations";
            importCSV.extractDeterminations(mergeToFileString, "a_" + tableType);
        }
        else if (firstHeader == image)
        {
            tableType = "images";
            importCSV.extractImageNames(mergeToFileString, "a_" + tableType);
        }
        else if (firstHeader == organ)
        {
            tableType = "organisms";
            importCSV.extractOrganisms(mergeToFileString, "a_" + tableType);
        }
        else if (firstHeader == sensu)
        {
            tableType = "sensu";
            importCSV.extractSensu(mergeToFileString, "a_" + tableType);
        }
        else if (firstHeader == names)
        {
            tableType = "taxa";
            importCSV.extractTaxa(mergeToFileString, "a_" + tableType);
        }

    // 2b. match up secondFileNames of matching types, appending their data to the first

        if (firstFileNames.isEmpty())
        {
            continue;
        }

        for (QString dataToAddString : secondFileNames)
        {
            // if the filenames and paths are the same skip to the next one
            if (dataToAddString == mergeToFileString)
                continue;

            QFile dataToAddFile(dataToAddString);

            if(!dataToAddFile.open(QFile::ReadOnly | QFile::Text))
            {
                QMessageBox msgBox;
                msgBox.setText("Could not open " + dataToAddString);
                continue;
            }

            QTextStream toAddStream(&dataToAddFile);
            toAddStream.setCodec("UTF-8");
            QString toAddLine;
            QStringList toAddStringList;
            QString secondHeader = toAddStream.readLine();

            // close the file when finished reading from it
            dataToAddFile.close();

            // now see if the first line in toAddStringList matches the first line in
            if (firstHeader.trimmed() == secondHeader.trimmed())
            {
                if (firstHeader == agent)
                {
                    importCSV.extractAgents(dataToAddString, "b_" + tableType);
                }
                else if (firstHeader == determ)
                {
                    importCSV.extractDeterminations(dataToAddString, "b_" + tableType);
                }
                else if (firstHeader == image)
                {
                    importCSV.extractImageNames(dataToAddString, "b_" + tableType);
                }
                else if (firstHeader == organ)
                {
                    importCSV.extractOrganisms(dataToAddString, "b_" + tableType);
                }
                else if (firstHeader == sensu)
                {
                    importCSV.extractSensu(dataToAddString, "b_" + tableType);
                }
                else if (firstHeader == names)
                {
                    importCSV.extractTaxa(dataToAddString, "b_" + tableType);
                }
            }
        }
    }
    // keep track of which files were "merged", and tell the user it's OK to delete the merged secondFiles
    // also tell the user which files, if any, were NOT merged

    // both the a_ table and b_ table are now populated, so merge them
    QPointer<MergeTables> abTables = new MergeTables("a_", "b_");
    abTables->setAttribute(Qt::WA_DeleteOnClose);
    connect(abTables,SIGNAL(finished()),this,SLOT(save()));
    connect(abTables,SIGNAL(canceled()),this,SLOT(cleanupab()));
    abTables->displayChoices();

    return;
}

void ManageCSVs::on_backButton_clicked()
{
    this->hide();
    emit windowClosed();
}

void ManageCSVs::save()
{
    // export a_ tables to proper CSVs
    QString where = "";
    QString tablePrefix = "a_";

    ExportCSV exportCSV;
    exportCSV.saveData(where,tablePrefix);

    cleanupab();
}

void ManageCSVs::cleanupab()
{
    // drop a_ and b_ tables
    QStringList suffixes;
    suffixes << "agents" << "determinations" << "images" << "organisms" << "sensu" << "taxa";
    QStringList tableNames;
    for (auto s: suffixes)
    {
        tableNames.append("a_" + s);
        tableNames.append("b_" + s);
    }

    QSqlDatabase db = QSqlDatabase::database();
    db.transaction();

    for (auto table : tableNames)
        QSqlQuery dropQuery("DROP TABLE " + table);

    if (!db.commit())
    {
        qDebug() << __LINE__ << "Problem with database transaction";
        db.rollback();
    }
    db.close();
    db.open();
    QSqlQuery query;
    query.exec("VACUUM");
}

void ManageCSVs::on_selectCSVFolderButton_clicked()
{
    QString appPath = "";
    QString CSVFolder = "";

    QSqlDatabase db = QSqlDatabase::database();
    db.transaction();

    QSqlQuery qry;
    qry.prepare("SELECT value FROM settings WHERE setting = (?)");
    qry.addBindValue("path.applicationfolder");
    qry.exec();
    if (qry.next())
        appPath = qry.value(0).toString();

    qry.prepare("SELECT value FROM settings WHERE setting = (?)");
    qry.addBindValue("path.updateCSVfolder");
    qry.exec();
    if (qry.next())
        CSVFolder = qry.value(0).toString();

    if (!db.commit())
    {
        qDebug() << "Problem committing changes to database in ManageCSVs::on_selectCSVFolderButton_clicked()";
        db.rollback();
    }

//    CSVFolder = QFileDialog::getExistingDirectory(this,"Select directory containing CSV files",CSVFolder,QFileDialog::DontUseNativeDialog);
//    if (CSVFolder == "")
//        return;

    selectedCSVs = QFileDialog::getOpenFileNames(this,"Select CSV files to import",CSVFolder,"CSVs (*.csv)");
    if (selectedCSVs.isEmpty())
        return;

    CSVFolder = QFileInfo(selectedCSVs.first()).absolutePath();

    qry.prepare("INSERT OR REPLACE INTO settings (setting, value) VALUES (?, ?)");
    qry.addBindValue("path.updateCSVfolder");
    qry.addBindValue(CSVFolder);
    qry.exec();

    // alternate method of importing with user interaction on conflicts
//    importMergeQueue();
//    return;

    QPointer<QMessageBox> importingMsg = new QMessageBox;
    importingMsg->setAttribute(Qt::WA_DeleteOnClose);
    for (auto child : importingMsg->findChildren<QDialogButtonBox *>())
        child->hide();
    importingMsg->setWindowFlags(Qt::CustomizeWindowHint | Qt::WindowTitleHint);
    QString importMsgStr = "Please wait while the following file is imported:\n";
    if (selectedCSVs.size() > 1)
        importMsgStr = "Please wait while the following files are imported:\n";
    foreach (QString csvPath, selectedCSVs)
    {
        importMsgStr = importMsgStr + "\n- " + QFileInfo(csvPath).fileName();
    }
    importingMsg->setText(importMsgStr);
    importingMsg->setModal(true);
    importingMsg->show();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    // clear tmp_ tables
    cleanuptmp();

    // loop over selectedCSVs, find what CSV type they are, and import them one at a time
    foreach (QString csv, selectedCSVs) {
        ImportCSV importCSV;
        QString csvType = importCSV.findType(csv);
        if (csvType.isEmpty())
            continue;
        QString tablePrefix = "tmp_";
        importCSV.extract(csvType, csv, tablePrefix);
        // begin merging imported records
        MergeTables mergeTables(tablePrefix, "", csvType);
        // force importing new records overwriting old with no user interaction
        mergeTables.setSilentMerge(true);
        mergeTables.displayChoices();
        QCoreApplication::processEvents();
        cleanuptmp();
    }

    importingMsg->close();
    importingMsg->deleteLater();
    QMessageBox mbox;
    mbox.setText("Importing complete.");
    mbox.exec();
}

void ManageCSVs::importMergeQueue()
{
    cleanuptmp();

    if (selectedCSVs.isEmpty())
    {
        QMessageBox mbox;
        mbox.setText("Importing complete.");
        mbox.exec();
        return;
    }
    QString csv = selectedCSVs.first();
    selectedCSVs.removeFirst();

    ImportCSV importCSV;
    QString csvType = importCSV.findType(csv);
    if (!csvType.isEmpty())
    {
        QString tablePrefix = "tmp_";
        importCSV.extract(csvType, csv, tablePrefix);
        // begin merging imported records
        QPointer<MergeTables> importedCSVMerge = new MergeTables(tablePrefix, "", csvType);
        connect(importedCSVMerge,SIGNAL(finished()),this,SLOT(importMergeQueue()));
        connect(importedCSVMerge,SIGNAL(canceled()),this,SLOT(importMergeQueue()));
        importedCSVMerge->setAttribute(Qt::WA_DeleteOnClose);
//        importedCSVMerge->setSilentMerge(true);
        QCoreApplication::processEvents();
        importedCSVMerge->displayChoices();
    }
    else
    {
        // finish working our way through the import queue
        importMergeQueue();
    }
}

void ManageCSVs::cleanuptmp()
{
    // drop tmp_ tables
    QStringList suffixes;
    suffixes << "agents" << "determinations" << "images" << "organisms" << "sensu" << "taxa";
    QStringList tableNames;
    for (auto s: suffixes)
    {
        tableNames.append("tmp_" + s);
    }

    QSqlDatabase db = QSqlDatabase::database();
    db.transaction();

    for (auto table : tableNames)
        QSqlQuery dropQuery("DELETE FROM " + table);

    if (!db.commit())
    {
        qDebug() << __LINE__ << "Problem with database transaction";
        db.rollback();
    }
    db.close();
    db.open();
    QSqlQuery query;
    query.exec("VACUUM");
}

void ManageCSVs::on_selectTwoColumnCSV_clicked()
{
    QString separator = ui->separatorSymbol->text();
    if (separator.isEmpty())
    {
        QMessageBox msgBox;
        msgBox.setText("Please enter the symbol that separates the columns. For tabs enter \\t.");
        msgBox.exec();
    }
    bool idInFirstColumn = true;
    bool identifierIsFileName = false;
    bool hasHeader = false;
    QString field;
    if (ui->idInSecondColumn->isChecked())
        idInFirstColumn = false;
    if (ui->hasHeaderRowCheckbox->isChecked())
        hasHeader = true;
    if (ui->identifierIsFileNameCheckbox->isChecked())
        identifierIsFileName = true;
    field = ui->valueDropdown->currentText();

    // pop up CSV selection
    QString folder = "";
    QSqlQuery qry;
    qry.prepare("SELECT value FROM settings WHERE setting = (?)");
    qry.addBindValue("path.saveCSVfolder");
    qry.exec();
    if (qry.next())
        folder = qry.value(0).toString();

    QStringList fileNames;
    QFileDialog dialog(this);
    dialog.setDirectory(folder);
    dialog.setFileMode(QFileDialog::ExistingFiles);
    dialog.setNameFilter("CSV (*.csv *.txt)");
    dialog.setViewMode(QFileDialog::Detail);
    if (dialog.exec())
    {
        fileNames = dialog.selectedFiles();
    }
    if (fileNames.isEmpty())
        return;
    // parse selected CSVs using the given settings and update tmp_ tables with the imported data
    parseCSVs(fileNames,identifierIsFileName,hasHeader,idInFirstColumn,separator,field);
}

void ManageCSVs::parseCSVs(const QStringList fileNames, bool identifierIsFileName, bool hasHeader, bool idInFirstColumn, const QString separator, const QString field)
{
    if (fileNames.isEmpty())
        return;

    // first, clear tmp_* tables
    QString tableType;
    if (imageFields.contains(field))
        tableType = "images";
    else if (organismFields.contains(field))
        tableType = "organisms";
    else
    {
        QMessageBox msgBox;
        msgBox.setText("Could not determine which table to insert selected values into.");
        msgBox.exec();
        return;
    }

    QString fieldName;
    if (field == "URL of high-resolution image")
        fieldName = "ac_hasServiceAccessPoint";
    else if (field == "Image GPS coordinates: latitude, longitude" || "Organism GPS coordinates: latitude, longitude")
        fieldName = "coordinates";
    else if (field == "Image latitude only" || "Organism latitude only")
        fieldName = "dwc_decimalLatitude";
    else if (field == "Image longitude only" || "Organism longitude only")
        fieldName = "dwc_decimalLongitude";
    else if (field == "Image altitude in meters" ||  "Organism elevation in meters")
        fieldName = "geo_alt";
    else if (field == "Organism HTML notes")
        fieldName = "notes";

    QSqlDatabase db = QSqlDatabase::database();
    db.transaction();

    QString tmpTable = "tmp_" + tableType;
    QSqlQuery dropQuery("DELETE FROM " + tmpTable);
    QSqlQuery copyToTmpQuery("INSERT INTO " + tmpTable + " SELECT * FROM " + tableType);

    if (!db.commit())
    {
        qDebug() << __LINE__ << "Problem with database transaction";
        db.rollback();
        return;
    }
    for (QString fileName : fileNames)
    {
        QFile file(fileName);
        if(!file.open(QFile::ReadOnly | QFile::Text))
        {
            QMessageBox msgBox;
            msgBox.setText("Could not open " + fileName);
            continue;
        }
        ImportCSV importCSV;
        importCSV.extractSingleColumn(fileName, "tmp_" + tableType, fieldName, identifierIsFileName, hasHeader, idInFirstColumn, separator);
    }
    // the tmp_ tables are now populated, so merge them into the base tables
    QPointer<MergeTables> tmpCSVMerge = new MergeTables("tmp_", "",tableType);
    connect(tmpCSVMerge,SIGNAL(finished()),this,SLOT(cleanuptmp()));
    tmpCSVMerge->setAttribute(Qt::WA_DeleteOnClose);
    QCoreApplication::processEvents();
    tmpCSVMerge->displayChoices();
}

void ManageCSVs::on_exportButton_clicked()
{
    QString where = "";
    if (ui->onlyLocalChanges->isChecked())
        where = " where dcterms_modified > '" + dbLastPublished + "'";

    ExportCSV exportCSV;
    exportCSV.saveData(where,"");
}
