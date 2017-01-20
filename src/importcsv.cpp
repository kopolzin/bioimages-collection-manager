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

#include <QMessageBox>
#include <QSqlQuery>
#include <QSqlError>


#include "importcsv.h"
#include "startwindow.h"
#include "agent.h"
#include "determination.h"
#include "image.h"
#include "organism.h"
#include "sensu.h"
#include "taxa.h"

ImportCSV::ImportCSV()
{

}

ImportCSV::~ImportCSV()
{

}

QStringList ImportCSV::parseCSV(const QString &line, int numFields)
{
    QStringList splitLine;
    if (line.count("|") == numFields - 1)
    {
        splitLine = line.split("|");
    }
    else if (line.count("|") > numFields - 1)
    {
        int pos2 = 0;
        QRegExp rx2("(?:\"([^\"]*)\"\\|?)|(?:([^\\|]*)\\|?)");

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

    if (splitLine.count() != numFields)
        return splitLine;

    QStringList newSplitLine;
    for (QString s : splitLine)
    {
        newSplitLine << s.remove("\"");
    }
    splitLine = newSplitLine;
    return splitLine;
}

QStringList ImportCSV::parseCSV(const QString &line, int numFields, const QString &sep)
{
    if (sep.isEmpty())
        return { };
    QStringList splitLine;
    if (line.count(sep) == numFields - 1)
    {
        splitLine = line.split(sep);
    }
    else if (line.count(sep) > numFields - 1)
    {
        int pos2 = 0;
        QString rsep = sep;
        if (rsep == "|")
            rsep = "\\|";
        QRegExp rx2("(?:\"([^\"]*)\"" + rsep + "?)|(?:([^" + rsep + "]*)" + rsep + "?)");

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

    if (splitLine.count() != numFields)
        return splitLine;

    QStringList newSplitLine;
    for (QString s : splitLine)
    {
        newSplitLine << s.remove("\"");
    }
    splitLine = newSplitLine;
    return splitLine;
}

bool ImportCSV::extractSingleColumn(const QString &CSVPath, const QString &table, const QString &field, bool identifierIsFileName, bool hasHeader, bool idInFirstColumn, const QString separator)
{
    QFile file(CSVPath);

    if(!file.open(QFile::ReadOnly | QFile::Text))
    {
        QMessageBox msgBox;
        msgBox.setText("Could not open " + CSVPath);
        return false;
    }

    QTextStream in(&file);
    in.setCodec("UTF-8");
    QString line;
    if (hasHeader)
    {
        line = in.readLine();
    }

    QList<QStringList> extractedValues;
    do
    {
        line = in.readLine();
        if (line.isEmpty())
            continue;

        QStringList splitLine = parseCSV(line,2,separator);
        if (splitLine.length() != 2)
            continue;
        extractedValues << splitLine;
    } while (!line.isNull());

    file.close();

    QSqlDatabase db = QSqlDatabase::database();
    db.transaction();

    for (QStringList idValuePair : extractedValues)
    {
        QString identifier;
        QString value;
        if (idValuePair.length() != 2)
            continue;
        if (idInFirstColumn)
        {
            identifier = idValuePair.at(0);
            value = idValuePair.at(1);
        }
        else
        {
            value = idValuePair.at(0);
            identifier = idValuePair.at(1);
        }

        QSqlQuery query;
        QString idFieldName = "dcterms_identifier";
        if (table == "tmp_images" && identifierIsFileName)
            idFieldName = "fileName";
        if (field == "coordinates")
        {
            QStringList splitValue = value.split(",");
            if (splitValue.length() != 2)
                continue;
            QString lat = splitValue.at(0).trimmed();
            QString lon = splitValue.at(1).trimmed();
            query.prepare("UPDATE " + table + " SET dwc_decimalLatitude = (?), dwc_decimalLongitude = (?), dcterms_modified = (?) WHERE " + idFieldName + " = (?)");
            query.addBindValue(lat);
            query.addBindValue(lon);
            query.addBindValue(modifiedNow());
            query.addBindValue(identifier);
        }
        else
        {
            query.prepare("UPDATE " + table + " SET " + field + " = (?), dcterms_modified = (?) WHERE " + idFieldName + " = (?)");
            query.addBindValue(value);
            query.addBindValue(modifiedNow());
            query.addBindValue(identifier);
        }
        if (!query.exec()) {
            QMessageBox::critical(0, "", table + " insertion failed: " + query.lastError().text());
            return false;
        }
    }

    if (!db.commit())
    {
        qDebug() << "Problem committing changes to database. Data may be lost.";
        db.rollback();
        return false;
    }

    return true;
}

QString ImportCSV::findType(const QString &CSVPath) {
    QFile csv(CSVPath);

    if(!csv.open(QFile::ReadOnly | QFile::Text))
    {
        QMessageBox msgBox;
        msgBox.setText("Could not open " + CSVPath);
        return "";
    }

    QTextStream in(&csv);
    in.setCodec("UTF-8");
    QString line = in.readLine();
    // Strip quotes from the header line
    line = line.replace("\"","");
    QString type;
    if (line == "dcterms_identifier|dc_contributor|iri|contactURL|morphbankUserID|dcterms_modified|type")
        type = "agents";
    else if (line == "dsw_identified|identifiedBy|dwc_dateIdentified|dwc_identificationRemarks|tsnID|nameAccordingToID|dcterms_modified|suppress")
        type = "determinations";
    else if (line == "dcterms_identifier|dwc_establishmentMeans|dcterms_modified|dwc_organismRemarks|dwc_collectionCode|dwc_catalogNumber|dwc_georeferenceRemarks|dwc_decimalLatitude|dwc_decimalLongitude|geo_alt|dwc_organismName|dwc_organismScope|cameo|notes|suppress")
        type = "organisms";
    else if (line == "dcterms_identifier|dc_creator|tcsSignature|dcterms_title|dc_publisher|dcterms_created|iri|dcterms_modified")
        type = "sensu";
    else if (line == "fileName|focalLength|dwc_georeferenceRemarks|dwc_decimalLatitude|dwc_decimalLongitude|geo_alt|exif_PixelXDimension|exif_PixelYDimension|dwc_occurrenceRemarks|dwc_geodeticDatum|dwc_coordinateUncertaintyInMeters|dwc_locality|dwc_countryCode|dwc_stateProvince|dwc_county|dwc_informationWithheld|dwc_dataGeneralizations|dwc_continent|geonamesAdmin|geonamesOther|dcterms_identifier|dcterms_modified|dcterms_title|dcterms_description|ac_caption|photographerCode|dcterms_created|photoshop_Credit|owner|dcterms_dateCopyrighted|dc_rights|xmpRights_Owner|ac_attributionLinkURL|ac_hasServiceAccessPoint|usageTermsIndex|view|xmp_Rating|foaf_depicts|suppress")
        type = "images";
    else if (line == "ubioID|dcterms_identifier|dwc_kingdom|dwc_class|dwc_order|dwc_family|dwc_genus|dwc_specificEpithet|dwc_infraspecificEpithet|dwc_taxonRank|dwc_scientificNameAuthorship|dwc_vernacularName|dcterms_modified")
        type = "taxa";

    return type;
}

bool ImportCSV::extract(const QString &csvType, const QString &csvPath, const QString &tablePrefix)
{
    QString table = tablePrefix + csvType;
    if (csvType == "agents")
        extractAgents(csvPath, table);
    else if (csvType == "determinations")
        extractDeterminations(csvPath, table);
    else if (csvType == "images")
        extractImageNames(csvPath, table);
    else if (csvType == "organisms")
        extractOrganisms(csvPath, table);
    else if (csvType == "sensu")
        extractSensu(csvPath, table);
    else if (csvType == "taxa")
        extractTaxa(csvPath, table);
    else
        return false;

    return true;
}

bool ImportCSV::extractAgents(const QString &CSVPath, const QString &table)
{
    QFile agentsCSV(CSVPath);

    if(!agentsCSV.open(QFile::ReadOnly | QFile::Text))
    {
        QMessageBox msgBox;
        msgBox.setText("Could not open " + CSVPath);
        return false;
    }

    QTextStream in(&agentsCSV);
    in.setCodec("UTF-8");
    QString line = in.readLine();
    // Strip quotes from the header line
    line = line.replace("\"","");
    //          "dcterms_identifier"|"dc_contributor"|"iri"|"contactURL"|"morphbankUserID"|"dcterms_modified"|"type"
    if (line != "dcterms_identifier|dc_contributor|iri|contactURL|morphbankUserID|dcterms_modified|type")
    {
        qDebug() << "Error loading agents.csv. Header line does not match.";
        return false;
    }

    QList<Agent> extractedAgents;
    do
    {
        line = in.readLine();
        if (line.isEmpty())
            continue;

        QStringList splitLine = parseCSV(line,7);
        if (splitLine.length() != 7)
            continue;

        Agent newAgent;
        newAgent.setAgentCode(splitLine.at(0));
        newAgent.setFullName(splitLine.at(1));
        newAgent.setAgentURI(splitLine.at(2));
        newAgent.setContactURL(splitLine.at(3));
        newAgent.setMorphbankID(splitLine.at(4));
        newAgent.setLastModified(splitLine.at(5));
        newAgent.setAgentType(splitLine.at(6));

        extractedAgents << newAgent;

    } while (!line.isNull());

    agentsCSV.close();

    QSqlDatabase db = QSqlDatabase::database();
    db.transaction();

    if (!db.tables().contains(table))
        QSqlQuery createQuery("CREATE TABLE " + table + " (dcterms_identifier TEXT primary key, "
            "dc_contributor TEXT, iri TEXT, contactURL TEXT, morphbankUserID TEXT, "
            "dcterms_modified TEXT, type TEXT)");

    for (Agent agentDialog : extractedAgents)
    {
        QSqlQuery query;
        query.prepare("INSERT OR REPLACE INTO " + table + " (dcterms_identifier, dc_contributor, iri, "
                      "contactURL, morphbankUserID, dcterms_modified, type) "
                      "VALUES (?, ?, ?, ?, ?, ?, ?)");
        query.addBindValue(agentDialog.getAgentCode());
        query.addBindValue(agentDialog.getFullName());
        query.addBindValue(agentDialog.getAgentURI());
        query.addBindValue(agentDialog.getContactURL());
        query.addBindValue(agentDialog.getMorphbankID());
        query.addBindValue(agentDialog.getLastModified());
        query.addBindValue(agentDialog.getAgentType());
        if (!query.exec()) {
            QMessageBox::critical(0, "", "Agent insertion failed: " + query.lastError().text());
            return false;
        }
    }

    if (!db.commit())
    {
        qDebug() << "Problem committing changes to database. Data may be lost.";
        db.rollback();
        return false;
    }

    return true;
}

bool ImportCSV::extractDeterminations(const QString &CSVPath, const QString &table)
{
    QFile determCSV(CSVPath);

    if(!determCSV.open(QFile::ReadOnly | QFile::Text))
    {
        QMessageBox msgBox;
        msgBox.setText("Could not open " + CSVPath);
        return false;
    }

    QList<Determination> determinations;
    QTextStream in(&determCSV);
    in.setCodec("UTF-8");
    QString line = in.readLine();
    line = line.replace("\"","");
    if (line != "dsw_identified|identifiedBy|dwc_dateIdentified|dwc_identificationRemarks|tsnID|nameAccordingToID|dcterms_modified|suppress")
    {
        qDebug() << "Error loading determinations.csv. Header line does not match.";
        return false;
    }

    do
    {
        line = in.readLine();
        if (line.isNull() || line.isEmpty())
            continue;

        QStringList splitLine = parseCSV(line,8);
        if (splitLine.length() != 8)
            continue;

        Determination determination;
        determination.identified = splitLine.at(0);
        determination.identifiedBy = splitLine.at(1);
        determination.dateIdentified = splitLine.at(2);
        determination.identificationRemarks = splitLine.at(3);
        determination.tsnID = splitLine.at(4);
        determination.nameAccordingToID = splitLine.at(5);
        determination.lastModified = splitLine.at(6);
        determination.suppress = splitLine.at(7);

        determinations << determination;

    } while (!line.isNull());

    determCSV.close();

    QSqlDatabase db = QSqlDatabase::database();
    db.transaction();

    if (!db.tables().contains(table))
        QSqlQuery createQuery("CREATE TABLE " + table + " (dsw_identified TEXT, "
               "identifiedBy TEXT, dwc_dateIdentified TEXT, dwc_identificationRemarks TEXT, "
               "tsnID TEXT, nameAccordingToID TEXT, dcterms_modified TEXT, suppress TEXT, "
               "PRIMARY KEY(dsw_identified, dwc_dateIdentified, tsnID, nameAccordingToID))");

    for (Determination d : determinations)
    {
        QSqlQuery query;
        query.prepare("INSERT OR REPLACE INTO " + table + " (dsw_identified, identifiedBy, dwc_dateIdentified, "
                      "dwc_identificationRemarks, tsnID, nameAccordingToID, dcterms_modified, suppress) "
                      "VALUES (?, ?, ?, ?, ?, ?, ?, ?)");
        query.addBindValue(d.identified);
        query.addBindValue(d.identifiedBy);
        query.addBindValue(d.dateIdentified);
        query.addBindValue(d.identificationRemarks);
        query.addBindValue(d.tsnID);
        query.addBindValue(d.nameAccordingToID);
        query.addBindValue(d.lastModified);
        query.addBindValue(d.suppress);
        if (!query.exec()) {
            QMessageBox::critical(0, "", "Determination insertion failed: " + query.lastError().text());
            return false;
        }
    }

    if (!db.commit())
    {
        qDebug() << "Problem committing changes to database. Data may be lost.";
        db.rollback();
        return false;
    }

    return true;
}

bool ImportCSV::extractOrganisms(const QString &CSVPath, const QString &table)
{
    QFile organismsCSV(CSVPath);

    if(!organismsCSV.open(QFile::ReadOnly | QFile::Text))
    {
        QMessageBox msgBox;
        msgBox.setText("Could not open " + CSVPath);
        return false;
    }

    QList<Organism> organisms;
    QTextStream in(&organismsCSV);
    in.setCodec("UTF-8");
    QString line = in.readLine();
    line = line.replace("\"","");
    if (line != "dcterms_identifier|dwc_establishmentMeans|dcterms_modified|dwc_organismRemarks|dwc_collectionCode|dwc_catalogNumber|dwc_georeferenceRemarks|dwc_decimalLatitude|dwc_decimalLongitude|geo_alt|dwc_organismName|dwc_organismScope|cameo|notes|suppress")
    {
        qDebug() << "Error loading organisms.csv. Header line does not match.";
        return false;
    }

    do
    {
        line = in.readLine();
        if (line.isNull() || line.isEmpty())
            continue;

        QStringList splitLine = parseCSV(line,15);
        if (splitLine.length() != 15)
            continue;

        Organism organism;
        organism.identifier = splitLine.at(0); // remove URL from ID http://foo/bar/ID
        organism.establishmentMeans = splitLine.at(1);
        organism.lastModified = splitLine.at(2);
        organism.organismRemarks = splitLine.at(3);
        organism.collectionCode = splitLine.at(4);
        organism.catalogNumber = splitLine.at(5);
        organism.georeferenceRemarks = splitLine.at(6);
        organism.decimalLatitude = splitLine.at(7);
        organism.decimalLongitude = splitLine.at(8);
        organism.altitudeInMeters = splitLine.at(9);
        organism.organismName = splitLine.at(10);
        organism.organismScope = splitLine.at(11);
        organism.cameo = splitLine.at(12);
        organism.notes = splitLine.at(13);
        organism.suppress = splitLine.at(14);

        organisms << organism;

    } while (!line.isNull());

    organismsCSV.close();

    QSqlDatabase db = QSqlDatabase::database();
    db.transaction();

    if (!db.tables().contains(table))
        QSqlQuery createQuery("CREATE TABLE " + table + " (dcterms_identifier TEXT primary key, dwc_establishmentMeans TEXT, "
               "dcterms_modified TEXT, dwc_organismRemarks TEXT, dwc_collectionCode TEXT, dwc_catalogNumber TEXT, "
               "dwc_georeferenceRemarks TEXT, dwc_decimalLatitude TEXT, dwc_decimalLongitude TEXT, geo_alt TEXT, "
               "dwc_organismName TEXT, dwc_organismScope TEXT, cameo TEXT, notes TEXT, suppress TEXT)");

    for (Organism newOrganism : organisms)
    {
        QSqlQuery insert;
        insert.prepare("INSERT OR REPLACE INTO " + table + " (dcterms_identifier, dwc_establishmentMeans, "
                       "dcterms_modified, dwc_organismRemarks, dwc_collectionCode, dwc_catalogNumber, "
                       "dwc_georeferenceRemarks, dwc_decimalLatitude, dwc_decimalLongitude, geo_alt, "
                       "dwc_organismName, dwc_organismScope, cameo, notes, suppress) VALUES "
                       "(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
        insert.addBindValue(newOrganism.identifier);
        insert.addBindValue(newOrganism.establishmentMeans);
        insert.addBindValue(newOrganism.lastModified);
        insert.addBindValue(newOrganism.organismRemarks);
        insert.addBindValue(newOrganism.collectionCode);
        insert.addBindValue(newOrganism.catalogNumber);
        insert.addBindValue(newOrganism.georeferenceRemarks);

        insert.addBindValue(newOrganism.decimalLatitude);
        insert.addBindValue(newOrganism.decimalLongitude);
        insert.addBindValue(newOrganism.altitudeInMeters);
        insert.addBindValue(newOrganism.organismName);
        insert.addBindValue(newOrganism.organismScope);
        insert.addBindValue(newOrganism.cameo);
        insert.addBindValue(newOrganism.notes);
        insert.addBindValue(newOrganism.suppress);

        if (!insert.exec()) {
            QMessageBox::critical(0, "", "Organism insertion failed: " + insert.lastError().text());
            return false;
        }
    }

    if (!db.commit())
    {
        qDebug() << "Problem committing changes to database. Data may be lost.";
        db.rollback();
        return false;
    }

    return true;
}

bool ImportCSV::extractSensu(const QString &CSVPath, const QString &table)
{
    QFile sensuCSV(CSVPath);

    if(!sensuCSV.open(QFile::ReadOnly | QFile::Text))
    {
        QMessageBox msgBox;
        msgBox.setText("Could not open " + CSVPath);
        return false;
    }

    QList<Sensu> sensus;
    QTextStream in(&sensuCSV);
    in.setCodec("UTF-8");
    QString line = in.readLine();
    line = line.replace("\"","");
    if (line != "dcterms_identifier|dc_creator|tcsSignature|dcterms_title|dc_publisher|dcterms_created|iri|dcterms_modified")
    {
        qDebug() << "Error loading sensu.csv. Header line does not match.";
        return false;
    }

    do
    {
        line = in.readLine();
        if (line.isNull() || line.isEmpty())
            continue;

        QStringList splitLine = parseCSV(line,8);
        if (splitLine.length() != 8)
            continue;

        Sensu sensu;
        sensu.identifier = splitLine.at(0);
        sensu.creator = splitLine.at(1);
        sensu.tcsSignature = splitLine.at(2);
        sensu.title = splitLine.at(3);
        sensu.publisher = splitLine.at(4);
        sensu.dcterms_created = splitLine.at(5);
        sensu.iri = splitLine.at(6);
        sensu.lastModified = splitLine.at(7);
        sensus << sensu;

    } while (!line.isNull());

    sensuCSV.close();

    QSqlDatabase db = QSqlDatabase::database();
    db.transaction();

    if (!db.tables().contains(table))
        QSqlQuery createQuery("CREATE TABLE " + table + " (dcterms_identifier TEXT primary key, dc_creator TEXT, tcsSignature TEXT, "
               "dcterms_title TEXT, dc_publisher TEXT, dcterms_created TEXT, iri TEXT, dcterms_modified TEXT)");

    for (Sensu s : sensus)
    {
        QSqlQuery query;
        query.prepare("INSERT OR REPLACE INTO " + table + " (dcterms_identifier, dc_creator, tcsSignature, "
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
            QMessageBox::critical(0, "", "Sensu insertion failed: " + query.lastError().text());
            return false;
        }
    }

    if (!db.commit())
    {
        qDebug() << "Problem committing changes to database. Data may be lost.";
        db.rollback();
        return false;
    }

    return true;
}

QStringList ImportCSV::extractImageNames(const QString &CSVPath, const QString &table)
{
    QFile imagesCSV(CSVPath);

    if(!imagesCSV.open(QFile::ReadOnly | QFile::Text))
    {
        QMessageBox msgBox;
        msgBox.setText("Could not open " + CSVPath);
        return { };
    }

    QTextStream in(&imagesCSV);
    in.setCodec("UTF-8");
    QString line = in.readLine();
    line = line.replace("\"","");

    if (line != "fileName|focalLength|dwc_georeferenceRemarks|dwc_decimalLatitude|dwc_decimalLongitude|geo_alt|exif_PixelXDimension|exif_PixelYDimension|dwc_occurrenceRemarks|dwc_geodeticDatum|dwc_coordinateUncertaintyInMeters|dwc_locality|dwc_countryCode|dwc_stateProvince|dwc_county|dwc_informationWithheld|dwc_dataGeneralizations|dwc_continent|geonamesAdmin|geonamesOther|dcterms_identifier|dcterms_modified|dcterms_title|dcterms_description|ac_caption|photographerCode|dcterms_created|photoshop_Credit|owner|dcterms_dateCopyrighted|dc_rights|xmpRights_Owner|ac_attributionLinkURL|ac_hasServiceAccessPoint|usageTermsIndex|view|xmp_Rating|foaf_depicts|suppress")
    {
        qDebug() << "Error loading images.csv. Header line does not match.";
        return { };
    }

    QStringList imageNames;
    QList<Image> extractedImages;
    do
    {
        line = in.readLine();
        if (line.isNull() || line.isEmpty())
            continue;

        QStringList splitLine = parseCSV(line,39);
        if (splitLine.length() != 39)
            continue;

        QString imageName = splitLine.at(0);
        if (!imageName.isEmpty())
            imageNames.append(imageName);

        Image image;
        image.fileName = splitLine.at(0);
        image.focalLength = splitLine.at(1);
        image.georeferenceRemarks = splitLine.at(2);
        image.decimalLatitude = splitLine.at(3);
        image.decimalLongitude = splitLine.at(4);
        image.altitudeInMeters = splitLine.at(5);
        image.width = splitLine.at(6);
        image.height = splitLine.at(7);
        image.occurrenceRemarks = splitLine.at(8);
        image.geodeticDatum = splitLine.at(9);
        image.coordinateUncertaintyInMeters = splitLine.at(10);
        image.locality = splitLine.at(11);
        image.countryCode = splitLine.at(12);
        image.stateProvince = splitLine.at(13);
        image.county = splitLine.at(14);
        image.informationWithheld = splitLine.at(15);
        image.dataGeneralizations = splitLine.at(16);
        image.continent = splitLine.at(17);
        image.geonamesAdmin = splitLine.at(18);
        image.geonamesOther = splitLine.at(19);
        image.identifier = splitLine.at(20);
        image.lastModified = splitLine.at(21);
        image.title = splitLine.at(22);
        image.description = splitLine.at(23);
        image.caption = splitLine.at(24);
        image.photographerCode = splitLine.at(25);
        image.dcterms_created = splitLine.at(26);

        QStringList eventDateSplit = image.dcterms_created.split("T");
        if (eventDateSplit.length() == 2)
        {
            image.date = eventDateSplit.at(0);
            image.time = eventDateSplit.at(1);
        }
        else
            image.date = image.dcterms_created;

        image.credit = splitLine.at(27);
        image.copyrightOwnerID = splitLine.at(28);
        image.copyrightYear = splitLine.at(29);
        image.copyrightStatement = splitLine.at(30);
        image.copyrightOwnerName = splitLine.at(31);
        image.attributionLinkURL = splitLine.at(32);
        image.urlToHighRes = splitLine.at(33);
        image.usageTermsIndex = splitLine.at(34);
        image.imageView = splitLine.at(35);
        image.rating = splitLine.at(36);
        image.depicts = splitLine.at(37);
        image.suppress = splitLine.at(38);

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

        extractedImages << image;

    } while (!line.isNull());

    imagesCSV.close();

    QSqlDatabase db = QSqlDatabase::database();
    db.transaction();


    if (!db.tables().contains(table))
        QSqlQuery createQuery("CREATE TABLE " + table + " (fileName TEXT, focalLength TEXT, dwc_georeferenceRemarks TEXT, "
               "dwc_decimalLatitude TEXT, dwc_decimalLongitude TEXT, geo_alt TEXT, exif_PixelXDimension TEXT, "
               "exif_PixelYDimension TEXT, dwc_occurrenceRemarks TEXT, dwc_geodeticDatum TEXT, "
               "dwc_coordinateUncertaintyInMeters TEXT, dwc_locality TEXT, dwc_countryCode TEXT, "
               "dwc_stateProvince TEXT, dwc_county TEXT, dwc_informationWithheld TEXT, dwc_dataGeneralizations TEXT, "
               "dwc_continent TEXT, geonamesAdmin TEXT, geonamesOther TEXT, dcterms_identifier TEXT primary key, "
               "dcterms_modified TEXT, dcterms_title TEXT, dcterms_description TEXT, ac_caption TEXT, "
               "photographerCode TEXT, dcterms_created TEXT, photoshop_Credit TEXT, owner TEXT, "
               "dcterms_dateCopyrighted TEXT, dc_rights TEXT, xmpRights_Owner TEXT, ac_attributionLinkURL TEXT, "
               "ac_hasServiceAccessPoint TEXT, usageTermsIndex TEXT, view TEXT, xmp_Rating TEXT, foaf_depicts TEXT, "
               "suppress TEXT)");

    for (Image newImage : extractedImages)
    {
        QSqlQuery query;
        query.setForwardOnly(true);
        query.prepare("INSERT OR REPLACE INTO " + table + " (fileName, focalLength, dwc_georeferenceRemarks, "
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
    }

    if (!db.commit())
    {
        qDebug() << "Problem committing changes to database. Data may be lost.";
        db.rollback();
    }

    return imageNames;
}

bool ImportCSV::extractTaxa(const QString &CSVPath, const QString &table)
{
    QFile taxaCSV(CSVPath);

    if(!taxaCSV.open(QFile::ReadOnly | QFile::Text))
    {
        QMessageBox msgBox;
        msgBox.setText("Could not open " + CSVPath);
        return false;
    }

    QList<Taxa> taxa;
    QTextStream in(&taxaCSV);
    in.setCodec("UTF-8");
    QString line = in.readLine();
    line = line.replace("\"","");
    if (line != "ubioID|dcterms_identifier|dwc_kingdom|dwc_class|dwc_order|dwc_family|dwc_genus|dwc_specificEpithet|dwc_infraspecificEpithet|dwc_taxonRank|dwc_scientificNameAuthorship|dwc_vernacularName|dcterms_modified")
    {
        qDebug() << "Error loading names.csv. Header line does not match.";
        return false;
    }

    do
    {
        line = in.readLine();
        if (line.isNull() || line.isEmpty())
            continue;

        QStringList splitLine = parseCSV(line,13);
        if (splitLine.length() != 13)
            continue;

        Taxa t;
        t.ubioID = splitLine.at(0);
        t.identifier = splitLine.at(1);
        t.kingdom = splitLine.at(2);
        t.className = splitLine.at(3);
        t.order = splitLine.at(4);
        t.family = splitLine.at(5);
        t.genus = splitLine.at(6);
        t.species = splitLine.at(7);
        t.subspecies = splitLine.at(8);
        t.taxonRank = splitLine.at(9);
        t.authorship = splitLine.at(10);
        t.vernacular = splitLine.at(11);
        t.lastModified = splitLine.at(12);
        taxa << t;

    } while (!line.isNull());

    taxaCSV.close();

    QSqlDatabase db = QSqlDatabase::database();
    db.transaction();

    if (!db.tables().contains(table))
        QSqlQuery createQuery("CREATE TABLE " + table + " (ubioID TEXT, dcterms_identifier TEXT primary key, dwc_kingdom TEXT, "
               "dwc_class TEXT, dwc_order TEXT, dwc_family TEXT, dwc_genus TEXT, dwc_specificEpithet TEXT, "
               "dwc_infraspecificEpithet TEXT, dwc_taxonRank TEXT, dwc_scientificNameAuthorship TEXT, "
               "dwc_vernacularName TEXT, dcterms_modified TEXT)");

    for (Taxa t : taxa)
    {
        QSqlQuery query;
        query.prepare("INSERT OR REPLACE INTO " + table + " (ubioID, dcterms_identifier, "
                      "dwc_kingdom, dwc_class, dwc_order, dwc_family, dwc_genus, "
                      "dwc_specificEpithet, dwc_infraspecificEpithet, dwc_taxonRank, "
                      "dwc_scientificNameAuthorship, dwc_vernacularName, dcterms_modified) "
                      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
        query.addBindValue(t.ubioID);
        query.addBindValue(t.identifier);
        query.addBindValue(t.kingdom);
        query.addBindValue(t.className);
        query.addBindValue(t.order);
        query.addBindValue(t.family);
        query.addBindValue(t.genus);
        query.addBindValue(t.species);
        query.addBindValue(t.subspecies);
        query.addBindValue(t.taxonRank);
        query.addBindValue(t.authorship);
        query.addBindValue(t.vernacular);
        query.addBindValue(t.lastModified);
        if (!query.exec()) {
            QMessageBox::critical(0, "", "Taxa insertion failed: " + query.lastError().text());
            return false;
        }
    }

    if (!db.commit())
    {
        qDebug() << "Problem committing changes to database. Data may be lost.";
        db.rollback();
        return false;
    }

    return true;
}


QString ImportCSV::modifiedNow()
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
