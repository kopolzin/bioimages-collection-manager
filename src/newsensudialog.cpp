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

#include "newsensudialog.h"
#include "ui_newsensudialog.h"

NewSensuDialog::NewSensuDialog(const QStringList &existingSensus, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::NewSensuDialog)
{
    ui->setupUi(this);

    sensus = existingSensus;
}

NewSensuDialog::~NewSensuDialog()
{
    delete ui;
}

Sensu NewSensuDialog::getSensu()
{
    Sensu s;
    s.identifier = identifier;
    s.creator = author;
    s.tcsSignature = citation;
    s.title = name;
    s.publisher = publisher;
    s.dcterms_created = date;
    s.iri = identifierURL;
    s.lastModified = changed;

    return s;
}

void NewSensuDialog::on_addButton_clicked()
{
    ui->fullTitle->setStyleSheet("border: 2px solid white");
    ui->identifier->setStyleSheet("border: 2px solid white");

    name = ui->fullTitle->text().trimmed();
    identifier = ui->identifier->text().trimmed().remove(" ");
    author = ui->author->text().trimmed();
    citation = ui->citation->text().trimmed();
    publisher = ui->publisher->text().trimmed();
    date = ui->date->text().trimmed();
    identifierURL = ui->iri->text().trimmed();

    // highlight empty boxes red if they're required information
    bool redo = false;
    if (name.isEmpty())
    {
        ui->fullTitle->setStyleSheet("border: 2px solid red");
        redo = true;
    }
    if (identifier.isEmpty())
    {
        ui->identifier->setStyleSheet("border: 2px solid red");
        redo = true;
    }
    if (redo)
        return;

    if (sensus.contains(identifier))
    {
        ui->identifier->setStyleSheet("border: 2px solid red");
        QMessageBox msgBox;
        msgBox.setText("That identifier is already being used. Please pick another.");
        msgBox.exec();
        return;
    }
    accept();
}

void NewSensuDialog::on_cancelButton_clicked()
{
    reject();
}
