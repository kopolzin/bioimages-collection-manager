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

#ifndef PROCESSNEWIMAGES_H
#define PROCESSNEWIMAGES_H

#include <QMainWindow>
#include <QProcess>
#include <memory>
#include "agent.h"
#include "dataentry.h"
#include "help.h"

namespace Ui {
class ProcessNewImages;
}

class ProcessNewImages : public QMainWindow
{
    Q_OBJECT

public:
    explicit ProcessNewImages(QWidget *parent = 0);
    ~ProcessNewImages();

signals:
    void windowClosed();

private slots:
    void on_backButton_clicked();
    void on_selectImagesButton_clicked();
    void on_clearButton_clicked();
    void on_doneButton_clicked();
    void on_helpButton_clicked();
    void on_addnewAgentButton_clicked();
    void on_agentBox_currentTextChanged(const QString &arg1);
    void on_namespaceBox_currentTextChanged(const QString &arg1);
    void closeDataEntry();

private:
    Ui::ProcessNewImages *ui;
    QPointer<Help> help_w;

    QStringList agents;
    QList<Agent> loadedAgents;
    QHash<QString, QString> photographerHash;
    QList<Image> images;
    QStringList fileNames;
    QHash<QString,QString> agentHash;
    QHash<QString,QString> nameSpaceHash;
    QHash<QString,int> trailingHash;

    void loadAgents();
    void setAgent(QString &agent, QString &nameSpace, QString &photographer);
    void setNumFiles();
    bool checkExifLocation();
    QPointer<DataEntry> dataEntry;
    QString modifiedNow();
};

#endif // PROCESSNEWIMAGES_H
