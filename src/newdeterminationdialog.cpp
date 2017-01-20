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

#include <QDate>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QListView>
#include <QMessageBox>
#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>
#include <QDate>
#include <QTime>

#include "sensu.h"
#include "newdeterminationdialog.h"
#include "newsensudialog.h"
#include "ui_newdeterminationdialog.h"

NewDeterminationDialog::NewDeterminationDialog(const QString &organismID, const QStringList &genera, const QStringList &commonNames, const QStringList &families, const QStringList &agents, const QStringList &sensus, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::NewDeterminationDialog)
{
    ui->setupUi(this);

    QRegExp dateRX("\\d{4}-\\d{2}-\\d{2}");
    QPointer<QValidator> dateValidator = new QRegExpValidator(dateRX, this);
    ui->dateIdentified->setValidator(dateValidator);

    sensuAdded = false;

    dsw_identified = organismID;
    allGenera = genera;
    allCommonNames = commonNames;
    allSensus = sensus;
    allFamilies = families;

    completeGenera = new QCompleter(allGenera, this);
    completeGenera->setCaseSensitivity(Qt::CaseInsensitive);
    completeGenera->popup()->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    completeGenera->popup()->setMinimumWidth(300);
    completeGenera->popup()->setMinimumHeight(200);
    completeGenera->setFilterMode(Qt::MatchContains);
    ui->genusSearch->setCompleter(0);

    completeCommonNames = new QCompleter(allCommonNames, this);
    completeCommonNames->setCaseSensitivity(Qt::CaseInsensitive);
    completeCommonNames->popup()->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    completeCommonNames->popup()->setMinimumWidth(300);
    completeCommonNames->popup()->setMinimumHeight(167);
    completeCommonNames->setFilterMode(Qt::MatchContains);
    ui->commonNameSearch->setCompleter(0);

    completeFamilies = new QCompleter(allFamilies, this);
    completeFamilies->setCaseSensitivity(Qt::CaseInsensitive);
    completeFamilies->popup()->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    completeFamilies->popup()->setMinimumWidth(300);
    completeFamilies->popup()->setMinimumHeight(134);
    completeFamilies->setFilterMode(Qt::MatchContains);
    ui->familySearch->setCompleter(0);

    QString agent = "";
    QSqlQuery qry;
    qry.prepare("SELECT value FROM settings WHERE setting = (?)");
    qry.addBindValue("last.agent");
    qry.exec();
    if (qry.next())
        agent = qry.value(0).toString();

    QPointer<QCompleter> completeAgent = new QCompleter(agents, this);
    completeAgent->setCaseSensitivity(Qt::CaseInsensitive);
    ui->identifiedBy->setCompleter(completeAgent);
    ui->identifiedBy->addItems(agents);
    ui->identifiedBy->setCurrentText(agent);

    allSensus.sort(Qt::CaseInsensitive);
    completeSensu = new QCompleter(allSensus, this);
    completeSensu->setCaseSensitivity(Qt::CaseInsensitive);
    completeSensu->popup()->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    completeSensu->popup()->setMinimumWidth(300);
    completeSensu->setFilterMode(Qt::MatchContains);
    ui->nameAccordingToID->setCompleter(completeSensu);
    ui->nameAccordingToID->addItems(allSensus);
    ui->nameAccordingToID->setCurrentText("");
    ui->nameAccordingToID->view()->setMinimumWidth(500);
    ui->nameAccordingToID->view()->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    QDate date;
    dwc_dateIdentified = date.currentDate().toString("yyyy-MM-dd");
    ui->dateIdentified->setText(dwc_dateIdentified);
}

NewDeterminationDialog::~NewDeterminationDialog()
{
    delete ui;
}

Determination NewDeterminationDialog::getDetermination()
{
    Determination d;
    d.identified = dsw_identified;
    d.identifiedBy = identifiedBy;
    d.dateIdentified = dwc_dateIdentified;
    d.identificationRemarks = dwc_identificationRemarks;
    d.tsnID = tsnID;
    d.nameAccordingToID = nameAccordingToID;
    d.lastModified = dcterms_modified;
    d.suppress = suppress;

    return d;
}

bool NewDeterminationDialog::newSensu()
{
    return sensuAdded;
}

void NewDeterminationDialog::on_genusSearch_textChanged(const QString &arg1)
{
    if (arg1.length() > 0 && arg1.length() < 3)
    {
        ui->genusSearch->setCompleter(0);
    }
    else
    {
        ui->genusSearch->setCompleter(completeGenera);
    }

    // If the current text isn't recognized as a genus, do not set other taxonomic names
    if (!allGenera.contains(arg1,Qt::CaseInsensitive))
    {
        return;
    }

    tsnID = arg1.split(" ").last();
    tsnID.remove("(");
    tsnID.remove(")");
    ui->tsnID->setText(tsnID);
    setTaxaFromTSNID();
}

