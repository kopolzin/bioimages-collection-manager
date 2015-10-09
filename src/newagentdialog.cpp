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
#include <QDebug>
#include "newagentdialog.h"
#include "ui_newagentdialog.h"

NewAgentDialog::NewAgentDialog(const QStringList &importAgents, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::NewAgentDialog)
{
    ui->setupUi(this);

    existingAgents = importAgents;

    QStringList agentTypes;
    agentTypes << "Person" << "Group" << "Organization";
    ui->agentTypeBox->addItems(agentTypes);
    ui->agentTypeBox->setCurrentText("Person");
}

QString NewAgentDialog::getAgentCode()
{
    return agentCode;
}

QString NewAgentDialog::getFullName()
{
    return fullName;
}

QString NewAgentDialog::getAgentURI()
{
    return agentURI;
}

QString NewAgentDialog::getContactURL()
{
    return contactURL;
}

QString NewAgentDialog::getMorphbankID()
{
    return morphbankID;
}

QString NewAgentDialog::getAgentType()
{
    return agentType;
}

NewAgentDialog::~NewAgentDialog()
{
    delete ui;
}

void NewAgentDialog::on_addButton_clicked()
{
    ui->agentCode->setStyleSheet("border: 2px solid white");
    ui->fullName->setStyleSheet("border: 2px solid white");
    ui->agentURI->setStyleSheet("border: 2px solid white");
    ui->contactURL->setStyleSheet("border: 2px solid white");

    ui->agentCode->setText(ui->agentCode->text().trimmed());
    ui->fullName->setText(ui->fullName->text().trimmed());
    ui->agentURI->setText(ui->agentURI->text().trimmed());
    ui->contactURL->setText(ui->contactURL->text().trimmed());
    ui->morphbankID->setText(ui->morphbankID->text().trimmed());

    agentCode = ui->agentCode->text();
    fullName = ui->fullName->text();
    agentURI = ui->agentURI->text();
    contactURL = ui->contactURL->text();
    morphbankID = ui->morphbankID->text();
    agentType = ui->agentTypeBox->currentText();

    // highlight empty boxes red if they're required information
    bool redo = false;
    if (agentCode.isEmpty())
    {
        ui->agentCode->setStyleSheet("border: 2px solid red");
        redo = true;
    }
    else if (agentCode.contains(" "))
    {
        ui->agentCode->setStyleSheet("border: 2px solid red");
        redo = true;
        ui->agentCode->setText(agentCode.remove(" "));
        QMessageBox msgBox;
        msgBox.setText("The agent code cannot contain spaces. The spaces\n"
                       "have been automatically removed. Please review the\n"
                       "altered agent code before proceeding.");
        msgBox.exec();
        return;
    }
    if (fullName.isEmpty())
    {
        ui->fullName->setStyleSheet("border: 2px solid red");
        redo = true;
    }
    if (agentURI.isEmpty())
    {
        ui->agentURI->setStyleSheet("border: 2px solid red");
        redo = true;
    }
    else if (!agentURI.contains("http://"))
    {
        if (agentURI.contains("orcid.org/"))
            agentURI.prepend("http://");
        else
            agentURI.prepend("http://orcid.org/");
    }
    if (contactURL.isEmpty())
    {
        ui->contactURL->setStyleSheet("border: 2px solid red");
        redo = true;
    }

    if (existingAgents.contains(agentCode, Qt::CaseInsensitive))
    {
        ui->agentCode->setStyleSheet("border: 2px solid red");
        QMessageBox msgBox;
        msgBox.setText("That unique agent code already exists.\n"
                       "Please create a different one.");
        msgBox.exec();
        return;
    }
    if (redo)
        return;

    accept();
}

void NewAgentDialog::on_cancelButton_clicked()
{
    reject();
}

void NewAgentDialog::on_fullName_textEdited(const QString &arg1)
{
    // Assign agent code based on their full name, i.e. last name + first name initial
    QStringList splitName = arg1.split(" ");
    if (splitName.length() <= 1)
        return;
    else if (splitName.last().isEmpty())
        return;
    else
    {
        if (splitName.first().length() < 1)
            return;
        QString suggestedAgentCode = splitName.last() + splitName.first().at(0);
        if (existingAgents.contains(suggestedAgentCode, Qt::CaseInsensitive))
        {
            if (splitName.first().length() < 2)
                return;
            suggestedAgentCode += splitName.first().at(1);
            if (existingAgents.contains(suggestedAgentCode, Qt::CaseInsensitive))
            {
                return;
            }
        }

        ui->agentCode->setText(suggestedAgentCode.toLower());
    }
}

