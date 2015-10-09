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
#include <QMessageBox>
#include <QPointer>

#include "geonamesother.h"
#include "ui_geonamesother.h"

GeonamesOther::GeonamesOther(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::GeonamesOther)
{
    ui->setupUi(this);

    QPointer<QValidator> IDvalidator = new QDoubleValidator(0, 9999999999, 0, this);
    ui->newID->setValidator(IDvalidator);
    ui->idToSet->setValidator(IDvalidator);

    ui->linkToGeonames->setOpenExternalLinks(true);
    populateDropdown();
}

void GeonamesOther::populateDropdown()
{
    // Populate dropdown list from "geonames_other" table
    QSqlQuery query;
    query.prepare("SELECT dcterms_identifier, description FROM geonames_other");
    query.exec();

    QStringList descriptions;
    QStringList ids;
    descToID.clear();
    while (query.next())
    {
        QString id = query.value(0).toString();
        QString desc = query.value(1).toString();
        ids.append(id);
        descriptions.append(desc);
        descToID.insert(desc,id);
    }
    descriptions.sort(Qt::CaseInsensitive);

    ui->selectByDescription->clear();
    ui->selectByDescription->addItems(descriptions);

    // if the dropdown changes the idToSet/descToSet must be refreshed
    QString currentlySelected = ui->selectByDescription->currentText();
    if (descToID.contains(currentlySelected))
    {
        ui->idToSet->setText(descToID.value(currentlySelected));
        ui->descToSet->setText(currentlySelected);
    }
    else
    {
        ui->idToSet->clear();
        ui->descToSet->clear();
    }
}

GeonamesOther::~GeonamesOther()
{
    delete ui;
}

QString GeonamesOther::getSelectedID()
{
    return selectedID;
}

void GeonamesOther::on_addNewPairButton_clicked()
{
    QString newID = ui->newID->text().trimmed();
    QString newDesc = ui->newDesc->text().trimmed();
    if (newID.isEmpty() || newDesc.isEmpty())
    {
        QMessageBox msgBox;
        msgBox.setText("Please add both a Geonames Other ID and a description.\n");
        msgBox.exec();
        return;
    }

    QSqlQuery insert;
    insert.prepare("INSERT OR REPLACE INTO geonames_other (dcterms_identifier, description) VALUES ((?), (?))");
    insert.addBindValue(newID);
    insert.addBindValue(newDesc);
    insert.exec();

    populateDropdown();
    ui->selectByDescription->setCurrentText(newDesc);
    ui->idToSet->setText(newID);
    ui->descToSet->setText(newDesc);
}

void GeonamesOther::on_selectByDescription_currentIndexChanged(const QString &arg1)
{
    if (descToID.contains(arg1))
    {
        QString id = descToID.value(arg1);
        ui->idToSet->setText(id);
        ui->descToSet->setText(arg1);
    }
    else
    {
        ui->idToSet->clear();
        ui->descToSet->clear();
    }
}

void GeonamesOther::on_deleteButton_clicked()
{
    QString desc = ui->selectByDescription->currentText();
    if (desc.isEmpty())
        return;
    if (!descToID.contains(desc))
        return;
    QSqlQuery remove;
    remove.prepare("DELETE FROM geonames_other WHERE description = (?)");
    remove.addBindValue(desc);
    remove.exec();
    populateDropdown();
}

void GeonamesOther::on_okButton_clicked()
{
    selectedID = ui->idToSet->text();
    accept();
}

void GeonamesOther::on_cancelButton_clicked()
{
    reject();
}
