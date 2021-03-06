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

#ifndef STARTWINDOW_H
#define STARTWINDOW_H

#include <QWidget>
#include <QTableView>

#include "processnewimages.h"
#include "editexisting.h"
#include "help.h"
#include "managecsvs.h"
#include "mergetables.h"
#include "advancedoptions.h"

namespace Ui {
class StartWindow;
}

class StartWindow : public QWidget
{
    Q_OBJECT

public:
    explicit StartWindow(QWidget *parent = 0);
    ~StartWindow();

private slots:
    void on_addNewImagesButton_clicked();
    void on_helpButton_clicked();
    void on_editExistingRecordsButton_clicked();
    void on_manageCSVsButton_clicked();
    void on_advancedButton_clicked();

    void resizeEvent(QResizeEvent *);
    void changeEvent(QEvent*event);

    void httpFinished();
    bool loadCSVs(const QString folder);

    void resetEditExistingButton();
    void resetManageCSVsButton();
    void resetAdvancedButton();

    void closeAddNew();
    void closeEditExisting();
    void closeManageCSVs();
    void closeAdvanced();

    void on_updatesAvailable_clicked();
    void cancelDownload();
    void cleanup();
    void setupDB();

    void on_checkUpdatesButton_clicked();

private:
    Ui::StartWindow *ui;
    QString appPath;
    bool screenPosLoaded;

    ProcessNewImages *processNewImagesWindow;
    EditExisting *editExistingWindow;
    QPointer<Help> help_w;
    ManageCSVs *manageCSVs;
    AdvancedOptions *advancedOptions;

    QString databaseVersion;
    void checkCSVUpdate();
    void fetchThis();
    void startRequest(QUrl url);
    QList<QUrl> fetchList;
    QUrl url;
    QNetworkAccessManager qnam;
    QNetworkReply *reply;
    QString modifiedNow();
    QString lastPublished; // stores dcterms_modified from GitHub
    MergeTables *updatesTable;
    QMessageBox *downloadingMsg;
    void moveEvent(QMoveEvent *);
    bool downloadingCanceled;
};

#endif // STARTWINDOW_H
