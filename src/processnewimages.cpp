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

#include <QFileDialog>
#include <QFileInfo>
#include <QCompleter>
#include <QMessageBox>
#include <QDate>
#include <QTime>
#include <QDateTime>
#include <QTimeZone>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QSqlError>

#include "newagentdialog.h"
#include "startwindow.h"
#include "processnewimages.h"
#include "ui_processnewimages.h"

ProcessNewImages::ProcessNewImages(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ProcessNewImages)
{
    ui->setupUi(this);
    screenPosLoaded = false;
    move(QApplication::desktop()->screen()->rect().center() - rect().center());

#ifdef Q_OS_MAC
    this->setStyleSheet("QLabel{font-size:12px} QComboBox{font-size:12px} QPushButton{font-size:13px}");
    ui->uniqIDLabel->setStyleSheet("font: bold 14px");
    ui->bulkRenameLabel->setStyleSheet("font: bold 14px");
    ui->selectImagesLabel->setStyleSheet("font: bold 14px");
    ui->backButton->setStyleSheet("font-size:13px");
    ui->doneButton->setStyleSheet("font-size:15px");
#endif

    ui->addnewAgentButton->setAutoDefault(true);
    ui->helpButton->setAutoDefault(true);
    ui->selectImagesButton->setAutoDefault(true);
    ui->clearButton->setAutoDefault(true);
    ui->backButton->setAutoDefault(true);
    ui->doneButton->setAutoDefault(true);

    QPointer<QValidator> trailingValidator = new QIntValidator(0, 99, this);
    ui->trailingCharacters->setValidator(trailingValidator);

    QString lastAgent = "";
    QString lastNamespace = "";
    QString lastPhotographer = "";
    QString lastTrailingChars = "5";

    QSqlDatabase db = QSqlDatabase::database();
    db.transaction();

    QSqlQuery qry;
    qry.prepare("SELECT value FROM settings WHERE setting = (?)");
    qry.addBindValue("last.agent");
    qry.exec();
    if (qry.next())
        lastAgent = qry.value(0).toString();

    qry.prepare("SELECT value FROM settings WHERE setting = (?)");
    qry.addBindValue("last.namespace");
    qry.exec();
    if (qry.next())
        lastNamespace = qry.value(0).toString();

    qry.prepare("SELECT value FROM settings WHERE setting = (?)");
    qry.addBindValue("last.photographer");
    qry.exec();
    if (qry.next())
        lastPhotographer = qry.value(0).toString();

    qry.prepare("SELECT value FROM settings WHERE setting = (?)");
    qry.addBindValue("last.trailingchars");
    qry.exec();
    if (qry.next())
        lastTrailingChars = qry.value(0).toString();

    qry.prepare("SELECT value FROM settings WHERE setting = (?)");
    qry.addBindValue("view.addimages.location");
    qry.exec();
    if (qry.next())
        restoreGeometry(qry.value(0).toByteArray());

    bool wasMaximized = false;
    qry.prepare("SELECT value FROM settings WHERE setting = (?)");
    qry.addBindValue("view.addimages.fullscreen");
    qry.exec();
    if (qry.next())
        wasMaximized = qry.value(0).toBool();

    if (wasMaximized)
        this->showMaximized();

    screenPosLoaded = true;

    if (!db.commit())
    {
        qDebug() << "Problem committing changes to database in ProcessNewImages()";
        db.rollback();
    }

    ui->trailingCharacters->setText(lastTrailingChars);

    // load agents from database
    loadAgents();
    // set last selected agent in UI
    setAgent(lastAgent, lastNamespace, lastPhotographer);
}

ProcessNewImages::~ProcessNewImages()
{
    delete ui;
}

void ProcessNewImages::resizeEvent(QResizeEvent *)
{
    if (!screenPosLoaded)
        return;
    QSqlQuery qry;
    qry.prepare("INSERT OR REPLACE INTO settings (setting, value) VALUES (?, ?)");
    qry.addBindValue("view.addimages.location");
    qry.addBindValue(this->saveGeometry());
    qry.exec();
}

