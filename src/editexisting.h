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

#ifndef EDITEXISTING_H
#define EDITEXISTING_H

#include <QMainWindow>
#include <QProcess>

#include "agent.h"
#include "determination.h"
#include "image.h"
#include "organism.h"
#include "sensu.h"
#include "dataentry.h"
#include "help.h"

namespace Ui {
class EditExisting;
}

class EditExisting : public QMainWindow
{
    Q_OBJECT

public:
    explicit EditExisting(QWidget *parent = 0);
    void setup();
    ~EditExisting();

signals:
    void windowClosed();

private slots:
    void on_backButton_clicked();
    void on_selectImageLocationButton_clicked();
    void on_doneButton_clicked();
    void on_addnewAgentButton_clicked();
    void on_agentBox_activated(const QString &arg1);
    void on_loadAllImagesCheckbox_toggled(bool loadEveryonesChecked);
    void on_loadHistoricalRecordsCheckbox_toggled(bool loadAllHistoricalChecked);
    void closeDataEntry();
    void on_showHideButton_clicked();
    void on_rootFolderCheckbox_toggled(bool checked);
    void on_clearCache_clicked();

private:
    Ui::EditExisting *ui;
    QString CSVFolder;

    int numImagesRef = 0;
    int numImagesFound = 0;
    bool pauseLoading;

    QHash<QString,QString> agentHash;
    QHash<QString,QString> photographerHash;
    QString photoFolder;
    QStringList fileNames;
    QStringList fullPathFileNames;
    void setNumFiles();
    bool checkExifLocation();
    QPointer<DataEntry> dataEntry;

    QStringList agents;
    QList<Agent> loadedAgents;
    QList<Agent> newAgents;
    void setAgent(QString &agent);

    QList<Determination> determinations;
    QList<Image> images;
    QList<Organism> organisms;
    QList<Sensu> sensus;

    void loadAgents();
    void loadDeterminations();
    void loadImages();
    void loadOrganisms();
    void loadSensu();

    QString modifiedNow();
    void findFilenamesByAgent();
    QString dbLastPublished;
    void findMatchingFiles();
};

#endif // EDITEXISTING_H
