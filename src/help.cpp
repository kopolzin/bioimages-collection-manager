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

#include <QFile>
#include <QDebug>
#include <QDesktopWidget>

#include "help.h"
#include "ui_help.h"

Help::Help(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Help)
{
    ui->setupUi(this);
    move(QApplication::desktop()->screen()->rect().center() - rect().center());

    QStringList indexList;
    indexList << "Overview" << "Start Screen" << "Process New Images" << "View/Edit Existing Records" << "Data Entry" << "Manage CSVs" << "Generate Website" << "About";
    ui->helpIndex->addItems(indexList);

    QFontMetrics fmIndex(ui->helpIndex->font());
    ui->helpIndex->setMinimumWidth(fmIndex.width("View/Edit Existing Records") + 10);

    connect(ui->helpIndex,SIGNAL(itemSelectionChanged()),this,SLOT(updateTextBrowser()));\
    ui->textBrowser->setOpenExternalLinks(true);
}

void Help::select(const QString &selected)
{
    if (selected == "Overview") {
        ui->helpIndex->setCurrentItem(ui->helpIndex->item(0));
    }
    else if (selected == "Start Screen") {
        ui->helpIndex->setCurrentItem(ui->helpIndex->item(1));
    }
    else if (selected == "Process New Images") {
        ui->helpIndex->setCurrentItem(ui->helpIndex->item(2));
    }
    else if (selected == "View/Edit Existing Records") {
        ui->helpIndex->setCurrentItem(ui->helpIndex->item(3));
    }
    else if (selected == "Data Entry") {
        ui->helpIndex->setCurrentItem(ui->helpIndex->item(4));
    }
    else if (selected == "Manage CSVs") {
        ui->helpIndex->setCurrentItem(ui->helpIndex->item(5));
    }
    else if (selected == "Generate Website") {
        ui->helpIndex->setCurrentItem(ui->helpIndex->item(6));
    }
    else if (selected == "About") {
        ui->helpIndex->setCurrentItem(ui->helpIndex->item(7));
    }
}

Help::~Help()
{
    delete ui;
}

void Help::updateTextBrowser()
{
    if (ui->helpIndex->selectedItems().count() < 1)
        return;

    QString selectedText = ui->helpIndex->selectedItems().at(0)->text();
    QString helpContent = "";

    if (selectedText == "Overview") {
        helpContent = read(":/help/overview.html");
    }
    else if (selectedText == "Start Screen") {
        helpContent = read(":/help/start_screen.html");
    }
    else if (selectedText == "Process New Images") {
        helpContent = read(":/help/process_new_images.html");
    }
    else if (selectedText == "View/Edit Existing Records") {
        helpContent = read(":/help/view_edit_existing.html");
    }
    else if (selectedText == "Data Entry") {
        helpContent = read(":/help/data_entry.html");
    }
    else if (selectedText == "Manage CSVs") {
        helpContent = read(":/help/manage_csvs.html");
    }
    else if (selectedText == "Generate Website") {
        helpContent = read(":/help/generate_website.html");
    }
    else if (selectedText == "About") {
        helpContent = read(":/help/about.html");
    }


    if (!helpContent.isEmpty())
        ui->textBrowser->setText(helpContent);
}

QString Help::read(QString filename)
{
    QFile mFile(filename);

    if(!mFile.open(QFile::ReadOnly | QFile::Text)){
        qDebug() << "Could not open file for read";
        return "";
    }

    QTextStream in(&mFile);
    in.setCodec("UTF-8");
    QString mText = in.readAll();

    mFile.close();

    return mText;
}
