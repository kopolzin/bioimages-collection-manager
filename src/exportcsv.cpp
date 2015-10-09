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

#include <QSqlQuery>
#include <QFileDialog>
#include <QMessageBox>

#include "exportcsv.h"
#include "startwindow.h"
#include "agent.h"
#include "determination.h"
#include "image.h"
#include "organism.h"
#include "sensu.h"
#include "taxa.h"

ExportCSV::ExportCSV(QObject *parent) : QObject(parent)
{
}

ExportCSV::~ExportCSV()
{

}

void ExportCSV::saveData(const QString &where, const QString &tpref)
{
    // Retrieve last folder saved to
    QString workingFolder = "";
    QSqlQuery qry;
    qry.prepare("SELECT value FROM settings WHERE setting = (?)");
    qry.addBindValue("path.saveCSVfolder");
    qry.exec();
    if (qry.next())
        workingFolder = qry.value(0).toString();

    // Select save directory
    workingFolder = QFileDialog::getExistingDirectory(qobject_cast<QWidget*>(this->parent()),"Select directory to save CSV files",workingFolder,QFileDialog::ShowDirsOnly);
    if (workingFolder == "")
        return;

    qry.prepare("INSERT OR REPLACE INTO settings (setting, value) VALUES (?, ?)");
    qry.addBindValue("path.saveCSVfolder");
    qry.addBindValue(workingFolder);
    qry.exec();

    // Save image data to <working_folder>/images.csv
    QSqlQuery imageChanges;
    imageChanges.prepare("select * from " + tpref + "images" + where);
    imageChanges.exec();
    QList<Image> changedImages;
    while (imageChanges.next())
    {
        Image newImage;

        newImage.fileName = imageChanges.value(0).toString();
        newImage.focalLength = imageChanges.value(1).toString();
        newImage.georeferenceRemarks = imageChanges.value(2).toString();
        newImage.decimalLatitude = imageChanges.value(3).toString();
        newImage.decimalLongitude = imageChanges.value(4).toString();
        newImage.altitudeInMeters = imageChanges.value(5).toString();
        newImage.width = imageChanges.value(6).toString();
        newImage.height = imageChanges.value(7).toString();
        newImage.occurrenceRemarks = imageChanges.value(8).toString();
        newImage.geodeticDatum = imageChanges.value(9).toString();

        newImage.coordinateUncertaintyInMeters = imageChanges.value(10).toString();
        newImage.locality = imageChanges.value(11).toString();
        newImage.countryCode = imageChanges.value(12).toString();
        newImage.stateProvince = imageChanges.value(13).toString();
        newImage.county = imageChanges.value(14).toString();
        newImage.informationWithheld = imageChanges.value(15).toString();
        newImage.dataGeneralizations = imageChanges.value(16).toString();
        newImage.continent = imageChanges.value(17).toString();
        newImage.geonamesAdmin = imageChanges.value(18).toString();
        newImage.geonamesOther = imageChanges.value(19).toString();

        newImage.identifier = imageChanges.value(20).toString();
        newImage.lastModified = imageChanges.value(21).toString();
        newImage.title = imageChanges.value(22).toString();
        newImage.description = imageChanges.value(23).toString();
        newImage.caption = imageChanges.value(24).toString();
        newImage.photographerCode = imageChanges.value(25).toString();
        newImage.dcterms_created = imageChanges.value(26).toString();
        newImage.credit = imageChanges.value(27).toString();
        newImage.copyrightOwnerID = imageChanges.value(28).toString();
        newImage.copyrightYear = imageChanges.value(29).toString();

        newImage.copyrightStatement = imageChanges.value(30).toString();
        newImage.copyrightOwnerName = imageChanges.value(31).toString();
        newImage.attributionLinkURL = imageChanges.value(32).toString();
        newImage.urlToHighRes = imageChanges.value(33).toString();
        newImage.usageTermsIndex = imageChanges.value(34).toString();
        newImage.imageView = imageChanges.value(35).toString();
        newImage.rating = imageChanges.value(36).toString();
        newImage.depicts = imageChanges.value(37).toString();
        newImage.suppress = imageChanges.value(38).toString();

        changedImages << newImage;
    }

    if (!changedImages.isEmpty())
    {
        QFile imageCSV(workingFolder + "/images.csv");
        if (imageCSV.exists())
        {
            if (QMessageBox::No == QMessageBox(QMessageBox::Information, "File already exists",
                                                "Caution - images.csv already exists in this folder.\nAre you sure you want to overwrite it?",
                                                QMessageBox::Yes|QMessageBox::No).exec())
            {
                QMessageBox msgBox;
                msgBox.setText("Saving was canceled.");
                msgBox.exec();
                return;
            }
        }

        if (!imageCSV.open(QFile::WriteOnly | QFile::Text))
        {
            QMessageBox msgBox;
            msgBox.setText("Could not open images.csv for writing.");
            msgBox.exec();
            return;
        }

        QString imageHeader = "fileName|focalLength|dwc_georeferenceRemarks|dwc_decimalLatitude|dwc_decimalLongitude|geo_alt|exif_PixelXDimension|exif_PixelYDimension|dwc_occurrenceRemarks|dwc_geodeticDatum|dwc_coordinateUncertaintyInMeters|dwc_locality|dwc_countryCode|dwc_stateProvince|dwc_county|dwc_informationWithheld|dwc_dataGeneralizations|dwc_continent|geonamesAdmin|geonamesOther|dcterms_identifier|dcterms_modified|dcterms_title|dcterms_description|ac_caption|photographerCode|dcterms_created|photoshop_Credit|owner|dcterms_dateCopyrighted|dc_rights|xmpRights_Owner|ac_attributionLinkURL|ac_hasServiceAccessPoint|usageTermsIndex|view|xmp_Rating|foaf_depicts|suppress";
        QTextStream imageOut(&imageCSV);
        imageOut.setCodec("UTF-8");
        imageOut << imageHeader;

        for (Image img : changedImages)
        {
            // Convert other values to their saved form
            QString attributionURL = img.identifier + ".htm";
            QString coordinateUncertainty = img.coordinateUncertaintyInMeters;
            if (coordinateUncertainty == "")
                coordinateUncertainty = "1000";

            QString depicts = "";
            if (!img.depicts.isEmpty())
                depicts = img.depicts;

            // fields contains everything except the first column (img.filename) which will be added separately
            QStringList fields;
            fields << img.focalLength << img.georeferenceRemarks << img.decimalLatitude << img.decimalLongitude << img.altitudeInMeters
                   << img.width << img.height << img.occurrenceRemarks << img.geodeticDatum << coordinateUncertainty
                   << img.locality << img.countryCode << img.stateProvince << img.county << img.informationWithheld
                   << img.dataGeneralizations << img.continent << img.geonamesAdmin << img.geonamesOther << img.identifier
                   << img.lastModified << img.title << img.description << img.caption << img.photographerCode
                   << img.dcterms_created << img.credit << img.copyrightOwnerID << img.copyrightYear << img.copyrightStatement
                   << img.copyrightOwnerName << attributionURL << img.urlToHighRes << img.usageTermsIndex << img.imageView
                   << img.rating << depicts << img.suppress;

            QString row = "\n" + img.fileName;
            for (QString field : fields)
            {
                // if the field contains any pipes surround the entire field in quotes
                if (field.contains("|"))
                    row = row + "|\"" + field + "\"";
                else
                    row = row + "|" + field;
            }

            imageOut << row;
        }

        imageCSV.flush();
        imageCSV.close();
    }

    // Save organism data to <working_folder>/organisms.csv
    QSqlQuery orgChanges;
    orgChanges.prepare("select * from " + tpref + "organisms" + where);
    orgChanges.exec();
    QList<Organism> changedOrgs;
    while (orgChanges.next())
    {
        Organism newOrg;
        newOrg.identifier = orgChanges.value(0).toString();
        newOrg.establishmentMeans = orgChanges.value(1).toString();
        newOrg.lastModified = orgChanges.value(2).toString();
        newOrg.organismRemarks = orgChanges.value(3).toString();
        newOrg.collectionCode = orgChanges.value(4).toString();
        newOrg.catalogNumber = orgChanges.value(5).toString();
        newOrg.georeferenceRemarks = orgChanges.value(6).toString();
        newOrg.decimalLatitude = orgChanges.value(7).toString();
        newOrg.decimalLongitude = orgChanges.value(8).toString();
        newOrg.altitudeInMeters = orgChanges.value(9).toString();

        newOrg.organismName = orgChanges.value(10).toString();
        newOrg.organismScope = orgChanges.value(11).toString();
        newOrg.cameo = orgChanges.value(12).toString();
        newOrg.notes = orgChanges.value(13).toString();
        newOrg.suppress = orgChanges.value(14).toString();

        changedOrgs << newOrg;
    }

    if (!changedOrgs.isEmpty())
    {
        QFile organismCSV(workingFolder + "/organisms.csv");
        if (organismCSV.exists())
        {
            if (QMessageBox::No == QMessageBox(QMessageBox::Information, "File already exists",
                                                "Caution - organisms.csv already exists in this folder.\nAre you sure you want to overwrite it?",
                                                QMessageBox::Yes|QMessageBox::No).exec())
            {
                QMessageBox msgBox;
                msgBox.setText("Saving was canceled.");
                msgBox.exec();
                return;
            }
        }


        if (!organismCSV.open(QFile::WriteOnly | QFile::Text))
        {
            QMessageBox msgBox;
            msgBox.setText("Could not open organisms.csv for writing.");
            msgBox.exec();
            return;
        }

        QString organismHeader = "dcterms_identifier|dwc_establishmentMeans|dcterms_modified|dwc_organismRemarks|dwc_collectionCode|dwc_catalogNumber|dwc_georeferenceRemarks|dwc_decimalLatitude|dwc_decimalLongitude|geo_alt|dwc_organismName|dwc_organismScope|cameo|notes|suppress";

        QTextStream organismsOut(&organismCSV);
        organismsOut.setCodec("UTF-8");
        organismsOut << organismHeader;

        for (auto org : changedOrgs)
        {
            QString indID = org.identifier;
            QString estabMeans = org.establishmentMeans;
            if (estabMeans == "")
                estabMeans = "uncertain";

            // fields contains everything except indID, which will be handled separately
            QStringList fields;
            fields << estabMeans << org.lastModified << org.organismRemarks << org.collectionCode << org.catalogNumber
                   << org.georeferenceRemarks << org.decimalLatitude << org.decimalLongitude << org.altitudeInMeters
                   << org.organismName << org.organismScope << org.cameo << org.notes << org.suppress;

            QString row = "\n" + indID;
            for (QString field : fields)
            {
                if (field.contains("|"))
                    row = row + "|\"" + field + "\"";
                else
                    row = row + "|" + field;
            }

            organismsOut << row;
        }
        organismCSV.close();
    }

    // Save determination data to <working_folder>/determinations.csv
    QSqlQuery detChanges;
    detChanges.prepare("select * from " + tpref + "determinations" + where);
    detChanges.exec();
    QList<Determination> changedDets;
    while (detChanges.next())
    {
        Determination newDet;
        newDet.identified = detChanges.value(0).toString();
        newDet.identifiedBy = detChanges.value(1).toString();
        newDet.dateIdentified = detChanges.value(2).toString();
        newDet.identificationRemarks = detChanges.value(3).toString();
        newDet.tsnID = detChanges.value(4).toString();
        newDet.nameAccordingToID = detChanges.value(5).toString();
        newDet.lastModified = detChanges.value(6).toString();
        newDet.suppress = detChanges.value(7).toString();

        changedDets << newDet;
    }

    QStringList tsnIDs;
    if (!changedDets.isEmpty())
    {
        QFile determinationCSV(workingFolder + "/determinations.csv");

        if (determinationCSV.exists())
        {
            if (QMessageBox::No == QMessageBox(QMessageBox::Information, "File already exists",
                                                "Caution - determinations.csv already exists in this folder.\nAre you sure you want to overwrite it?",
                                                QMessageBox::Yes|QMessageBox::No).exec())
            {
                QMessageBox msgBox;
                msgBox.setText("Saving was canceled.");
                msgBox.exec();
                return;
            }
        }

        if (!determinationCSV.open(QFile::WriteOnly | QFile::Text))
        {
            QMessageBox msgBox;
            msgBox.setText("Could not open determinations.csv for writing.");
            msgBox.exec();
            return;
        }

        QString determinationHeader = "dsw_identified|identifiedBy|dwc_dateIdentified|dwc_identificationRemarks|tsnID|nameAccordingToID|dcterms_modified|suppress";
        QTextStream determinationsOut(&determinationCSV);
        determinationsOut.setCodec("UTF-8");
        determinationsOut << determinationHeader;

        for (Determination det : changedDets)
        {
            // it's not a determination if it hasn't been determined
            if (det.tsnID.isEmpty())
                continue;

            tsnIDs.append(det.tsnID);

            //QString dwcidentID = det.ID;
            QString nameAccordingTo = det.nameAccordingToID.split(" ").last();
            nameAccordingTo.remove("(");
            nameAccordingTo.remove(")");
            if (nameAccordingTo.isEmpty())
                nameAccordingTo = "nominal";

            // fields contains everything except det.ID, which is handled separately
            QStringList fields;
            fields << det.identifiedBy << det.dateIdentified << det.identificationRemarks
                   << det.tsnID << nameAccordingTo << det.lastModified << det.suppress;

            QString row = "\n" + det.identified;
            for (QString field : fields)
            {
                // if the field contains any pipes surround the entire field in quotes
                if (field.contains("|"))
                    row = row + "|\"" + field + "\"";
                else
                    row = row + "|" + field;
            }

            determinationsOut << row;
        }
        determinationCSV.close();
    }

    // Save agent data to <working_folder>/agents.csv
    QSqlQuery agentChanges;
    agentChanges.prepare("select * from " + tpref + "agents" + where);
    agentChanges.exec();
    QList<Agent> changedAgents;
    while (agentChanges.next())
    {
        Agent newAgent;
        newAgent.setAgentCode(agentChanges.value(0).toString());
        newAgent.setFullName(agentChanges.value(1).toString());
        newAgent.setAgentURI(agentChanges.value(2).toString());
        newAgent.setContactURL(agentChanges.value(3).toString());
        newAgent.setMorphbankID(agentChanges.value(4).toString());
        newAgent.setLastModified(agentChanges.value(5).toString());
        newAgent.setAgentType(agentChanges.value(6).toString());

        changedAgents << newAgent;
    }

    if (!changedAgents.isEmpty())
    {
        QFile agentCSV(workingFolder + "/agents.csv");

        if (agentCSV.exists())
        {
            if (QMessageBox::No == QMessageBox(QMessageBox::Information, "File already exists",
                                                "Caution - agents.csv already exists in this folder.\nAre you sure you want to overwrite it?",
                                                QMessageBox::Yes|QMessageBox::No).exec())
            {
                QMessageBox msgBox;
                msgBox.setText("Saving was canceled.");
                msgBox.exec();
                return;
            }
        }

        if (!agentCSV.open(QFile::WriteOnly | QFile::Text))
        {
            QMessageBox msgBox;
            msgBox.setText("Could not open agents.csv for writing.");
            msgBox.exec();
            return;
        }

        QString agentHeader = "dcterms_identifier|dc_contributor|iri|contactURL|morphbankUserID|dcterms_modified|type";
        QTextStream agentsOut(&agentCSV);
        agentsOut.setCodec("UTF-8");
        agentsOut << agentHeader;

        for (Agent agent : changedAgents)
        {
            if (agent.getAgentCode().isEmpty())
                continue;

            // fields contains everything except agent.ID, which is handled separately
            QStringList fields;
            fields << agent.getFullName() << agent.getAgentURI()
                   << agent.getContactURL() << agent.getMorphbankID() << agent.getLastModified() << agent.getAgentType();

            QString row = "\n" + agent.getAgentCode();
            for (QString field : fields)
            {
                // if the field contains any pipes surround the entire field in quotes
                if (field.contains("|"))
                    row = row + "|\"" + field + "\"";
                else
                    row = row + "|" + field;
            }

            agentsOut << row;
        }

        agentCSV.flush();
        agentCSV.close();
    }

    // Save sensu data to <working_folder>/sensu.csv
    QSqlQuery sensuChanges;
    sensuChanges.prepare("select dcterms_identifier, dc_creator, tcsSignature, dcterms_title, dc_publisher, "
                         "dcterms_created, iri, dcterms_modified from " + tpref + "sensu" + where);
    sensuChanges.exec();
    QList<Sensu> changedSensus;
    while (sensuChanges.next())
    {
        Sensu newSensu;
        newSensu.identifier = sensuChanges.value(0).toString();
        newSensu.creator = sensuChanges.value(1).toString();
        newSensu.tcsSignature = sensuChanges.value(2).toString();
        newSensu.title = sensuChanges.value(3).toString();
        newSensu.publisher = sensuChanges.value(4).toString();
        newSensu.dcterms_created = sensuChanges.value(5).toString();
        newSensu.iri = sensuChanges.value(6).toString();
        newSensu.lastModified = sensuChanges.value(7).toString();

        changedSensus << newSensu;
    }

    if (!changedSensus.isEmpty())
    {
        QFile sensuCSV(workingFolder + "/sensu.csv");

        if (sensuCSV.exists())
        {
            if (QMessageBox::No == QMessageBox(QMessageBox::Information, "File already exists",
                                                "Caution - sensu.csv already exists in this folder.\nAre you sure you want to overwrite it?",
                                                QMessageBox::Yes|QMessageBox::No).exec())
            {
                QMessageBox msgBox;
                msgBox.setText("Saving was canceled.");
                msgBox.exec();
                return;
            }
        }

        if (!sensuCSV.open(QFile::WriteOnly | QFile::Text))
        {
            QMessageBox msgBox;
            msgBox.setText("Could not open sensus.csv for writing.");
            msgBox.exec();
            return;
        }

        QString sensuHeader = "dcterms_identifier|dc_creator|tcsSignature|dcterms_title|dc_publisher|dcterms_created|iri|dcterms_modified";
        QTextStream sensusOut(&sensuCSV);
        sensusOut.setCodec("UTF-8");
        sensusOut << sensuHeader;

        for (Sensu sensu : changedSensus)
        {
            if (sensu.identifier.isEmpty())
                continue;

            // fields contains everything except sensu.ID, which is handled separately
            QStringList fields;
            fields << sensu.creator << sensu.tcsSignature << sensu.title
                   << sensu.publisher << sensu.dcterms_created << sensu.iri << sensu.lastModified;

            QString row = "\n" + sensu.identifier;
            for (QString field : fields)
            {
                // if the field contains any pipes surround the entire field in quotes
                if (field.contains("|"))
                    row = row + "|\"" + field + "\"";
                else
                    row = row + "|" + field;
            }

            sensusOut << row;
        }

        sensuCSV.flush();
        sensuCSV.close();
    }

    // Save taxa data to <working_folder>/names.csv
    QSqlQuery taxaChanges;
    taxaChanges.prepare("select * from " + tpref + "taxa" + where);
    taxaChanges.exec();
    QList<Taxa> changedTaxa;
    while (taxaChanges.next())
    {
        Taxa taxon;
        taxon.ubioID = taxaChanges.value(0).toString();
        taxon.identifier = taxaChanges.value(1).toString();
        taxon.kingdom = taxaChanges.value(2).toString();
        taxon.className = taxaChanges.value(3).toString();
        taxon.order = taxaChanges.value(4).toString();
        taxon.family = taxaChanges.value(5).toString();
        taxon.genus = taxaChanges.value(6).toString();
        taxon.species = taxaChanges.value(7).toString();
        taxon.subspecies = taxaChanges.value(8).toString();
        taxon.taxonRank = taxaChanges.value(9).toString();
        taxon.authorship = taxaChanges.value(10).toString();
        taxon.vernacular = taxaChanges.value(11).toString();
        taxon.lastModified = taxaChanges.value(12).toString();

        changedTaxa << taxon;
    }

    if (!changedTaxa.isEmpty())
    {
        QFile taxaCSV(workingFolder + "/names.csv");

        if (taxaCSV.exists())
        {
            if (QMessageBox::No == QMessageBox(QMessageBox::Information, "File already exists",
                                                "Caution - names.csv already exists in this folder.\nAre you sure you want to overwrite it?",
                                                QMessageBox::Yes|QMessageBox::No).exec())
            {
                QMessageBox msgBox;
                msgBox.setText("Saving was canceled.");
                msgBox.exec();
                return;
            }
        }

        if (!taxaCSV.open(QFile::WriteOnly | QFile::Text))
        {
            QMessageBox msgBox;
            msgBox.setText("Could not open names.csv for writing.");
            msgBox.exec();
            return;
        }

        QString taxaHeader = "ubioID|dcterms_identifier|dwc_kingdom|dwc_class|dwc_order|dwc_family|dwc_genus|dwc_specificEpithet|dwc_infraspecificEpithet|dwc_taxonRank|dwc_scientificNameAuthorship|dwc_vernacularName|dcterms_modified";
        QTextStream taxaOut(&taxaCSV);
        taxaOut.setCodec("UTF-8");
        taxaOut << taxaHeader;

        tsnIDs.removeDuplicates();
        for (Taxa taxon : changedTaxa)
        {
            if (taxon.identifier.isEmpty())
                continue;
            // only export tsnIDs from actual determinations
            if (!tsnIDs.contains(taxon.identifier))
                continue;

            // fields contains everything except taxon.ubioID, which is handled separately
            QStringList fields;
            fields << taxon.identifier << taxon.kingdom << taxon.className
                   << taxon.order << taxon.family << taxon.genus << taxon.species
                   << taxon.subspecies << taxon.taxonRank << taxon.authorship
                   << taxon.vernacular << taxon.lastModified;

            QString row = "\n" + taxon.ubioID;
            for (QString field : fields)
            {
                // if the field contains any pipes surround the entire field in quotes
                if (field.contains("|"))
                    row = row + "|\"" + field + "\"";
                else
                    row = row + "|" + field;
            }

            taxaOut << row;
        }

        taxaCSV.close();
    }

    QMessageBox msgBox;
    msgBox.setText("Data saved to CSV files successfully.");
    msgBox.exec();
}