void ProcessNewImages::moveEvent(QMoveEvent *)
{
    if (!screenPosLoaded)
        return;
    QSqlQuery qry;
    qry.prepare("INSERT OR REPLACE INTO settings (setting, value) VALUES (?, ?)");
    qry.addBindValue("view.addimages.location");
    qry.addBindValue(saveGeometry());
    qry.exec();
}

void ProcessNewImages::changeEvent(QEvent* event)
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
        qry.addBindValue("view.addimages.fullscreen");
        qry.addBindValue(isMax);
        qry.exec();
    }
}

void ProcessNewImages::on_backButton_clicked()
{
    // discard changes and go back to the start screen
    this->hide();
    emit windowClosed();
}

void ProcessNewImages::on_selectImagesButton_clicked()
{
    if (ui->namespaceBox->currentText().isEmpty())
    {
        QMessageBox msgBox;
        msgBox.setText("Please specify a namespace first.");
        msgBox.exec();
        return;
    }

    if (ui->photographerBox->currentText().isEmpty())
    {
        QMessageBox msgBox;
        msgBox.setText("Please specify a photographer first.");
        msgBox.exec();
        return;
    }
    if (ui->trailingCharacters->text().toInt() < 0)
    {
        QMessageBox msgBox;
        msgBox.setText("Set the number of trailing characters to use from the\n"
                       "filenames as the image identifier to a value of 0 or greater.");
        msgBox.exec();
        return;
    }

    QString appPath = "";

    QSqlDatabase db = QSqlDatabase::database();
    db.transaction();

    QSqlQuery qry;
    qry.prepare("SELECT value FROM settings WHERE setting = (?)");
    qry.addBindValue("path.applicationfolder");
    qry.exec();
    if (qry.next())
        appPath = qry.value(0).toString();

    QString photoFolder = appPath;
    qry.prepare("SELECT value FROM settings WHERE setting = (?)");
    qry.addBindValue("path.photofolder");
    qry.exec();
    if (qry.next())
        photoFolder = qry.value(0).toString();

    if (!db.commit())
    {
        qDebug() << "Problem committing changes to database in ProcessNewImages::on_selectImagesButton_clicked()";
        db.rollback();
    }

    QString nameSpace = ui->namespaceBox->currentText();
    QString photographer = ui->photographerBox->currentText();
    int trailingChars = ui->trailingCharacters->text().toInt();
    QFileDialog dialog(this);
    dialog.setDirectory(photoFolder);
    dialog.setFileMode(QFileDialog::ExistingFiles);
    dialog.setNameFilter("Images (*.jpg *.jpeg)");
    dialog.setViewMode(QFileDialog::Detail);
    if (dialog.exec())
        fileNames = dialog.selectedFiles();

    QStringList filenameCollisions;

    QStringList baseNames;
    QHash<QString,QString> baseToFileHash;
    QStringList containedSpaces;
    foreach(QString f, fileNames)
    {
        QFileInfo fileInfo(f);
        QString base = fileInfo.fileName();
        // don't allow spaces in image filenames
        if (base.contains(" "))
        {
            containedSpaces.append(f);
            fileNames.removeOne(f);
        }
        else
        {
            baseNames.append(base);
            baseToFileHash.insert(base,f);
        }
    }

    QSqlQuery namespaceQuery;
    namespaceQuery.prepare("SELECT fileName, dcterms_identifier FROM images");
    namespaceQuery.exec();

    while (namespaceQuery.next())
    {
        QString ff = namespaceQuery.value(0).toString();
        QString id = namespaceQuery.value(1).toString();
        id.remove("http://bioimages.vanderbilt.edu/");
        id.remove(QRegExp("/.*"));
        if (id != nameSpace)
            continue;
        else if (baseNames.contains(ff))
        {
            filenameCollisions.append(ff);
            baseNames.removeOne(ff);
            fileNames.removeOne(baseToFileHash.value(ff));
        }
    }


    foreach(QString f, fileNames)
    {
        // if the file is already in the "to be added" list, don't add it again
        if (ui->listWidget->findItems(f, Qt::MatchExactly).size() != 0)
            continue;

        QFileInfo fileInfo(f);
        QString base = fileInfo.fileName();

        ui->listWidget->addItem(f);
        nameSpaceHash.insert(f,nameSpace);
        photographerHash.insert(f,photographer);
        trailingHash.insert(f,trailingChars);
        Image newImage;
        newImage.fileAndPath = f;
        newImage.fileName = base;
        // need to create a unique identifier for this image
        //newImage.identifier = "http://bioimages.vanderbilt.edu/" + nameSpace + "/" + identifier;
        newImage.photographerCode = photographer;
    }

    setNumFiles();

    db.transaction();

    qry.prepare("INSERT OR REPLACE INTO settings (setting, value) VALUES (?, ?)");
    qry.addBindValue("path.photofolder");
    qry.addBindValue(dialog.directory().absolutePath());
    qry.exec();

    qry.prepare("INSERT OR REPLACE INTO settings (setting, value) VALUES (?, ?)");
    qry.addBindValue("last.agent");
    qry.addBindValue(ui->agentBox->currentText());
    qry.exec();

    qry.prepare("INSERT OR REPLACE INTO settings (setting, value) VALUES (?, ?)");
    qry.addBindValue("last.namespace");
    qry.addBindValue(ui->namespaceBox->currentText());
    qry.exec();

    qry.prepare("INSERT OR REPLACE INTO settings (setting, value) VALUES (?, ?)");
    qry.addBindValue("last.photographer");
    qry.addBindValue(ui->photographerBox->currentText());
    qry.exec();

    qry.prepare("INSERT OR REPLACE INTO settings (setting, value) VALUES (?, ?)");
    qry.addBindValue("last.trailingchars");
    qry.addBindValue(ui->trailingCharacters->text());
    qry.exec();

    if (!db.commit())
    {
        qDebug() << "Problem committing changes to database in ProcessNewImages::on_selectImagesButton_clicked()";
        db.rollback();
    }

    if (!fileNames.isEmpty())
        ui->doneButton->setFocus();

    if (!filenameCollisions.isEmpty())
    {
        QString filenameCollisionsMerged = filenameCollisions.join("\n");
        QMessageBox msgBox;
        msgBox.setText("The following filenames already exist in the Bioimages database in\n"
                       "the '" + nameSpace + "' namespace. Rename them before trying to add again.\n");
        msgBox.setDetailedText(filenameCollisionsMerged);
        msgBox.exec();
    }
    if (!containedSpaces.isEmpty())
    {
        QString filenameWithSpaces = containedSpaces.join("\n");
        QMessageBox msgBox;
        if (containedSpaces.size() == 1)
        {
            msgBox.setText("The following file was not added. Image filenames must not contain spaces.\n"
                           "Please rename the file, such as by replacing the spaces with underscores (_)\n"
                           "or dashes (-).\n");
        }
        else
        {
            msgBox.setText("The following files were not added. Image filenames must not contain spaces.\n"
                           "Please rename the files, such as by replacing the spaces with underscores (_)\n"
                           "or dashes (-).\n");
        }
        msgBox.setDetailedText(filenameWithSpaces);
        msgBox.exec();
    }
}

