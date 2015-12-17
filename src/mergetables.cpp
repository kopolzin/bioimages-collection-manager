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

#include <QtSql>
#include "mergetables.h"

MergeTables::MergeTables(QWidget *parent) :
    QWidget(parent)
{
    QSqlQuery versionQry;
    versionQry.prepare("SELECT value FROM settings WHERE setting = (?) LIMIT 1");
    versionQry.addBindValue("metadata.version");
    versionQry.exec();
    if (versionQry.next())
        databaseVersion = versionQry.value(0).toString();
    updating = true;

    ageT = "agents";
    imaT = "images";
    detT = "determinations";
    orgT = "organisms";
    senT = "sensu";
    namT = "taxa";
    age2T = "tmp_agents";
    ima2T = "tmp_images";
    det2T = "tmp_determinations";
    org2T = "tmp_organisms";
    sen2T = "tmp_sensu";
    nam2T = "tmp_taxa";
    setupLayout();
}

MergeTables::MergeTables(const QString &firstPrefix, const QString &secondPrefix, QWidget *parent) :
    QWidget(parent)
{
    QSqlQuery versionQry;
    versionQry.prepare("SELECT value FROM settings WHERE setting = (?) LIMIT 1");
    versionQry.addBindValue("metadata.version");
    versionQry.exec();
    if (versionQry.next())
        databaseVersion = versionQry.value(0).toString();
    updating = false;

    ageT = firstPrefix + "agents";
    imaT = firstPrefix + "images";
    detT = firstPrefix + "determinations";
    orgT = firstPrefix + "organisms";
    senT = firstPrefix + "sensu";
    namT = firstPrefix + "taxa";
    age2T = secondPrefix + "agents";
    ima2T = secondPrefix + "images";
    det2T = secondPrefix + "determinations";
    org2T = secondPrefix + "organisms";
    sen2T = secondPrefix + "sensu";
    nam2T = secondPrefix + "taxa";
    setupLayout();
}

MergeTables::MergeTables(const QString &firstPrefix, const QString &secondPrefix, const QString &oneTable, QWidget *parent) :
    QWidget(parent)
{
    QSqlQuery versionQry;
    versionQry.prepare("SELECT value FROM settings WHERE setting = (?) LIMIT 1");
    versionQry.addBindValue("metadata.version");
    versionQry.exec();
    if (versionQry.next())
        databaseVersion = versionQry.value(0).toString();
    updating = false;
    onlyTable = oneTable;

    ageT = firstPrefix + "agents";
    imaT = firstPrefix + "images";
    detT = firstPrefix + "determinations";
    orgT = firstPrefix + "organisms";
    senT = firstPrefix + "sensu";
    namT = firstPrefix + "taxa";
    age2T = secondPrefix + "agents";
    ima2T = secondPrefix + "images";
    det2T = secondPrefix + "determinations";
    org2T = secondPrefix + "organisms";
    sen2T = secondPrefix + "sensu";
    nam2T = secondPrefix + "taxa";
    setupLayout();
}

MergeTables::~MergeTables()
{
}

QSize MergeTables::sizeHint() const
{
    return QSize(640,240);
}