void NewDeterminationDialog::on_commonNameSearch_textChanged(const QString &arg1)
{
    if (arg1.length() > 0 && arg1.length() < 3)
    {
        ui->commonNameSearch->setCompleter(0);
    }
    else
    {
        ui->commonNameSearch->setCompleter(completeCommonNames);
    }

    // If the current text isn't recognized as a common name, do not set other taxonomic names
    if (!allCommonNames.contains(arg1,Qt::CaseInsensitive))
        return;

    tsnID = arg1.split(" ").last();
    tsnID.remove("(");
    tsnID.remove(")");
    ui->tsnID->setText(tsnID);
    setTaxaFromTSNID();
}

void NewDeterminationDialog::on_familySearch_textChanged(const QString &arg1)
{
    if (arg1.length() > 0 && arg1.length() < 3)
    {
        ui->familySearch->setCompleter(0);
    }
    else
    {
        ui->familySearch->setCompleter(completeFamilies);
    }

    // If the current text isn't recognized as a family name, do not set other taxonomic names
    if (!allFamilies.contains(arg1,Qt::CaseInsensitive))
        return;

    tsnID = arg1.split(" ").last();
    tsnID.remove("(");
    tsnID.remove(")");
    ui->tsnID->setText(tsnID);
    setTaxaFromTSNID();
}

void NewDeterminationDialog::on_identifiedBy_currentTextChanged(const QString &arg1)
{
    identifiedBy = arg1;
}

void NewDeterminationDialog::on_dateIdentified_textEdited(const QString &arg1)
{
    dwc_dateIdentified = arg1;
}

void NewDeterminationDialog::on_nameAccordingToID_currentTextChanged(const QString &arg1)
{
    nameAccordingToID = arg1;
    ui->nameAccordingToID->setToolTip(arg1);
}

void NewDeterminationDialog::on_identificationRemarks_textEdited(const QString &arg1)
{
    dwc_identificationRemarks = arg1;
}

