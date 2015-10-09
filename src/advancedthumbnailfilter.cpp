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
#include <QSqlQuery>

#include "advancedthumbnailfilter.h"
#include "ui_advancedthumbnailfilter.h"

AdvancedThumbnailFilter::AdvancedThumbnailFilter(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AdvancedThumbnailFilter)
{
    ui->setupUi(this);

    QString lastFilter;
    QSqlQuery query;
    query.prepare("SELECT value FROM settings WHERE setting = (?) LIMIT 1");
    query.addBindValue("last.thumbnailfilter");
    query.exec();
    while (query.next())
    {
        lastFilter = query.value(0).toString();
    }
    populateDropdown();

    if (!lastFilter.isEmpty())
        ui->loadSavedFilterDropdown->setCurrentText(lastFilter);
}

AdvancedThumbnailFilter::~AdvancedThumbnailFilter()
{
    delete ui;
}

QString AdvancedThumbnailFilter::getAdvancedFilter()
{
    QString filter = selectedFilter;
    if (!filter.toLower().startsWith("select filename from images where "))
        filter.prepend("SELECT fileName FROM images WHERE ");
    return filter;
}

void AdvancedThumbnailFilter::populateDropdown()
{
    // Populate dropdown list from "geonames_other" table
    QSqlQuery query;
    query.prepare("SELECT nickname, query FROM thumbnail_filters");
    query.exec();

    QStringList nicknames;
    QStringList queries;
    nicknameToQuery.clear();
    while (query.next())
    {
        QString nickname = query.value(0).toString();
        QString filter = query.value(1).toString();
        nicknames.append(nickname);
        queries.append(filter);
        nicknameToQuery.insert(nickname,filter);
    }
    nicknames.sort(Qt::CaseInsensitive);

    ui->loadSavedFilterDropdown->clear();
    ui->loadSavedFilterDropdown->addItems(nicknames);

    // if the dropdown changes the idToSet/descToSet must be refreshed
    QString currentlySelected = ui->loadSavedFilterDropdown->currentText();
    if (nicknameToQuery.contains(currentlySelected))
    {
        ui->saveFilterDescription->setText(currentlySelected);
        ui->filterToApply->setPlainText(nicknameToQuery.value(currentlySelected));
    }
    else
    {
        ui->filterToApply->clear();
    }
}

void AdvancedThumbnailFilter::on_doneButton_clicked()
{
    selectedFilter = ui->filterToApply->toPlainText();
    accept();
}

void AdvancedThumbnailFilter::on_cancelButton_clicked()
{
    reject();
}

void AdvancedThumbnailFilter::on_loadSavedFilterDropdown_currentIndexChanged(const QString &arg1)
{
    if (nicknameToQuery.contains(arg1))
    {
        ui->saveFilterDescription->setText(arg1);
        QString filter = nicknameToQuery.value(arg1);
        ui->filterToApply->setPlainText(filter);
        QSqlQuery insert;
        insert.prepare("INSERT OR REPLACE INTO settings (setting, value) VALUES ((?), (?))");
        insert.addBindValue("last.thumbnailfilter");
        insert.addBindValue(arg1);
        insert.exec();
    }
    else
    {
        ui->filterToApply->clear();
    }
}

void AdvancedThumbnailFilter::on_saveFilterButton_clicked()
{
    QString newNickname = ui->saveFilterDescription->text().trimmed();
    QString newFilter = ui->filterToApply->toPlainText();
    if (newNickname.isEmpty())
    {
        QMessageBox msgBox;
        msgBox.setText("Please add a nickname for the filter.\n");
        msgBox.exec();
        return;
    }
    else if (newFilter.isEmpty())
    {
        QMessageBox msgBox;
        msgBox.setText("Please add a filter first.\n");
        msgBox.exec();
        return;
    }

    QSqlQuery insert;
    insert.prepare("INSERT OR REPLACE INTO thumbnail_filters (nickname, query) VALUES ((?), (?))");
    insert.addBindValue(newNickname);
    insert.addBindValue(newFilter);
    insert.exec();

    populateDropdown();
    ui->loadSavedFilterDropdown->setCurrentText(newNickname);
    ui->filterToApply->setPlainText(newFilter);
}

void AdvancedThumbnailFilter::on_deleteButton_clicked()
{
    QString nickname = ui->loadSavedFilterDropdown->currentText();
    if (nickname.isEmpty())
        return;
    if (!nicknameToQuery.contains(nickname))
        return;
    QSqlQuery remove;
    remove.prepare("DELETE FROM thumbnail_filters WHERE nickname = (?)");
    remove.addBindValue(nickname);
    remove.exec();
    populateDropdown();
}