void MergeTables::displayChoices()
{
    // first let's query the tables for conflicting records, storing the dcterms_identifier (or PK values, for determinations)
    // then let's fill in the model by querying the appropriate tables for the saved dcterms_identifiers

    findDifferences();
    QCoreApplication::processEvents();

    if (imageIDs.isEmpty() && agentIDs.isEmpty() && determinationIDs.isEmpty()
            && organismIDs.isEmpty() && sensuIDs.isEmpty() && taxaIDs.isEmpty())
    {
        mergeNonConflicts();
        QCoreApplication::processEvents();
        alterTables();
        emit loaded();
        emit finished();
        QMessageBox mbox;
        if (updating)
        {
            mbox.setText("Your local database is now up to date.");
            mbox.exec();
        }
        else
        {
            mbox.setText("Finished merging CSVs.");
            mbox.exec();
        }

        return;
    }

    show();
    emit loaded();

    QSqlDatabase db = QSqlDatabase::database();
    db.transaction();
    QList<Image> toFromImages;
    for (auto id : imageIDs)
    {
            toFromImages.append(loadImages(ima2T," WHERE dcterms_identifier = '" + id + "' LIMIT 1").at(0));
            toFromImages.append(loadImages(imaT," WHERE dcterms_identifier = '" + id + "' LIMIT 1").at(0));
    }

    QList<Agent> toFromAgents;
    for (auto id : agentIDs)
    {
            toFromAgents.append(loadAgents(age2T," WHERE dcterms_identifier = '" + id + "' LIMIT 1").at(0));
            toFromAgents.append(loadAgents(ageT," WHERE dcterms_identifier = '" + id + "' LIMIT 1").at(0));

    }

    QList<Determination> toFromDeterminations;
    for (auto id : determinationIDs)
    {
            toFromDeterminations.append(loadDeterminations(det2T," WHERE " + id + " LIMIT 1").at(0));
            toFromDeterminations.append(loadDeterminations(detT," WHERE " + id + " LIMIT 1").at(0));
    }

    QList<Organism> toFromOrganisms;
    for (auto id : organismIDs)
    {
            toFromOrganisms.append(loadOrganisms(org2T," WHERE dcterms_identifier = '" + id + "' LIMIT 1").at(0));
            toFromOrganisms.append(loadOrganisms(orgT," WHERE dcterms_identifier = '" + id + "' LIMIT 1").at(0));
    }

    QList<Sensu> toFromSensu;
    for (auto id : sensuIDs)
    {
            toFromSensu.append(loadSensu(sen2T," WHERE dcterms_identifier = '" + id + "' LIMIT 1").at(0));
            toFromSensu.append(loadSensu(senT," WHERE dcterms_identifier = '" + id + "' LIMIT 1").at(0));

    }

    QList<Taxa> toFromTaxa;
    for (auto id : taxaIDs)
    {
            toFromTaxa.append(loadTaxa(nam2T," WHERE dcterms_identifier = '" + id + "' LIMIT 1").at(0));
            toFromTaxa.append(loadTaxa(namT," WHERE dcterms_identifier = '" + id + "' LIMIT 1").at(0));
    }

    if (!db.commit())
    {
        qDebug() << __LINE__ << "Problem with database transaction";
        db.rollback();
    }

    QList<QStringList> conflictingImages;
    for (auto tfi : toFromImages)
    {
        QStringList im;
        im.append(tfi.fileName);
        im.append(tfi.focalLength);
        im.append(tfi.georeferenceRemarks);
        im.append(tfi.decimalLatitude);
        im.append(tfi.decimalLongitude);
        im.append(tfi.altitudeInMeters);
        im.append(tfi.width);
        im.append(tfi.height);
        im.append(tfi.occurrenceRemarks);
        im.append(tfi.geodeticDatum);
        im.append(tfi.coordinateUncertaintyInMeters);
        im.append(tfi.locality);
        im.append(tfi.countryCode);
        im.append(tfi.stateProvince);
        im.append(tfi.county);
        im.append(tfi.informationWithheld);
        im.append(tfi.dataGeneralizations);
        im.append(tfi.continent);
        im.append(tfi.geonamesAdmin);
        im.append(tfi.geonamesOther);
        im.append(tfi.identifier);
        im.append(tfi.lastModified);
        im.append(tfi.title);
        im.append(tfi.description);
        im.append(tfi.caption);
        im.append(tfi.photographerCode);
        im.append(tfi.dcterms_created);
        im.append(tfi.credit);
        im.append(tfi.copyrightOwnerID);
        im.append(tfi.copyrightYear);
        im.append(tfi.copyrightStatement);
        im.append(tfi.copyrightOwnerName);
        im.append(tfi.attributionLinkURL);
        im.append(tfi.urlToHighRes);
        im.append(tfi.usageTermsIndex);
        im.append(tfi.imageView);
        im.append(tfi.rating);
        im.append(tfi.depicts);
        im.append(tfi.suppress);
        conflictingImages.append(im);
    }

    QList<QStringList> conflictingAgents;
    for (auto tfa : toFromAgents)
    {
        QStringList ag;
        ag.append(tfa.getAgentCode());
        ag.append(tfa.getFullName());
        ag.append(tfa.getAgentURI());
        ag.append(tfa.getContactURL());
        ag.append(tfa.getMorphbankID());
        ag.append(tfa.getLastModified());
        ag.append(tfa.getAgentType());
        conflictingAgents.append(ag);
    }

    QList<QStringList> conflictingDeterminations;
    for (auto determination : toFromDeterminations)
    {
        QStringList dt;
        dt.append(determination.identified);
        dt.append(determination.identifiedBy);
        dt.append(determination.dateIdentified);
        dt.append(determination.identificationRemarks);
        dt.append(determination.tsnID);
        dt.append(determination.nameAccordingToID);
        dt.append(determination.lastModified);
        dt.append(determination.suppress);
        conflictingDeterminations.append(dt);
    }

    QList<QStringList> conflictingOrganisms;
    for (auto organism : toFromOrganisms)
    {
        QStringList org;
        org.append(organism.identifier);
        org.append(organism.establishmentMeans);
        org.append(organism.lastModified);
        org.append(organism.organismRemarks);
        org.append(organism.collectionCode);
        org.append(organism.catalogNumber);
        org.append(organism.georeferenceRemarks);
        org.append(organism.decimalLatitude);
        org.append(organism.decimalLongitude);
        org.append(organism.altitudeInMeters);
        org.append(organism.organismName);
        org.append(organism.organismScope);
        org.append(organism.cameo);
        org.append(organism.notes);
        org.append(organism.suppress);
        conflictingOrganisms.append(org);
    }

    QList<QStringList> conflictingSensu;
    for (auto sensu : toFromSensu)
    {
        QStringList sn;
        sn.append(sensu.identifier);
        sn.append(sensu.creator);
        sn.append(sensu.tcsSignature);
        sn.append(sensu.title);
        sn.append(sensu.publisher);
        sn.append(sensu.dcterms_created);
        sn.append(sensu.iri);
        sn.append(sensu.lastModified);
        conflictingSensu.append(sn);
    }

    QList<QStringList> conflictingTaxa;
    for (auto taxon : toFromTaxa)
    {
        QStringList tx;
        tx.append(taxon.ubioID);
        tx.append(taxon.identifier);
        tx.append(taxon.kingdom);
        tx.append(taxon.className);
        tx.append(taxon.order);
        tx.append(taxon.family);
        tx.append(taxon.genus);
        tx.append(taxon.species);
        tx.append(taxon.subspecies);
        tx.append(taxon.taxonRank);
        tx.append(taxon.authorship);
        tx.append(taxon.vernacular);
        tx.append(taxon.lastModified);
        conflictingTaxa.append(tx);
    }

    // populate the images tab
    int c = 0;
    if (conflictingImages.size() > 0)
        c = conflictingImages.at(0).size() + 1;
    int r = toFromImages.size();
    QStandardItemModel *imagesModel = new QStandardItemModel(r, c);
    imagesTable = new QTableView(this);
    imagesTable->setModel(imagesModel);
    QStringList imageHeader;
    imageHeader << "keep" << "fileName" << "focalLength" << "dwc_georeferenceRemarks" <<
                   "dwc_decimalLatitude" << "dwc_decimalLongitude" << "geo_alt" <<
                   "exif_PixelXDimension" << "exif_PixelYDimension" <<
                   "dwc_occurrenceRemarks" << "dwc_geodeticDatum" <<
                   "dwc_coordinateUncertaintyInMeters" << "dwc_locality" <<
                   "dwc_countryCode" << "dwc_stateProvince" << "dwc_county" <<
                   "dwc_informationWithheld" << "dwc_dataGeneralizations" <<
                   "dwc_continent" << "geonamesAdmin" << "geonamesOther" <<
                   "dcterms_identifier" << "dcterms_modified" << "dcterms_title" <<
                   "dcterms_description" << "ac_caption" << "photographerCode" <<
                   "dcterms_created" << "photoshop_Credit" << "owner" <<
                   "dcterms_dateCopyrighted" << "dc_rights" << "xmpRights_Owner" <<
                   "ac_attributionLinkURL" << "ac_hasServiceAccessPoint" <<
                   "usageTermsIndex" << "view" << "xmp_Rating" << "foaf_depicts" <<
                   "suppress";
    imagesModel->setHorizontalHeaderLabels(imageHeader);
    for (int row = 0; row < r; ++row)
    {
        for (int column = 0; column < c; ++column)
        {
            if (column == 0)
            {
                QStandardItem *item = new QStandardItem();
                item->setEditable(false);
                if (row % 2 == 1)
                {
                    item->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
                    item->setCheckState(Qt::Checked);
                }
                imagesModel->setItem(row, column, item);
                const QModelIndex index = imagesModel->index(row, column);
                if (row % 4 == 2 || row % 4 == 3)
                    imagesModel->setData(index, QColor("lightgray"), Qt::BackgroundRole);
                if (row % 2 == 1 && column == 0)
                    imagesModel->setData(index, Qt::AlignCenter, Qt::TextAlignmentRole);
            }
            else
            {
                QStandardItem *item = new QStandardItem(conflictingImages.at(row).at(column-1));
                item->setEditable(false);
                imagesModel->setItem(row, column, item);
                const QModelIndex index = imagesModel->index(row, column);
                if (row % 4 == 2 || row % 4 == 3)
                    imagesModel->setData(index, QColor("lightgray"), Qt::BackgroundRole);
                if (row % 2 == 1 && column == 0)
                    imagesModel->setData(index, Qt::AlignCenter, Qt::TextAlignmentRole);
            }
        }
    }

    // populate the agents tab
    c = 0;
    if (conflictingAgents.size() > 0)
        c = conflictingAgents.at(0).size() + 1;
    r = toFromAgents.size();
    QStandardItemModel *agentsModel = new QStandardItemModel(r, c);
    agentsTable = new QTableView(this);
    agentsTable->setModel(agentsModel);
    QStringList agentHeader;
    agentHeader << "keep" << "dcterms_identifier" << "dc_contributor" << "iri" << "contactURL" << "morphbankUserID" << "dcterms_modified" << "type";
    agentsModel->setHorizontalHeaderLabels(agentHeader);
    for (int row = 0; row < r; ++row)
    {
        for (int column = 0; column < c; ++column)
        {
            if (column == 0)
            {
                QStandardItem *item = new QStandardItem();
                item->setEditable(false);
                if (row % 2 == 1)
                {
                    item->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
                    item->setCheckState(Qt::Checked);
                }
                agentsModel->setItem(row, column, item);
                const QModelIndex index = agentsModel->index(row, column);
                if (row % 4 == 2 || row % 4 == 3)
                    agentsModel->setData(index, QColor("lightgray"), Qt::BackgroundRole);
                if (row % 2 == 1 && column == 0)
                    agentsModel->setData(index, Qt::AlignCenter, Qt::TextAlignmentRole);
            }
            else
            {
                QStandardItem *item = new QStandardItem(conflictingAgents.at(row).at(column-1));
                item->setEditable(false);
                agentsModel->setItem(row, column, item);
                const QModelIndex index = agentsModel->index(row, column);
                if (row % 4 == 2 || row % 4 == 3)
                    agentsModel->setData(index, QColor("lightgray"), Qt::BackgroundRole);
                if (row % 2 == 1 && column == 0)
                    agentsModel->setData(index, Qt::AlignCenter, Qt::TextAlignmentRole);
            }
        }
    }

    // populate the determinations tab
    c = 0;
    if (conflictingDeterminations.size() > 0)
        c = conflictingDeterminations.at(0).size() + 1;
    r = toFromDeterminations.size();
    QStandardItemModel *determinationsModel = new QStandardItemModel(r, c);
    determinationsTable = new QTableView(this);
    determinationsTable->setModel(determinationsModel);
    QStringList determHeader;
    determHeader << "keep" << "dsw_identified" << "identifiedBy" << "dwc_dateIdentified"
                 << "dwc_identificationRemarks" << "tsnID" << "nameAccordingToID"
                 << "dcterms_modified" << "suppress";
    determinationsModel->setHorizontalHeaderLabels(determHeader);
    for (int row = 0; row < r; ++row)
    {
        for (int column = 0; column < c; ++column)
        {
            if (column == 0)
            {
                QStandardItem *item = new QStandardItem();
                item->setEditable(false);
                if (row % 2 == 1)
                {
                    item->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
                    item->setCheckState(Qt::Checked);
                }
                determinationsModel->setItem(row, column, item);
                const QModelIndex index = determinationsModel->index(row, column);
                if (row % 4 == 2 || row % 4 == 3)
                    determinationsModel->setData(index, QColor("lightgray"), Qt::BackgroundRole);
                if (row % 2 == 1 && column == 0)
                    determinationsModel->setData(index, Qt::AlignCenter, Qt::TextAlignmentRole);
            }
            else
            {
                QStandardItem *item = new QStandardItem(conflictingDeterminations.at(row).at(column-1));
                item->setEditable(false);
                determinationsModel->setItem(row, column, item);
                const QModelIndex index = determinationsModel->index(row, column);
                if (row % 4 == 2 || row % 4 == 3)
                    determinationsModel->setData(index, QColor("lightgray"), Qt::BackgroundRole);
                if (row % 2 == 1 && column == 0)
                    determinationsModel->setData(index, Qt::AlignCenter, Qt::TextAlignmentRole);
            }
        }
    }

    // populate the taxa tab
    c = 0;
    if (conflictingTaxa.size() > 0)
        c = conflictingTaxa.at(0).size() + 1;
    r = toFromTaxa.size();
    QStandardItemModel *taxaModel = new QStandardItemModel(r, c);
    taxaTable = new QTableView(this);
    taxaTable->setModel(taxaModel);
    QStringList taxaHeader;
    taxaHeader << "keep" << "ubioID" << "dcterms_identifier" << "dwc_kingdom" << "dwc_class" <<
                  "dwc_order" << "dwc_family" << "dwc_genus" << "dwc_specificEpithet" <<
                  "dwc_infraspecificEpithet" << "dwc_taxonRank" <<
                  "dwc_scientificNameAuthorship" << "dwc_vernacularName" << "dcterms_modified";
    taxaModel->setHorizontalHeaderLabels(taxaHeader);
    for (int row = 0; row < r; ++row) {
        for (int column = 0; column < c; ++column) {
            if (column == 0)
            {
                QStandardItem *item = new QStandardItem();
                item->setEditable(false);
                if (row % 2 == 1)
                {
                    item->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
                    item->setCheckState(Qt::Checked);
                }
                taxaModel->setItem(row, column, item);
                const QModelIndex index = taxaModel->index(row, column);
                if (row % 4 == 2 || row % 4 == 3)
                    taxaModel->setData(index, QColor("lightgray"), Qt::BackgroundRole);
                if (row % 2 == 1 && column == 0)
                    taxaModel->setData(index, Qt::AlignCenter, Qt::TextAlignmentRole);
            }
            else
            {
                QStandardItem *item = new QStandardItem(conflictingTaxa.at(row).at(column-1));
                item->setEditable(false);
                taxaModel->setItem(row, column, item);
                const QModelIndex index = taxaModel->index(row, column);
                if (row % 4 == 2 || row % 4 == 3)
                    taxaModel->setData(index, QColor("lightgray"), Qt::BackgroundRole);
                if (row % 2 == 1 && column == 0)
                    taxaModel->setData(index, Qt::AlignCenter, Qt::TextAlignmentRole);
            }
        }
    }

    // populate the organisms tab
    c = 0;
    if (conflictingOrganisms.size() > 0)
        c = conflictingOrganisms.at(0).size() + 1;
    r = toFromOrganisms.size();
    QStandardItemModel *organismsModel = new QStandardItemModel(r, c);
    organismsTable = new QTableView(this);
    organismsTable->setModel(organismsModel);

    QStringList orgHeader;
    orgHeader << "keep" << "dcterms_identifier" << "dwc_establishmentMeans" << "dcterms_modified" <<
                 "dwc_organismRemarks" << "dwc_collectionCode" << "dwc_catalogNumber" <<
                 "dwc_georeferenceRemarks" << "dwc_decimalLatitude" <<
                 "dwc_decimalLongitude" << "geo_alt" << "dwc_organismName" <<
                 "dwc_organismScope" << "cameo" << "notes" << "suppress";
    organismsModel->setHorizontalHeaderLabels(orgHeader);
    for (int row = 0; row < r; ++row) {
        for (int column = 0; column < c; ++column) {
            if (column == 0)
            {
                QStandardItem *item = new QStandardItem();
                item->setEditable(false);
                if (row % 2 == 1)
                {
                    item->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
                    item->setCheckState(Qt::Checked);
                }
                organismsModel->setItem(row, column, item);
                const QModelIndex index = organismsModel->index(row, column);
                if (row % 4 == 2 || row % 4 == 3)
                    organismsModel->setData(index, QColor("lightgray"), Qt::BackgroundRole);
                if (row % 2 == 1 && column == 0)
                    organismsModel->setData(index, Qt::AlignCenter, Qt::TextAlignmentRole);
            }
            else
            {
                QStandardItem *item = new QStandardItem(conflictingOrganisms.at(row).at(column-1));
                item->setEditable(false);
                organismsModel->setItem(row, column, item);
                const QModelIndex index = organismsModel->index(row, column);
                if (row % 4 == 2 || row % 4 == 3)
                    organismsModel->setData(index, QColor("lightgray"), Qt::BackgroundRole);
                if (row % 2 == 1 && column == 0)
                    organismsModel->setData(index, Qt::AlignCenter, Qt::TextAlignmentRole);
            }
        }
    }

    // populate the sensu tab
    c = 0;
    if (conflictingSensu.size() > 0)
        c = conflictingSensu.at(0).size() + 1;
    r = toFromSensu.size();
    QStandardItemModel *sensuModel = new QStandardItemModel(r, c);
    sensuTable = new QTableView(this);
    sensuTable->setModel(sensuModel);
    QStringList sensuHeader;
    sensuHeader << "keep" << "dcterms_identifier" << "dc_creator" << "tcsSignature" <<
                   "dcterms_title" << "dc_publisher" << "dcterms_created" <<
                   "iri" << "dcterms_modified";
    sensuModel->setHorizontalHeaderLabels(sensuHeader);
    for (int row = 0; row < r; ++row) {
        for (int column = 0; column < c; ++column) {
            if (column == 0)
            {
                QStandardItem *item = new QStandardItem();
                item->setEditable(false);
                if (row % 2 == 1)
                {
                    item->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
                    item->setCheckState(Qt::Checked);
                }
                sensuModel->setItem(row, column, item);
                const QModelIndex index = sensuModel->index(row, column);
                if (row % 4 == 2 || row % 4 == 3)
                    sensuModel->setData(index, QColor("lightgray"), Qt::BackgroundRole);
                if (row % 2 == 1 && column == 0)
                    sensuModel->setData(index, Qt::AlignCenter, Qt::TextAlignmentRole);
            }
            else
            {
                QStandardItem *item = new QStandardItem(conflictingSensu.at(row).at(column-1));
                item->setEditable(false);
                sensuModel->setItem(row, column, item);
                const QModelIndex index = sensuModel->index(row, column);
                if (row % 4 == 2 || row % 4 == 3)
                    sensuModel->setData(index, QColor("lightgray"), Qt::BackgroundRole);
                if (row % 2 == 1 && column == 0)
                    sensuModel->setData(index, Qt::AlignCenter, Qt::TextAlignmentRole);
            }
        }
    }

    agentsTable->resizeColumnsToContents();
    determinationsTable->resizeColumnsToContents();
    imagesTable->resizeColumnsToContents();
    taxaTable->resizeColumnsToContents();
    organismsTable->resizeColumnsToContents();
    sensuTable->resizeColumnsToContents();

    // only show tabs that have records
    QTabWidget *tabWidget = new QTabWidget(this);
    if (agentsTable->model()->rowCount() > 0)
        tabWidget->addTab(agentsTable,"Agents");
    else
        agentsTable->hide();
    if (determinationsTable->model()->rowCount() > 0)
        tabWidget->addTab(determinationsTable,"Determinations");
    else
        determinationsTable->hide();
    if (imagesTable->model()->rowCount() > 0)
        tabWidget->addTab(imagesTable,"Images");
    else
        imagesTable->hide();
    if (taxaTable->model()->rowCount() > 0)
        tabWidget->addTab(taxaTable,"Names");
    else
        taxaTable->hide();
    if (organismsTable->model()->rowCount() > 0)
        tabWidget->addTab(organismsTable,"Organisms");
    else
        organismsTable->hide();
    if (sensuTable->model()->rowCount() > 0)
        tabWidget->addTab(sensuTable,"Sensu");
    else
        sensuTable->hide();

    QPushButton *submitButton = new QPushButton(tr("&Finished"));
    submitButton->setDefault(true);
    QPushButton *selectAllButton = new QPushButton(tr("&Select all local changes"));
    QPushButton *selectNoneButton = new QPushButton(tr("&Unselect all local changes"));
    QPushButton *quitButton = new QPushButton(tr("&Cancel"));

    QDialogButtonBox *buttonBox = new QDialogButtonBox(Qt::Vertical);
    buttonBox->addButton(submitButton, QDialogButtonBox::ActionRole);
    buttonBox->addButton(selectAllButton, QDialogButtonBox::ActionRole);
    buttonBox->addButton(selectNoneButton, QDialogButtonBox::ActionRole);
    buttonBox->addButton(quitButton, QDialogButtonBox::ActionRole);

    connect(submitButton, SIGNAL(clicked()), this, SLOT(submit())); // this needs to save (submit) all changes from all tables
    connect(selectAllButton, SIGNAL(clicked()), this, SLOT(selectAllLocal()));
    connect(selectNoneButton, SIGNAL(clicked()), this, SLOT(unselectAllLocal()));
    connect(quitButton, SIGNAL(clicked()), this, SLOT(confirmClose()));

    QHBoxLayout *mainLayout = new QHBoxLayout;
    mainLayout->addWidget(tabWidget);
    mainLayout->addWidget(buttonBox);

    setLayout(mainLayout);
    setWindowTitle("Select which conflicting records to use");
}