void ProcessNewImages::on_clearButton_clicked()
{
    ui->listWidget->clear();
    setNumFiles();
    nameSpaceHash.clear();
    photographerHash.clear();
    images.clear();
}

void ProcessNewImages::setNumFiles()
{
    if (ui->listWidget->count() == 1)
    {
        ui->numFilesLabel->setText("1 file");
    }
    else
    {
        ui->numFilesLabel->setText(QString::number(ui->listWidget->count()) + " files");
    }
}

bool ProcessNewImages::checkExifLocation()
{
    QString appPath = "";
    QSqlQuery qry;
    qry.prepare("SELECT value FROM settings WHERE setting = (?)");
    qry.addBindValue("path.applicationfolder");
    qry.exec();
    if (qry.next())
        appPath = qry.value(0).toString();
    else
        qDebug() << "Unable to load 'appPath' value from Settings table";

    QString exifLocation = "\"" + appPath + "/exiftool/exiftool.exe\"";
    QString exifLocationNoExt = "\"" + appPath + "/exiftool/exiftool\"";

    if (QFile::exists(exifLocation.remove("\"")))
    {
        exifLocation = exifLocation;
    }
    else if (!QFile::exists(exifLocation.remove("\"")) && QFile::exists(exifLocationNoExt.remove("\"")))
    {
        exifLocation = exifLocationNoExt;
    }
    else
    {
        QMessageBox msgBox;
        msgBox.setText("Could not find the ExifTool executable in:\n"
                       + appPath + "/exiftool/");
        msgBox.exec();
        return false;
    }

    qry.prepare("INSERT OR REPLACE INTO settings (setting, value) VALUES (?, ?)");
    qry.addBindValue("path.exiftool");
    qry.addBindValue(exifLocation);
    qry.exec();

    return true;
}