void NewDeterminationDialog::setTaxaFromTSNID()
{
    if (tsnID.isEmpty())
        return;

    QSqlQuery nQuery;
    nQuery.setForwardOnly(true);
    nQuery.prepare("SELECT dwc_kingdom, dwc_class, dwc_order, dwc_family, dwc_genus, dwc_specificEpithet, dwc_infraspecificEpithet, dwc_taxonRank, dwc_vernacularName from taxa where dcterms_identifier = (?) LIMIT 1");
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
        qDebug() << "In setTaxaFromTSN(): tsnID doesn't exist in USDA list. Allow the user to input all necessary information";
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

void NewDeterminationDialog::on_cancelButton_clicked()
{
    reject();
}

void NewDeterminationDialog::on_doneButton_clicked()
{
    ui->identifiedBy->setStyleSheet("border: 2px solid white");
    ui->tsnID->setStyleSheet("border: 2px solid white");

    // highlight empty boxes red if they're required information
    bool redo = false;
    if (identifiedBy.isEmpty())
    {
        ui->identifiedBy->setStyleSheet("border: 2px solid red");
        redo = true;
    }
    if (tsnID.isEmpty())
    {
        ui->tsnID->setStyleSheet("border: 2px solid red");
        redo = true;
    }
    if (redo)
        return;
    QSqlQuery query;
    query.setForwardOnly(true);
    query.prepare("SELECT 1 FROM determinations WHERE dsw_identified = (?) "
                  "AND identifiedBy = (?) AND tsnID = (?)");
    query.addBindValue(dsw_identified);
    query.addBindValue(identifiedBy);
    query.addBindValue(tsnID);
    query.exec();
    if (query.next())
    {
        ui->tsnID->setStyleSheet("border: 2px solid red");
        QMessageBox msgBox;
        msgBox.setText("That combination of dsw_identified, identifiedBy and tsnID\n"
                       "already exists in the database. If you wish to edit that\n"
                       "record, please exit the Add New Determination dialog.");
        msgBox.exec();
        return;
    }

    if (nameAccordingToID.isEmpty())
        nameAccordingToID = "nominal";
    accept();
}

void NewDeterminationDialog::on_newSensuButton_clicked()
{
    NewSensuDialog sensuDialog(allSensus, this);
    int dialogResult = sensuDialog.exec();

    if (dialogResult == QDialog::Rejected)
        return;

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

    QString now = currentDateTime + timezoneOffset;

    Sensu s = sensuDialog.getSensu();
    s.lastModified = now;

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

    sensuAdded = true;
    QString sensuDisplay = s.title + " (" + s.identifier + ")";
    allSensus.append(sensuDisplay);
    allSensus.sort(Qt::CaseInsensitive);

    ui->nameAccordingToID->clear();
    ui->nameAccordingToID->addItems(allSensus);
    ui->nameAccordingToID->setCurrentText(sensuDisplay);
    ui->nameAccordingToID->view()->setMinimumWidth(500);
    ui->nameAccordingToID->view()->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
}

void NewDeterminationDialog::on_manualEntry_clicked()
{
    QDialog taxaDialog(this);
    // Use a layout allowing to have a label next to each field
    QFormLayout taxaForm(&taxaDialog);

    QLabel *info = new QLabel(this);
    info->setTextFormat(Qt::RichText);
    info->setOpenExternalLinks(true);
    info->setText("Enter as much taxonomic information as possible. The tsnID is particularly<br>"
                 "important and can be obtained at <a href='http://www.itis.gov/'>http://www.itis.gov/</a>. If the organism has<br>"
                 "no tsnID there, please use a placeholder value in the interim, e.g. place<br>"
                 "an 'x' in front of its ubioID and use that as the tsnID (if ubioID is 5767885<br>"
                 "and it has no tsnID, set its tsnID to x5767885).");
    // Add some text above the fields
    taxaForm.addRow(info);

    // Add the lineEdits with their respective labels
    QPointer<QLineEdit> tsnIDInput = new QLineEdit(&taxaDialog);
    taxaForm.addRow("tsnID", tsnIDInput);

    QPointer<QLineEdit> taxonRankInput = new QLineEdit(&taxaDialog);
    taxaForm.addRow("Taxon Rank", taxonRankInput);

    QPointer<QLineEdit> kingdomInput = new QLineEdit(&taxaDialog);
    taxaForm.addRow("Kingdom", kingdomInput);

    QPointer<QLineEdit> classInput = new QLineEdit(&taxaDialog);
    taxaForm.addRow("Class", classInput);

    QPointer<QLineEdit> orderInput = new QLineEdit(&taxaDialog);
    taxaForm.addRow("Order", orderInput);

    QPointer<QLineEdit> familyInput = new QLineEdit(&taxaDialog);
    taxaForm.addRow("Family", familyInput);

    QPointer<QLineEdit> genusInput = new QLineEdit(&taxaDialog);
    taxaForm.addRow("Genus", genusInput);

    QPointer<QLineEdit> speciesInput = new QLineEdit(&taxaDialog);
    taxaForm.addRow("Specific\nEpithet", speciesInput);

    QPointer<QLineEdit> infraInput = new QLineEdit(&taxaDialog);
    taxaForm.addRow("Infraspecific\nEpithet", infraInput);

    QPointer<QLineEdit> vernacularInput = new QLineEdit(&taxaDialog);
    taxaForm.addRow("Common Name", vernacularInput);

    QPointer<QLineEdit> ubioIDInput = new QLineEdit(&taxaDialog);
    taxaForm.addRow("ubioID", ubioIDInput);

    QPointer<QLineEdit> authorshipInput = new QLineEdit(&taxaDialog);
    taxaForm.addRow("Scientific Name\nAuthorship", authorshipInput);

    // Add some standard buttons (Cancel/Ok) at the bottom of the dialog
    QDialogButtonBox buttonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, &taxaDialog);
    taxaForm.addRow(&buttonBox);
    QObject::connect(&buttonBox, SIGNAL(accepted()), &taxaDialog, SLOT(accept()));
    QObject::connect(&buttonBox, SIGNAL(rejected()), &taxaDialog, SLOT(reject()));

    // Show the dialog as modal
    // If the user didn't dismiss the dialog, do something with the fields
    if (taxaDialog.exec() == QDialog::Accepted) {


        if (!tsnIDInput->text().isEmpty())
        {
            QSqlQuery uniqueQry;
            uniqueQry.prepare("SELECT * FROM taxa WHERE dcterms_identifier = (?) LIMIT 1");
            uniqueQry.addBindValue(tsnIDInput->text());
            uniqueQry.exec();

            if (uniqueQry.next())
            {
                QMessageBox msgBox;
                msgBox.setText("Record discarded: tsnID already exists.");
                msgBox.exec();
            }
            else
            {
                tsnID = tsnIDInput->text().trimmed();
                ui->tsnID->setText(tsnID);

                QSqlQuery qry;
                qry.prepare("INSERT INTO taxa (ubioID, dcterms_identifier, dwc_kingdom, dwc_class, "
                            "dwc_order, dwc_family, dwc_genus, dwc_specificEpithet, dwc_infraspecificEpithet, "
                            "dwc_taxonRank, dwc_scientificNameAuthorship, dwc_vernacularName, dcterms_modified) "
                            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
                qry.addBindValue(ubioIDInput->text().trimmed());
                qry.addBindValue(tsnID);
                qry.addBindValue(kingdomInput->text().trimmed());
                qry.addBindValue(classInput->text().trimmed());
                qry.addBindValue(orderInput->text().trimmed());
                qry.addBindValue(familyInput->text().trimmed());
                qry.addBindValue(genusInput->text().trimmed());
                qry.addBindValue(speciesInput->text().trimmed());
                qry.addBindValue(infraInput->text().trimmed());
                qry.addBindValue(taxonRankInput->text().trimmed().toLower());
                qry.addBindValue(authorshipInput->text().trimmed());
                qry.addBindValue(vernacularInput->text().trimmed());
                qry.addBindValue(modifiedNow());
                qry.exec();

                setTaxaFromTSNID();
            }
        }
        else
        {
            QMessageBox msgBox;
            msgBox.setText("Record discarded: tsnID cannot be empty.");
            msgBox.exec();
        }
    }
}

QString NewDeterminationDialog::modifiedNow()
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