QList<Image> MergeTables::loadImages(const QString &t, const QString &whereStatement)
{
    QList<Image> images;
    QString selection;
    selection= "SELECT fileName, focalLength, dwc_georeferenceRemarks, dwc_decimalLatitude, "
            "dwc_decimalLongitude, geo_alt, exif_PixelXDimension, exif_PixelYDimension, "
            "dwc_occurrenceRemarks, dwc_geodeticDatum, dwc_coordinateUncertaintyInMeters, "
            "dwc_locality, dwc_countryCode, dwc_stateProvince, dwc_county, dwc_informationWithheld, "
            "dwc_dataGeneralizations, dwc_continent, geonamesAdmin, geonamesOther, dcterms_identifier, "
            "dcterms_modified, dcterms_title, dcterms_description, ac_caption, photographerCode, dcterms_created, "
            "photoshop_Credit, owner, dcterms_dateCopyrighted, dc_rights, xmpRights_Owner, ac_attributionLinkURL, "
            "ac_hasServiceAccessPoint, usageTermsIndex, view, xmp_Rating, foaf_depicts, "
            "suppress FROM " + t + whereStatement;
    QSqlQuery query;
    query.prepare(selection);
    query.exec();
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

        images << image;
    }
    return images;
}

QList<Agent> MergeTables::loadAgents(const QString &t, const QString &whereStatement)
{
    QList<Agent> agents;
    QSqlQuery query;
    query.prepare("SELECT dcterms_identifier, dc_contributor, iri, contactURL, morphbankUserID, "
                  "dcterms_modified, type FROM " + t + whereStatement);
    query.exec();
    while (query.next())
    {
        Agent agent;
        agent.setAgentCode(query.value(0).toString());
        agent.setFullName(query.value(1).toString());
        agent.setAgentURI(query.value(2).toString());
        agent.setContactURL(query.value(3).toString());
        agent.setMorphbankID(query.value(4).toString());
        agent.setLastModified(query.value(5).toString());
        agent.setAgentType(query.value(6).toString());
        agents << agent;
    }
    return agents;
}