void ProcessNewImages::on_doneButton_clicked()
{
    ui->selectImagesButton->setStyleSheet("border: 1px solid black; border-radius: 6px; min-width: 80px; background-color: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1, stop: 0 #f6f7fa, stop: 1 #dadbde)");

    if (ui->listWidget->count() == 0)
    {
        ui->selectImagesButton->setStyleSheet("border: 2px solid red; border-radius: 6px; min-width: 80px; background-color: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1, stop: 0 #f6f7fa, stop: 1 #dadbde)");

        QMessageBox msgBox;
        msgBox.setText("No images have been selected. Select some first to proceed.");
        msgBox.exec();
        ui->selectImagesButton->setFocus();
        return;
    }
    else if (ui->agentBox->currentText().isEmpty())
    {
        QMessageBox msgBox;
        msgBox.setText("Enter your unique agent identifier first.");
        msgBox.exec();
        return;
    }
    else
    {
        if (!checkExifLocation())
        {
            QMessageBox msgBox;
            msgBox.setText("Exiftool was not found. Please make sure the exiftool executable is\n"
                           "in the exiftool directory within the application directory. You may\n"
                           "also have to remove (-k) from the executable name if it is present.");
            msgBox.exec();
            return;
        }

        QStringList imageFileNames;
        QStringList baseFileNames;
        for (int i=0; i < ui->listWidget->count(); i++)
        {
            QFileInfo fileInfo(ui->listWidget->item(i)->text());
            if (!fileInfo.isFile())
            {
                QMessageBox msgBox;
                msgBox.setText(ui->listWidget->item(i)->text() + " does not exist.");
                msgBox.exec();
                continue;
            }
            imageFileNames.append(ui->listWidget->item(i)->text());
            baseFileNames.append(fileInfo.fileName());
        }

        int duplicates = baseFileNames.removeDuplicates();
        if (duplicates > 0)
        {
            QMessageBox msgBox;
            msgBox.setText("Found duplicate filenames.\n"
                           "Ensure filenames are unique then try again.");
            msgBox.exec();
            return;
        }

        QSqlDatabase db = QSqlDatabase::database();
        db.transaction();

        QSqlQuery qry;
        qry.prepare("INSERT OR REPLACE INTO settings (setting, value) VALUES (?, ?)");
        qry.addBindValue("last.agent");
        qry.addBindValue(ui->agentBox->currentText());
        qry.exec();

        qry.prepare("INSERT OR REPLACE INTO settings (setting, value) VALUES (?, ?)");
        qry.addBindValue("last.namespace");
        qry.addBindValue(ui->namespaceBox->currentText());
        qry.exec();

        qry.prepare("INSERT OR REPLACE INTO settings (setting, value) VALUES (?, ?)");
        qry.addBindValue("last.photographer");
        qry.addBindValue(ui->photographerBox->currentText());
        qry.exec();

        if (!db.commit())
        {
            qDebug() << "Problem committing changes to database in ProcessNewImages::on_doneButton_clicked()";
            db.rollback();
        }

        dataEntry = new DataEntry(imageFileNames,nameSpaceHash,photographerHash,trailingHash,agentHash);
        dataEntry->setAttribute(Qt::WA_DeleteOnClose);
        connect(dataEntry,SIGNAL(windowClosed()),this,SLOT(closeDataEntry()));
        this->hide();
        dataEntry->show();
    }
}

