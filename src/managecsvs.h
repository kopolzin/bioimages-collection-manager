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

#ifndef MANAGECSVS_H
#define MANAGECSVS_H

#include <QWidget>

namespace Ui {
class ManageCSVs;
}

class ManageCSVs : public QWidget
{
    Q_OBJECT

public:
    explicit ManageCSVs(QWidget *parent = 0);
    ~ManageCSVs();

signals:
    void windowClosed();

private slots:
    void on_selectFirst_clicked();
    void on_selectSecond_clicked();
    void on_mergeButton_clicked();
    void on_backButton_clicked();
    void cleanupab();
    void cleanuptmp();
    void save();

    void on_selectCSVFolderButton_clicked();
    void on_selectTwoColumnCSV_clicked();
    void on_exportButton_clicked();
    void importMergeQueue();

private:
    Ui::ManageCSVs *ui;

    QStringList selectFiles();
    QStringList convertToNewFormat(QString file);
    QHash<QString,QString> loadHighRes();
    QHash<QString,QString> loadAgents();

    QStringList imageFields;
    QStringList organismFields;
    void parseCSVs(const QStringList fileNames, bool identifierIsFileName, bool hasHeader, bool idInFirstColumn, const QString separator, const QString field);

    QString oldAgent;
    QString oldDeterm;
    QString oldImage;
    QString oldOrgan;
    QString oldSensu;
    QStringList oldHeaders;

    QString agent;
    QString determ;
    QString image;
    QString organ;
    QString sensu;
    QString names;
    QStringList headers;

    QStringList firstFileNames;
    QStringList secondFileNames;
    QString dbLastPublished;
    QStringList selectedCSVs;
    void resizeEvent(QResizeEvent *);
    void moveEvent(QMoveEvent *);
    void changeEvent(QEvent *event);
    bool screenPosLoaded;
};

#endif // MANAGECSVS_H
