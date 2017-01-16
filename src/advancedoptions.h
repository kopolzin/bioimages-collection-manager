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

#ifndef ADVANCEDOPTIONS_H
#define ADVANCEDOPTIONS_H

#include <QWidget>
#include <QProgressDialog>

#include "agent.h"
#include "determination.h"
#include "image.h"
#include "organism.h"
#include "sensu.h"

namespace Ui {
class AdvancedOptions;
}

class AdvancedOptions : public QWidget
{
    Q_OBJECT

public:
    explicit AdvancedOptions(QWidget *parent = 0);
    ~AdvancedOptions();

signals:
    void windowClosed();

private slots:
    void on_selectImagesButton_clicked();
    void on_baseFolderButton_clicked();
    void on_resizeImagesButton_clicked();
    void on_backButton_clicked();
    void on_resetCheckbox_toggled(bool checked);

    void on_resetButton_clicked();

private:
    Ui::AdvancedOptions *ui;
    QStringList selectImages();
    void loadAgents();

    void resizeEvent(QResizeEvent *);
    void moveEvent(QMoveEvent *);
    void changeEvent(QEvent *event);
    bool screenPosLoaded;

    QHash<QString,QString> agentHash;
    QStringList agentsList;
    QString photographer;
    QString baseFolder;
    QString photoFolder;
    QStringList imagesToResize;
};

#endif // ADVANCEDOPTIONS_H
