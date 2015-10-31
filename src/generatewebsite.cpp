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
#include <QMessageBox>
#include <QSqlQuery>

#include "startwindow.h"
#include "generatewebsite.h"
#include "ui_generatewebsite.h"

GenerateWebsite::GenerateWebsite(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::GenerateWebsite)
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

    QSqlQuery qry;
    qry.prepare("SELECT value FROM settings WHERE setting = (?)");
    qry.addBindValue("path.photofolder");
    qry.exec();
    if (qry.next())
        photoFolder = qry.value(0).toString();

    baseFolder = "";
}

GenerateWebsite::~GenerateWebsite()
{
    delete ui;
}

QStringList GenerateWebsite::selectImages()
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

void GenerateWebsite::on_selectImagesButton_clicked()
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

void GenerateWebsite::on_baseFolderButton_clicked()
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

void GenerateWebsite::loadAgents()
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

void GenerateWebsite::on_resizeImagesButton_clicked()
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
        int wid = fullSize.width();
        int hei = fullSize.height();

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

void GenerateWebsite::on_backButton_clicked()
{
    this->hide();
    emit windowClosed();
}