QList<Determination> MergeTables::loadDeterminations(const QString &t, const QString &whereStatement)
{
    QList<Determination> determinations;
    QSqlQuery query;
    query.prepare("SELECT dsw_identified, identifiedBy, dwc_dateIdentified, dwc_identificationRemarks, "
                  "tsnID, nameAccordingToID, dcterms_modified, suppress FROM " + t + whereStatement);
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
    return determinations;
}

QList<Organism> MergeTables::loadOrganisms(const QString &t, const QString &whereStatement)
{
    QList<Organism> organisms;
    QSqlQuery query;
    query.prepare("SELECT dcterms_identifier, dwc_establishmentMeans, dcterms_modified, "
                    "dwc_organismRemarks, dwc_collectionCode, dwc_catalogNumber, "
                    "dwc_georeferenceRemarks, dwc_decimalLatitude, dwc_decimalLongitude, geo_alt, "
                    "dwc_organismName, dwc_organismScope, cameo, notes, suppress from " + t + whereStatement);
    query.exec();
    while (query.next())
    {
        Organism organism;
        organism.identifier = query.value(0).toString();
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
    return organisms;
}

QList<Sensu> MergeTables::loadSensu(const QString &t, const QString &whereStatement)
{
    QList<Sensu> sensus;

    QSqlQuery query;
    query.prepare("SELECT dcterms_identifier, dc_creator, tcsSignature, dcterms_title, "
                  "dc_publisher, dcterms_created, iri, dcterms_modified from " + t + whereStatement);
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
    return sensus;
}

QList<Taxa> MergeTables::loadTaxa(const QString &t, const QString &whereStatement)
{
    QList<Taxa> taxa;

    QSqlQuery query;
    query.prepare("SELECT ubioID, dcterms_identifier, dwc_kingdom, dwc_class, "
                  "dwc_order, dwc_family, dwc_genus, dwc_specificEpithet, "
                  "dwc_infraspecificEpithet, dwc_taxonRank, dwc_scientificNameAuthorship, "
                  "dwc_vernacularName, dcterms_modified FROM " + t + whereStatement);
    query.exec();
    while (query.next())
    {
        Taxa taxon;
        taxon.ubioID = query.value(0).toString();
        taxon.identifier = query.value(1).toString();
        taxon.kingdom = query.value(2).toString();
        taxon.className = query.value(3).toString();
        taxon.order = query.value(4).toString();
        taxon.family = query.value(5).toString();
        taxon.genus = query.value(6).toString();
        taxon.species = query.value(7).toString();
        taxon.subspecies = query.value(8).toString();
        taxon.taxonRank = query.value(9).toString();
        taxon.authorship = query.value(10).toString();
        taxon.vernacular = query.value(11).toString();
        taxon.lastModified = query.value(12).toString();
        taxa << taxon;
    }
    return taxa;
}

void MergeTables::findImageDiff()
{
    // Find differences between image tables
    QSqlQuery iDiffQry;
    if (updating)
    {
        iDiffQry.prepare("SELECT fileName, focalLength, dwc_georeferenceRemarks, dwc_decimalLatitude, "
                 "dwc_decimalLongitude, geo_alt, exif_PixelXDimension, exif_PixelYDimension, "
                 "dwc_occurrenceRemarks, dwc_geodeticDatum, dwc_coordinateUncertaintyInMeters, "
                 "dwc_locality, dwc_countryCode, dwc_stateProvince, dwc_county, dwc_informationWithheld, "
                 "dwc_dataGeneralizations, dwc_continent, geonamesAdmin, geonamesOther, dcterms_identifier, "
                 "dcterms_title, dcterms_description, ac_caption, photographerCode, dcterms_created, "
                 "photoshop_Credit, owner, dcterms_dateCopyrighted, dc_rights, xmpRights_Owner, "
                 "ac_attributionLinkURL, ac_hasServiceAccessPoint, usageTermsIndex, view, xmp_Rating, "
                 "foaf_depicts, suppress FROM " + imaT + " WHERE dcterms_modified > '" + databaseVersion + "' "
                 "EXCEPT SELECT fileName, focalLength, dwc_georeferenceRemarks, dwc_decimalLatitude, "
                 "dwc_decimalLongitude, geo_alt, exif_PixelXDimension, exif_PixelYDimension, "
                 "dwc_occurrenceRemarks, dwc_geodeticDatum, dwc_coordinateUncertaintyInMeters, dwc_locality, "
                 "dwc_countryCode, dwc_stateProvince, dwc_county, dwc_informationWithheld, "
                 "dwc_dataGeneralizations, dwc_continent, geonamesAdmin, geonamesOther, dcterms_identifier, "
                 "dcterms_title, dcterms_description, ac_caption, photographerCode, dcterms_created, "
                 "photoshop_Credit, owner, dcterms_dateCopyrighted, dc_rights, xmpRights_Owner, "
                 "ac_attributionLinkURL, ac_hasServiceAccessPoint, usageTermsIndex, view, xmp_Rating, "
                 "foaf_depicts, suppress FROM " + ima2T);
    }
    else
    {
        iDiffQry.prepare("SELECT fileName, focalLength, dwc_georeferenceRemarks, dwc_decimalLatitude, "
                 "dwc_decimalLongitude, geo_alt, exif_PixelXDimension, exif_PixelYDimension, "
                 "dwc_occurrenceRemarks, dwc_geodeticDatum, dwc_coordinateUncertaintyInMeters, "
                 "dwc_locality, dwc_countryCode, dwc_stateProvince, dwc_county, dwc_informationWithheld, "
                 "dwc_dataGeneralizations, dwc_continent, geonamesAdmin, geonamesOther, dcterms_identifier, "
                 "dcterms_title, dcterms_description, ac_caption, photographerCode, dcterms_created, "
                 "photoshop_Credit, owner, dcterms_dateCopyrighted, dc_rights, xmpRights_Owner, "
                 "ac_attributionLinkURL, ac_hasServiceAccessPoint, usageTermsIndex, view, xmp_Rating, "
                 "foaf_depicts, suppress FROM " + imaT +
                 " EXCEPT SELECT fileName, focalLength, dwc_georeferenceRemarks, dwc_decimalLatitude, "
                 "dwc_decimalLongitude, geo_alt, exif_PixelXDimension, exif_PixelYDimension, "
                 "dwc_occurrenceRemarks, dwc_geodeticDatum, dwc_coordinateUncertaintyInMeters, dwc_locality, "
                 "dwc_countryCode, dwc_stateProvince, dwc_county, dwc_informationWithheld, "
                 "dwc_dataGeneralizations, dwc_continent, geonamesAdmin, geonamesOther, dcterms_identifier, "
                 "dcterms_title, dcterms_description, ac_caption, photographerCode, dcterms_created, "
                 "photoshop_Credit, owner, dcterms_dateCopyrighted, dc_rights, xmpRights_Owner, "
                 "ac_attributionLinkURL, ac_hasServiceAccessPoint, usageTermsIndex, view, xmp_Rating, "
                 "foaf_depicts, suppress FROM " + ima2T);
    }
    iDiffQry.exec();

    while (iDiffQry.next())
    {
        imageIDs.append(iDiffQry.value(20).toString());
    }

    QSqlQuery iNewQry;
    if (updating)
    {
        iNewQry.prepare("SELECT t1.dcterms_identifier FROM " + imaT + " t1 LEFT JOIN " + ima2T + " t2 ON "
                    "t2.dcterms_identifier = t1.dcterms_identifier WHERE t2.dcterms_identifier "
                    "IS NULL AND t1.dcterms_modified > '" + databaseVersion + "'");
    }
    else
    {
        iNewQry.prepare("SELECT t1.dcterms_identifier FROM " + imaT + " t1 LEFT JOIN " + ima2T + " t2 ON "
                    "t2.dcterms_identifier = t1.dcterms_identifier WHERE t2.dcterms_identifier "
                    "IS NULL");
    }
    iNewQry.exec();

    while (iNewQry.next())
    {
        QString id = iNewQry.value(0).toString();
        newImageIDs.append(id);
        imageIDs.removeOne(id);
    }
}

void MergeTables::findAgentDiff()
{
    // Find differences between agents tables
    QSqlQuery aDiffQry;
    if (updating)
    {
        aDiffQry.prepare("SELECT dcterms_identifier, dc_contributor, iri, contactURL, "
                 "morphbankUserID, type FROM " + ageT + " WHERE dcterms_modified > '" + databaseVersion + "' "
                 "EXCEPT SELECT dcterms_identifier, dc_contributor, iri, contactURL, morphbankUserID, type "
                 "FROM " + age2T);
    }
    else
    {
        aDiffQry.prepare("SELECT dcterms_identifier, dc_contributor, iri, contactURL, "
                 "morphbankUserID, type FROM " + ageT +
                 " EXCEPT SELECT dcterms_identifier, dc_contributor, iri, contactURL, morphbankUserID, type "
                 "FROM " + age2T);
    }
    aDiffQry.exec();

    while (aDiffQry.next())
    {
        agentIDs.append(aDiffQry.value(0).toString());
    }

    QSqlQuery aNewQry;
    if (updating)
    {
        aNewQry.prepare("SELECT t1.dcterms_identifier FROM " + ageT + " t1 LEFT JOIN " + age2T + " t2 ON "
                    "t2.dcterms_identifier = t1.dcterms_identifier WHERE t2.dcterms_identifier "
                    "IS NULL AND t1.dcterms_modified > '" + databaseVersion + "'");
    }
    else
    {
        aNewQry.prepare("SELECT t1.dcterms_identifier FROM " + ageT + " t1 LEFT JOIN " + age2T + " t2 ON "
                    "t2.dcterms_identifier = t1.dcterms_identifier WHERE t2.dcterms_identifier "
                    "IS NULL");
    }
    aNewQry.exec();

    while (aNewQry.next())
    {
        QString id = aNewQry.value(0).toString();
        newAgentIDs.append(id);
        agentIDs.removeOne(id);
    }
}

void MergeTables::findDeterminationDiff()
{
    // Find differences between determination tables
    QSqlQuery dDiffQry;
    if (updating)
    {
        dDiffQry.prepare("SELECT dsw_identified, identifiedBy, dwc_dateIdentified, dwc_identificationRemarks, "
                 "tsnID, nameAccordingToID, suppress FROM " + detT + " WHERE dcterms_modified > '" + databaseVersion + "' "
                 "EXCEPT SELECT dsw_identified, identifiedBy, dwc_dateIdentified, dwc_identificationRemarks, "
                 "tsnID, nameAccordingToID, suppress FROM " + det2T);
    }
    else
    {
        dDiffQry.prepare("SELECT dsw_identified, identifiedBy, dwc_dateIdentified, dwc_identificationRemarks, "
                 "tsnID, nameAccordingToID, suppress FROM " + detT +
                 " EXCEPT SELECT dsw_identified, identifiedBy, dwc_dateIdentified, dwc_identificationRemarks, "
                 "tsnID, nameAccordingToID, suppress FROM " + det2T);
    }
    dDiffQry.exec();

    while (dDiffQry.next())
    {
        determinationIDs.append(
                    "dsw_identified = '" + dDiffQry.value(0).toString() + "' AND " +
                    "dwc_dateIdentified = '" + dDiffQry.value(2).toString() + "' AND " +
                    "tsnID = '" + dDiffQry.value(4).toString() + "' AND " +
                    "nameAccordingToID = '" + dDiffQry.value(5).toString() + "'"
                    );
    }

    QSqlQuery dNewQry;
    if (updating)
    {
        dNewQry.prepare("SELECT t1.dsw_identified, t1.dwc_dateIdentified, t1.tsnID, t1.nameAccordingToID "
                    "FROM " + detT + " t1 LEFT JOIN " + det2T + " t2 ON "
                    "t2.dsw_identified = t1.dsw_identified AND t2.tsnID = t1.tsnID AND t2.dwc_dateIdentified "
                    "= t1.dwc_dateIdentified AND t2.nameAccordingToID = t1.nameAccordingToID WHERE t2.dsw_identified "
                    "IS NULL AND t2.tsnID IS NULL AND t2.dwc_dateIdentified IS NULL AND t2.nameAccordingToID IS NULL "
                    "AND t1.dcterms_modified > '" + databaseVersion + "'");
    }
    else
    {
        dNewQry.prepare("SELECT t1.dsw_identified, t1.dwc_dateIdentified, t1.tsnID, t1.nameAccordingToID "
                    "FROM " + detT + " t1 LEFT JOIN " + det2T + " t2 ON "
                    "t2.dsw_identified = t1.dsw_identified AND t2.tsnID = t1.tsnID AND t2.dwc_dateIdentified "
                    "= t1.dwc_dateIdentified AND t2.nameAccordingToID = t1.nameAccordingToID WHERE t2.dsw_identified "
                    "IS NULL AND t2.tsnID IS NULL AND t2.dwc_dateIdentified IS NULL AND t2.nameAccordingToID IS NULL");
    }
    dNewQry.exec();

    while (dNewQry.next())
    {
        QString id = "dsw_identified = '" + dNewQry.value(0).toString() + "' AND " +
                     "dwc_dateIdentified = '" + dNewQry.value(1).toString() + "' AND " +
                     "tsnID = '" + dNewQry.value(2).toString() + "' AND " +
                     "nameAccordingToID = '" + dNewQry.value(3).toString() + "'";
        newDeterminationIDs.append(id);
        determinationIDs.removeAll(id);
    }
}

void MergeTables::findOrganismDiff()
{
    // Find differences between organism tables
    QSqlQuery oDiffQry;
    if (updating)
    {
        oDiffQry.prepare("SELECT dcterms_identifier, dwc_establishmentMeans, dwc_organismRemarks, dwc_collectionCode, "
                 "dwc_catalogNumber, dwc_georeferenceRemarks, dwc_decimalLatitude, dwc_decimalLongitude, "
                 "geo_alt, dwc_organismName, dwc_organismScope, cameo, notes, suppress FROM " + orgT + " WHERE "
                 "dcterms_modified > '" + databaseVersion + "' EXCEPT SELECT dcterms_identifier, "
                 "dwc_establishmentMeans, dwc_organismRemarks, dwc_collectionCode, dwc_catalogNumber, "
                 "dwc_georeferenceRemarks, dwc_decimalLatitude, dwc_decimalLongitude, geo_alt, dwc_organismName, "
                 "dwc_organismScope, cameo, notes, suppress FROM " + org2T);
    }
    else
    {
        oDiffQry.prepare("SELECT dcterms_identifier, dwc_establishmentMeans, dwc_organismRemarks, dwc_collectionCode, "
                 "dwc_catalogNumber, dwc_georeferenceRemarks, dwc_decimalLatitude, dwc_decimalLongitude, "
                 "geo_alt, dwc_organismName, dwc_organismScope, cameo, notes, suppress FROM " + orgT +
                 " EXCEPT SELECT dcterms_identifier, "
                 "dwc_establishmentMeans, dwc_organismRemarks, dwc_collectionCode, dwc_catalogNumber, "
                 "dwc_georeferenceRemarks, dwc_decimalLatitude, dwc_decimalLongitude, geo_alt, dwc_organismName, "
                 "dwc_organismScope, cameo, notes, suppress FROM " + org2T);
    }
    oDiffQry.exec();

    while (oDiffQry.next())
    {
        organismIDs.append(oDiffQry.value(0).toString());
    }

    QSqlQuery oNewQry;
    if (updating)
    {
        oNewQry.prepare("SELECT t1.dcterms_identifier FROM " + orgT + " t1 LEFT JOIN " + org2T + " t2 ON "
                    "t2.dcterms_identifier = t1.dcterms_identifier WHERE t2.dcterms_identifier "
                    "IS NULL AND t1.dcterms_modified > '" + databaseVersion + "'");
    }
    else
    {
        oNewQry.prepare("SELECT t1.dcterms_identifier FROM " + orgT + " t1 LEFT JOIN " + org2T + " t2 ON "
                    "t2.dcterms_identifier = t1.dcterms_identifier WHERE t2.dcterms_identifier "
                    "IS NULL");
    }
    oNewQry.exec();

    while (oNewQry.next())
    {
        QString id = oNewQry.value(0).toString();
        newOrganismIDs.append(id);
        organismIDs.removeOne(id);
    }

}

void MergeTables::findSensuDiff()
{
    // Find differences between sensu tables
    QSqlQuery sDiffQry;
    if (updating)
    {
        sDiffQry.prepare("SELECT dcterms_identifier, dc_creator, tcsSignature, dcterms_title, "
                 "dc_publisher, dcterms_created, iri FROM " + senT + " WHERE dcterms_modified > '" + databaseVersion + "' "
                 "EXCEPT SELECT dcterms_identifier, dc_creator, tcsSignature, dcterms_title, "
                 "dc_publisher, dcterms_created, iri FROM " + sen2T);
    }
    else
    {
        sDiffQry.prepare("SELECT dcterms_identifier, dc_creator, tcsSignature, dcterms_title, "
                 "dc_publisher, dcterms_created, iri FROM " + senT +
                 " EXCEPT SELECT dcterms_identifier, dc_creator, tcsSignature, dcterms_title, "
                 "dc_publisher, dcterms_created, iri FROM " + sen2T);
    }
    sDiffQry.exec();

    while (sDiffQry.next())
    {
        sensuIDs.append(sDiffQry.value(0).toString());
    }

    QSqlQuery sNewQry;
    if (updating)
    {
        sNewQry.prepare("SELECT t1.dcterms_identifier FROM " + senT + " t1 LEFT JOIN " + sen2T + " t2 ON "
                        "t2.dcterms_identifier = t1.dcterms_identifier WHERE t2.dcterms_identifier "
                        "IS NULL AND t1.dcterms_modified > '" + databaseVersion + "'");
    }
    else
    {
        sNewQry.prepare("SELECT t1.dcterms_identifier FROM " + senT + " t1 LEFT JOIN " + sen2T + " t2 ON "
                        "t2.dcterms_identifier = t1.dcterms_identifier WHERE t2.dcterms_identifier "
                        "IS NULL");
    }
    sNewQry.exec();

    while (sNewQry.next())
    {
        QString id = sNewQry.value(0).toString();
        newSensuIDs.append(id);
        sensuIDs.removeOne(id);
    }
}

void MergeTables::findTaxaDiff()
{
    // Find differences between taxa tables
    QSqlQuery tDiffQry;
    if (updating)
    {
        tDiffQry.prepare("SELECT ubioID, dcterms_identifier, dwc_kingdom, dwc_class, "
                     "dwc_order, dwc_family, dwc_genus, dwc_specificEpithet, "
                     "dwc_infraspecificEpithet, dwc_taxonRank, dwc_scientificNameAuthorship, "
                     "dwc_vernacularName FROM " + nam2T + " EXCEPT SELECT ubioID, "
                     "dcterms_identifier, dwc_kingdom, dwc_class, dwc_order, dwc_family, "
                     "dwc_genus, dwc_specificEpithet, dwc_infraspecificEpithet, dwc_taxonRank, "
                     "dwc_scientificNameAuthorship, dwc_vernacularName FROM " + namT);
    }
    else
    {
        tDiffQry.prepare("SELECT ubioID, dcterms_identifier, dwc_kingdom, dwc_class, "
                     "dwc_order, dwc_family, dwc_genus, dwc_specificEpithet, "
                     "dwc_infraspecificEpithet, dwc_taxonRank, dwc_scientificNameAuthorship, "
                     "dwc_vernacularName FROM " + namT + " EXCEPT SELECT ubioID, "
                     "dcterms_identifier, dwc_kingdom, dwc_class, dwc_order, dwc_family, "
                     "dwc_genus, dwc_specificEpithet, dwc_infraspecificEpithet, dwc_taxonRank, "
                     "dwc_scientificNameAuthorship, dwc_vernacularName FROM " + nam2T);
    }
    tDiffQry.exec();

    while (tDiffQry.next())
    {
        taxaIDs.append(tDiffQry.value(1).toString());
    }

    QSqlQuery tNewQry;
    if (updating)
    {
        tNewQry.prepare("SELECT t1.dcterms_identifier FROM " + nam2T + " t1 LEFT JOIN " + namT + " t2 ON "
                        "t2.dcterms_identifier = t1.dcterms_identifier WHERE t2.dcterms_identifier "
                        "IS NULL OR t2.dcterms_modified <= '" + databaseVersion + "'");
    }
    else
    {
        tNewQry.prepare("SELECT t1.dcterms_identifier FROM " + namT + " t1 LEFT JOIN " + nam2T + " t2 ON "
                        "t2.dcterms_identifier = t1.dcterms_identifier WHERE t2.dcterms_identifier "
                        "IS NULL");
    }
    tNewQry.exec();

    while (tNewQry.next())
    {
        QString id = tNewQry.value(0).toString();
        if (taxaIDs.contains(id))
        {
            newTaxaIDs.append(id);
            taxaIDs.removeOne(id);
        }
    }
}

void MergeTables::findDifferences()
{
    QSqlDatabase db = QSqlDatabase::database();
    db.transaction();

    if (!onlyTable.isEmpty())
    {
        if (onlyTable == "images")
            findImageDiff();
        else if (onlyTable == "agents")
            findAgentDiff();
        else if (onlyTable == "determinations")
            findDeterminationDiff();
        else if (onlyTable == "organisms")
            findOrganismDiff();
        else if (onlyTable == "sensu")
            findSensuDiff();
        else if (onlyTable == "taxa")
            findTaxaDiff();
    }
    else
    {
        findImageDiff();
        findAgentDiff();
        findDeterminationDiff();
        findOrganismDiff();
        findSensuDiff();
        findTaxaDiff();
    }

    if (!db.commit())
    {
        qDebug() << __LINE__ << "Problem with database transaction";
        db.rollback();
    }
}

void MergeTables::mergeNonConflicts()
{
    QSqlDatabase db = QSqlDatabase::database();
    db.transaction();

    // we have all the information we need to merge/prompt for action
    // first let's update the dcterms_modified so they show up relative to the last_published.xml date
    QStringList tables;
    tables << ageT << detT << imaT << orgT << senT << namT;

    for (auto t : tables)
    {
        QSqlQuery updateModified;
        updateModified.prepare("UPDATE " + t + " SET dcterms_modified = (?) WHERE dcterms_modified > '" + databaseVersion + "'");
        updateModified.addBindValue(modifiedNow());
        updateModified.exec();
    }

    // special case for taxa: merge the full record of 'id' into the (in this case) existing table
    for (auto id : newTaxaIDs)
    {
        QSqlQuery mergeQry;
        if (updating)
            mergeQry.prepare("INSERT OR REPLACE INTO " + namT + " SELECT * FROM " + nam2T + " WHERE dcterms_identifier = (?)");
        else
            mergeQry.prepare("INSERT OR REPLACE INTO " + nam2T + " SELECT * FROM " + namT + " WHERE dcterms_identifier = (?)");
        mergeQry.addBindValue(id);
        mergeQry.exec();
    }
    // the other newWhateverIDs also need to be merged, but INTO tmp_table FROM table, opposite of taxa
    for (auto id : newAgentIDs)
    {
        QSqlQuery mergeQry;
        mergeQry.prepare("INSERT OR REPLACE INTO " + age2T + " SELECT * FROM " + ageT + " WHERE dcterms_identifier = (?)");
        mergeQry.addBindValue(id);
        mergeQry.exec();
    }
    for (auto id : newDeterminationIDs)
    {
        QSqlQuery mergeQry;
        mergeQry.prepare("INSERT OR REPLACE INTO " + det2T + " SELECT * FROM " + detT + " WHERE dcterms_identifier = (?)");
        mergeQry.addBindValue(id);
        mergeQry.exec();
    }
    for (auto id : newImageIDs)
    {
        QSqlQuery mergeQry;
        mergeQry.prepare("INSERT OR REPLACE INTO " + ima2T + " SELECT * FROM " + imaT + " WHERE dcterms_identifier = (?)");
        mergeQry.addBindValue(id);
        mergeQry.exec();
    }
    for (auto id : newOrganismIDs)
    {
        QSqlQuery mergeQry;
        mergeQry.prepare("INSERT OR REPLACE INTO " + org2T + " SELECT * FROM " + orgT + " WHERE dcterms_identifier = (?)");
        mergeQry.addBindValue(id);
        mergeQry.exec();
    }
    for (auto id : newSensuIDs)
    {
        QSqlQuery mergeQry;
        mergeQry.prepare("INSERT OR REPLACE INTO " + sen2T + " SELECT * FROM " + senT + " WHERE dcterms_identifier = (?)");
        mergeQry.addBindValue(id);
        mergeQry.exec();
    }

    if (!db.commit())
    {
        qDebug() << __LINE__ << "Problem with database transaction";
        db.rollback();
    }
}

void MergeTables::alterTables()
{
    // move and remove tables as appropriate
    QStringList deleteTables;
    if (!onlyTable.isEmpty())
    {
        if (onlyTable == "images")
            deleteTables << ima2T;
        else if (onlyTable == "agents")
            deleteTables << age2T;
        else if (onlyTable == "determinations")
            deleteTables << det2T;
        else if (onlyTable == "organisms")
            deleteTables << org2T;
        else if (onlyTable == "sensu")
            deleteTables << sen2T;
        else if (onlyTable == "taxa")
            deleteTables << nam2T;
    }
    else if (updating)
        deleteTables << ageT << detT << imaT << orgT << senT << nam2T;
    else
        deleteTables << ageT << detT << imaT << orgT << senT << namT;

    QSqlDatabase db = QSqlDatabase::database();
    db.transaction();
    for (auto t : deleteTables)
    {
        QSqlQuery dropQry;
        dropQry.prepare("DELETE FROM " + t);
        dropQry.exec();
    }

    QStringList insertQuery;
    if (!onlyTable.isEmpty())
    {
        if (onlyTable == "images")
            insertQuery << ima2T + " SELECT * FROM " + imaT;
        else if (onlyTable == "agents")
            insertQuery << age2T + " SELECT * FROM " + ageT;
        else if (onlyTable == "determinations")
            insertQuery << det2T + " SELECT * FROM " + detT;
        else if (onlyTable == "organisms")
            insertQuery << org2T + " SELECT * FROM " + orgT;
        else if (onlyTable == "sensu")
            insertQuery << sen2T + " SELECT * FROM " + senT;
        else if (onlyTable == "taxa")
            insertQuery << nam2T + " SELECT * FROM " + namT;
    }
    else if (updating)
    {
        insertQuery << ageT + " SELECT * FROM " + age2T << detT + " SELECT * FROM " + det2T <<
                       imaT + " SELECT * FROM " + ima2T << orgT + " SELECT * FROM " + org2T <<
                       senT + " SELECT * FROM " + sen2T;
    }
    else
    {
        insertQuery << ageT + " SELECT * FROM " + age2T << detT + " SELECT * FROM " + det2T <<
                       imaT + " SELECT * FROM " + ima2T << orgT + " SELECT * FROM " + org2T <<
                       senT + " SELECT * FROM " + sen2T << namT + " SELECT * FROM " + nam2T;
    }
    for (auto t : insertQuery)
    {
        QSqlQuery renameQry;
        renameQry.prepare("INSERT INTO " + t);
        renameQry.exec();
    }

    // now clear the tmp_ tables
    deleteTables.clear();
    if (!onlyTable.isEmpty())
    {
        if (onlyTable == "images")
            deleteTables << imaT;
        else if (onlyTable == "agents")
            deleteTables << ageT;
        else if (onlyTable == "determinations")
            deleteTables << detT;
        else if (onlyTable == "organisms")
            deleteTables << orgT;
        else if (onlyTable == "sensu")
            deleteTables << senT;
        else if (onlyTable == "taxa")
            deleteTables << namT;
    }
    else if (updating)
        deleteTables << age2T << det2T << ima2T << org2T << sen2T;
    else
        deleteTables << age2T << det2T << ima2T << org2T << sen2T << nam2T;
    for (auto t : deleteTables)
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
    else
    {
        db.close();
        db.open();
        QSqlQuery query;
        query.exec("VACUUM");
        close();
    }
}

void MergeTables::submit()
{
    if (merging)
        return;

    merging = true;

    // first let's merge the changes that have no conflicts
    mergeNonConflicts();

    QSqlDatabase db = QSqlDatabase::database();
    db.transaction();

    QSqlQuery mergeQry;
    for (int i =0; i < agentsTable->model()->rowCount(); i++)
    {
        if (agentsTable->model()->data(agentsTable->model()->index(i,0),Qt::CheckStateRole) == Qt::Checked)
        {
            QString id = agentsTable->model()->data(agentsTable->model()->index(i,1),Qt::DisplayRole).toString();
            mergeQry.prepare("INSERT OR REPLACE INTO " + age2T + " SELECT * FROM " + ageT + " WHERE dcterms_identifier = (?)");
            mergeQry.addBindValue(id);
            mergeQry.exec();
        }
    }

    QAbstractItemModel *dtm = determinationsTable->model();
    for (int i =0; i < dtm->rowCount(); i++)
    {
        if (dtm->data(dtm->index(i,0),Qt::CheckStateRole) == Qt::Checked)
        {
            QString whereStatement = "dsw_identified = '" + dtm->data(dtm->index(i,1),Qt::DisplayRole).toString() + "' AND " +
                    "dwc_dateIdentified = '" + dtm->data(dtm->index(i,3),Qt::DisplayRole).toString() + "' AND " +
                    "tsnID = '" + dtm->data(dtm->index(i,5),Qt::DisplayRole).toString() + "' AND " +
                    "nameAccordingToID = '" + dtm->data(dtm->index(i,6),Qt::DisplayRole).toString() + "'";
            mergeQry.prepare("INSERT OR REPLACE INTO " + det2T + " SELECT * FROM " + detT + " WHERE " + whereStatement);
            mergeQry.exec();
        }
    }

    for (int i =0; i < imagesTable->model()->rowCount(); i++)
    {
        if (imagesTable->model()->data(imagesTable->model()->index(i,0),Qt::CheckStateRole) == Qt::Checked)
        {
            QString id = imagesTable->model()->data(imagesTable->model()->index(i,21),Qt::DisplayRole).toString();
            mergeQry.prepare("INSERT OR REPLACE INTO " + ima2T + " SELECT * FROM " + imaT + " WHERE dcterms_identifier = (?)");
            mergeQry.addBindValue(id);
            mergeQry.exec();
        }
    }

    // taxa is different from the rest: find the unchecked and transfer from tmp_taxa to taxa
    for (int i =0; i < taxaTable->model()->rowCount(); i++)
    {
        if (taxaTable->model()->data(taxaTable->model()->index(i,0),Qt::CheckStateRole) == Qt::Unchecked)
        {
            QString id = taxaTable->model()->data(taxaTable->model()->index(i,2),Qt::DisplayRole).toString();
            if (updating)
                mergeQry.prepare("INSERT OR REPLACE INTO " + namT + " SELECT * FROM " + nam2T + " WHERE dcterms_identifier = (?)");
            else
                mergeQry.prepare("INSERT OR REPLACE INTO " + nam2T + " SELECT * FROM " + namT + " WHERE dcterms_identifier = (?)");
            mergeQry.addBindValue(id);
            mergeQry.exec();
        }
    }

    for (int i =0; i < organismsTable->model()->rowCount(); i++)
    {
        if (organismsTable->model()->data(organismsTable->model()->index(i,0),Qt::CheckStateRole) == Qt::Checked)
        {
            QString id = organismsTable->model()->data(organismsTable->model()->index(i,1),Qt::DisplayRole).toString();
            mergeQry.prepare("INSERT OR REPLACE INTO " + org2T + " SELECT * FROM " + orgT + " WHERE dcterms_identifier = (?)");
            mergeQry.addBindValue(id);
            mergeQry.exec();
        }
    }

    for (int i =0; i < sensuTable->model()->rowCount(); i++)
    {
        if (sensuTable->model()->data(sensuTable->model()->index(i,0),Qt::CheckStateRole) == Qt::Checked)
        {
            QString id = sensuTable->model()->data(sensuTable->model()->index(i,1),Qt::DisplayRole).toString();
            mergeQry.prepare("INSERT OR REPLACE INTO " + sen2T + " SELECT * FROM " + senT + " WHERE dcterms_identifier = (?)");
            mergeQry.addBindValue(id);
            mergeQry.exec();
        }
    }
    if (!db.commit())
    {
        qDebug() << __LINE__ << "Problem with database transaction";
        db.rollback();
    }
    else
    {
        alterTables();
        emit finished();
    }
}

void MergeTables::selectAllLocal()
{
    for (int i = 0; i < agentsTable->model()->rowCount(); i++)
    {
        if (i % 2 == 1)
        agentsTable->model()->setData(agentsTable->model()->index(i,0),Qt::Checked,Qt::CheckStateRole);
    }

    for (int i = 0; i < determinationsTable->model()->rowCount(); i++)
    {
        if (i % 2 == 1)
        determinationsTable->model()->setData(determinationsTable->model()->index(i,0),Qt::Checked,Qt::CheckStateRole);
    }

    for (int i = 0; i < imagesTable->model()->rowCount(); i++)
    {
        if (i % 2 == 1)
        imagesTable->model()->setData(imagesTable->model()->index(i,0),Qt::Checked,Qt::CheckStateRole);
    }

    // taxa is different from the rest: find the unchecked and transfer from tmp_taxa to taxa
    for (int i = 0; i < taxaTable->model()->rowCount(); i++)
    {
        if (i % 2 == 1)
        taxaTable->model()->setData(taxaTable->model()->index(i,0),Qt::Checked,Qt::CheckStateRole);
    }

    for (int i = 0; i < organismsTable->model()->rowCount(); i++)
    {
        if (i % 2 == 1)
        organismsTable->model()->setData(organismsTable->model()->index(i,0),Qt::Checked,Qt::CheckStateRole);
    }

    for (int i = 0; i < sensuTable->model()->rowCount(); i++)
    {
        if (i % 2 == 1)
        sensuTable->model()->setData(sensuTable->model()->index(i,0),Qt::Checked,Qt::CheckStateRole);
    }
}

void MergeTables::unselectAllLocal()
{
    for (int i = 0; i < agentsTable->model()->rowCount(); i++)
    {
        if (i % 2 == 1)
            agentsTable->model()->setData(agentsTable->model()->index(i,0),Qt::Unchecked,Qt::CheckStateRole);
    }

    for (int i = 0; i < determinationsTable->model()->rowCount(); i++)
    {
        if (i % 2 == 1)
            determinationsTable->model()->setData(determinationsTable->model()->index(i,0),Qt::Unchecked,Qt::CheckStateRole);
    }

    for (int i = 0; i < imagesTable->model()->rowCount(); i++)
    {
        if (i % 2 == 1)
            imagesTable->model()->setData(imagesTable->model()->index(i,0),Qt::Unchecked,Qt::CheckStateRole);
    }

    // taxa is different from the rest: find the unchecked and transfer from tmp_taxa to taxa
    for (int i = 0; i < taxaTable->model()->rowCount(); i++)
    {
        if (i % 2 == 1)
            taxaTable->model()->setData(taxaTable->model()->index(i,0),Qt::Unchecked,Qt::CheckStateRole);
    }

    for (int i = 0; i < organismsTable->model()->rowCount(); i++)
    {
        if (i % 2 == 1)
            organismsTable->model()->setData(organismsTable->model()->index(i,0),Qt::Unchecked,Qt::CheckStateRole);
    }

    for (int i = 0; i < sensuTable->model()->rowCount(); i++)
    {
        if (i % 2 == 1)
            sensuTable->model()->setData(sensuTable->model()->index(i,0),Qt::Unchecked,Qt::CheckStateRole);
    }
}

void MergeTables::confirmClose()
{
    // revert all transactions
    emit canceled();
    close();
}

void MergeTables::resizeEvent(QResizeEvent *)
{
    QSqlQuery qry;
    qry.prepare("INSERT OR REPLACE INTO settings (setting, value) VALUES (?, ?)");
    qry.addBindValue("view.mergetables.location");
    qry.addBindValue(saveGeometry());
    qry.exec();
}

void MergeTables::moveEvent(QMoveEvent *)
{
    QSqlQuery qry;
    qry.prepare("INSERT OR REPLACE INTO settings (setting, value) VALUES (?, ?)");
    qry.addBindValue("view.mergetables.location");
    qry.addBindValue(saveGeometry());
    qry.exec();
}

void MergeTables::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::WindowStateChange)
    {
        bool isMax = false;
        if (windowState() == Qt::WindowMaximized)
            isMax = true;
        QSqlQuery qry;
        qry.prepare("INSERT OR REPLACE INTO settings (setting, value) VALUES (?, ?)");
        qry.addBindValue("view.mergetables.fullscreen");
        qry.addBindValue(isMax);
        qry.exec();
    }
}

void MergeTables::setupLayout()
{
    QSqlDatabase db = QSqlDatabase::database();
    db.transaction();

    QSqlQuery qry;
    qry.prepare("SELECT value FROM settings WHERE setting = (?)");
    qry.addBindValue("view.mergetables.location");
    qry.exec();
    if (qry.next())
    {
        restoreGeometry(qry.value(0).toByteArray());
    }

    bool wasMaximized = false;
    qry.prepare("SELECT value FROM settings WHERE setting = (?)");
    qry.addBindValue("view.mergetables.fullscreen");
    qry.exec();
    if (qry.next())
    {
        wasMaximized = qry.value(0).toBool();
    }

    if (!db.commit())
    {
        qDebug() << "Problem committing changes to database in MergeTables::setupLayout()";
        db.rollback();
    }

    if (wasMaximized)
        this->showMaximized();
}

QString MergeTables::modifiedNow()
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