void ProcessNewImages::on_helpButton_clicked()
{
    // open help window
    if (help_w.isNull())
    {
        help_w = new Help();
        help_w->setAttribute(Qt::WA_DeleteOnClose);
        help_w->show();
        help_w->select("Process New Images");
    }
    else
    {
        help_w->show();
        help_w->activateWindow();
        help_w->raise();
    }
}

void ProcessNewImages::on_addnewAgentButton_clicked()
{
    NewAgentDialog agentDialog(agents, this);
    int dialogResult = agentDialog.exec();

    if (dialogResult == QDialog::Rejected)
        return;

    QString agentCode = agentDialog.getAgentCode();
    QString fullName = agentDialog.getFullName();
    QString agentURI = agentDialog.getAgentURI();
    QString contactURL = agentDialog.getContactURL();
    QString morphbankID = agentDialog.getMorphbankID();
    QString agentType = agentDialog.getAgentType();

    QString lastModified = modifiedNow();

    QSqlQuery query;
    query.prepare("INSERT INTO agents (dcterms_identifier, dc_contributor, iri, "
                  "contactURL, morphbankUserID, dcterms_modified, type) "
                  "VALUES (?, ?, ?, ?, ?, ?, ?)");
    query.addBindValue(agentCode);
    query.addBindValue(fullName);
    query.addBindValue(agentURI);
    query.addBindValue(contactURL);
    query.addBindValue(morphbankID);
    query.addBindValue(lastModified);
    query.addBindValue(agentType);
    if (!query.exec()) {
        QMessageBox::critical(0, "", "Agent insertion failed: " + query.lastError().text());
        return;
    }

    agentHash.insert(agentCode, fullName);
    agents.append(agentCode);
    agents.sort(Qt::CaseInsensitive);
    setAgent(agentCode, agentCode, agentCode);

    ui->selectImagesButton->setFocus();
}

QString ProcessNewImages::modifiedNow()
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

void ProcessNewImages::loadAgents()
{
    agentHash.clear();
    agents.clear();

    QSqlQuery query("SELECT dcterms_identifier, dc_contributor from agents");
    while (query.next())
    {
        QString dcterms_identifier = query.value(0).toString();
        QString dc_contributor = query.value(1).toString();
        agents.append(dcterms_identifier);
        agentHash.insert(dcterms_identifier,dc_contributor);
    }

    agents.sort(Qt::CaseInsensitive);
}

void ProcessNewImages::setAgent(QString &agent, QString &nameSpace, QString &photographer)
{
    ui->agentBox->clear();
    ui->agentBox->addItems(agents);
    ui->agentBox->setCurrentText(agent);

    ui->photographerBox->clear();
    ui->photographerBox->addItems(agents);
    ui->photographerBox->setCurrentText(photographer);

    ui->namespaceBox->clear();
    ui->namespaceBox->addItems(agents);
    ui->namespaceBox->setCurrentText(nameSpace);
}

void ProcessNewImages::on_agentBox_currentTextChanged(const QString &arg1)
{
    ui->namespaceBox->setCurrentText(arg1);
}

void ProcessNewImages::on_namespaceBox_currentTextChanged(const QString &arg1)
{
    ui->photographerBox->setCurrentText(arg1);
}

void ProcessNewImages::closeDataEntry()
{
    emit windowClosed();
}
