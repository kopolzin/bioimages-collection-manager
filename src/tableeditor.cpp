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

#include <QtSql>
#include "tableeditor.h"

TableEditor::TableEditor(const QString &incTableFilter, QWidget *parent)
    : QWidget(parent)
{
    tableFilter = incTableFilter;
    setupLayout();
}

TableEditor::TableEditor(QWidget *parent)
{
    tableFilter = "";
    setupLayout();
}

void TableEditor::submit()
{
    QList<QSqlTableModel*> tableModels;
    tableModels << agentsModel << determinationsModel << imagesModel << taxaModel << organismsModel << sensuModel;

    // update dcterms_modified first
    for (auto model : tableModels)
    {
        // make sure there were some changes first
        if (model->isDirty())
        {
            // find which column holds the dcterms_modified value
            int modifiedColumn = model->fieldIndex("dcterms_modified");

            for (QModelIndex index = model->index(0,0);
                 index.isValid();
                 index = model->index(index.row()+1,0) )
            {
                for (QModelIndex cindex = model->index(index.row(),0);
                     cindex.isValid();
                     cindex = model->index(index.row(),cindex.column()+1) )
                {
                    if (model->isDirty(cindex))
                    {
                        QModelIndex timeIndex = model->index(index.row(),modifiedColumn);
                        model->setData(timeIndex,modifiedNow());
                        break;
                    }
                }
            }
        }
    }

    QString errors;

    // save changes to each table
    for (auto model : tableModels)
    {
        model->database().transaction();
        if (model->submitAll())
            model->database().commit();
        else
        {
            model->database().rollback();
            errors.append(model->lastError().text() + "\n");
        }
    }

    if (!errors.isEmpty())
    {
        QString title = "Saving changes";
        QMessageBox::warning(this,title,errors);
    }

    QSqlDatabase db = QSqlDatabase::database();
    db.transaction();

    reversions.removeDuplicates();
    for (auto r : reversions)
    {
        QSqlQuery revertQry(r);
    }

    for (auto model : tableModels)
    {
        model->select();
    }

    if (!db.commit())
    {
        qDebug() << "Problem committing changes to database in TableEditor::setupLayout()";
        db.rollback();
    }
    reversions.clear();
}

void TableEditor::removeSelectedRows()
{
    int currentTab = tabWidget->currentIndex();
    QTableView *tv = agentsTable;
    QSqlTableModel *stm = agentsModel;
    QString table = "agents";
    int idColumn = 0;
    if (currentTab == 1)
    {
        tv = determinationsTable;
        stm = determinationsModel;
        table = "determinations";
    }
    else if (currentTab == 2)
    {
        tv = imagesTable;
        stm = imagesModel;
        table = "images";
        idColumn = 20;
    }
    else if (currentTab == 3)
    {
        tv = taxaTable;
        stm = taxaModel;
        table = "taxa";
        idColumn = 1;
    }
    else if (currentTab == 4)
    {
        tv = organismsTable;
        stm = organismsModel;
        table = "organisms";
    }
    else if (currentTab == 5)
    {
        tv = sensuTable;
        stm = sensuModel;
        table = "sensu";
    }
    QItemSelectionModel *selected = tv->selectionModel();
    QModelIndexList rowList = selected->selectedIndexes();

    for (int i = rowList.count()-1; i >= 0; i--)
    {
        stm->removeRow(rowList.at(i).row(), rowList.at(i).parent());
        QString identifier;
        if (table == "determinations")
        {
            QString orgID = stm->data(stm->index(rowList.at(i).row(),0),Qt::DisplayRole).toString();
            QString date = stm->data(stm->index(rowList.at(i).row(),2),Qt::DisplayRole).toString();
            QString tsnID = stm->data(stm->index(rowList.at(i).row(),4),Qt::DisplayRole).toString();
            QString source = stm->data(stm->index(rowList.at(i).row(),5),Qt::DisplayRole).toString();
            reversions.append("INSERT OR REPLACE INTO determinations SELECT * FROM pub_determinations "
                              "WHERE dsw_identified = '" + orgID + "' AND "
                              "dwc_dateIdentified = '" + date + "' AND "
                              "tsnID = '" + tsnID + "' AND "
                              "nameAccordingToID = '" + source + "' LIMIT 1");

        }
        else
        {
            identifier = stm->data(stm->index(rowList.at(i).row(),idColumn),Qt::DisplayRole).toString();
            reversions.append("INSERT OR REPLACE INTO " + table + " SELECT * FROM pub_" + table + " WHERE dcterms_identifier = '" + identifier + "' LIMIT 1");
        }
    }
}

void TableEditor::revertAll()
{
    agentsModel->revertAll();
    determinationsModel->revertAll();
    imagesModel->revertAll();
    taxaModel->revertAll();
    organismsModel->revertAll();
    sensuModel->revertAll();
    reversions.clear();
}

void TableEditor::refreshAll()
{
    agentsModel->select();
    determinationsModel->select();
    imagesModel->select();
    taxaModel->select();
    organismsModel->select();
    sensuModel->select();
    reversions.clear();
}

QSize TableEditor::sizeHint() const
{
    return QSize(640,240);
}

void TableEditor::resizeEvent(QResizeEvent *)
{
    QSqlQuery qry;
    qry.prepare("INSERT OR REPLACE INTO settings (setting, value) VALUES (?, ?)");
    qry.addBindValue("view.table.location");
    qry.addBindValue(saveGeometry());
    qry.exec();
}

void TableEditor::moveEvent(QMoveEvent *)
{
    QSqlQuery qry;
    qry.prepare("INSERT OR REPLACE INTO settings (setting, value) VALUES (?, ?)");
    qry.addBindValue("view.table.location");
    qry.addBindValue(saveGeometry());
    qry.exec();
}

void TableEditor::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::WindowStateChange)
    {
        bool isMax = false;
        if (windowState() == Qt::WindowMaximized)
            isMax = true;
        QSqlQuery qry;
        qry.prepare("INSERT OR REPLACE INTO settings (setting, value) VALUES (?, ?)");
        qry.addBindValue("view.table.fullscreen");
        qry.addBindValue(isMax);
        qry.exec();
    }
}

void TableEditor::setupLayout()
{
    // set up agents tab
    agentsModel = new QSqlTableModel(this);
    agentsModel->setTable("agents");
    if (!tableFilter.isEmpty())
        agentsModel->setFilter(tableFilter);
    agentsModel->setEditStrategy(QSqlTableModel::OnManualSubmit);
    agentsModel->select();

    agentsTable = new QTableView(this);
    agentsTable->setModel(agentsModel);
    agentsTable->resizeColumnsToContents();
    agentsTable->sortByColumn(0,Qt::AscendingOrder);
    agentsTable->setSortingEnabled(true);

    // set up determinations tab
    determinationsModel = new QSqlTableModel(this);
    determinationsModel->setTable("determinations");
    if (!tableFilter.isEmpty())
        determinationsModel->setFilter(tableFilter);
    determinationsModel->setEditStrategy(QSqlTableModel::OnManualSubmit);
    determinationsModel->select();

    determinationsTable = new QTableView(this);
    determinationsTable->setModel(determinationsModel);
    determinationsTable->resizeColumnsToContents();
    determinationsTable->sortByColumn(0,Qt::AscendingOrder);
    determinationsTable->setSortingEnabled(true);

    // set up images tab
    imagesModel = new QSqlTableModel(this);
    imagesModel->setTable("images");
    if (!tableFilter.isEmpty())
        imagesModel->setFilter(tableFilter);
    imagesModel->setEditStrategy(QSqlTableModel::OnManualSubmit);
    imagesModel->select();

    imagesTable = new QTableView(this);
    imagesTable->setModel(imagesModel);
    imagesTable->resizeColumnsToContents();
    imagesTable->sortByColumn(19,Qt::AscendingOrder);
    imagesTable->setSortingEnabled(true);

    // set up taxa tab
    taxaModel = new QSqlTableModel(this);
    taxaModel->setTable("taxa");
    if (!tableFilter.isEmpty())
        taxaModel->setFilter(tableFilter);
    taxaModel->setEditStrategy(QSqlTableModel::OnManualSubmit);
    taxaModel->select();

    taxaTable = new QTableView(this);
    taxaTable->setModel(taxaModel);
    taxaTable->resizeColumnsToContents();
    taxaTable->sortByColumn(1,Qt::AscendingOrder);
    taxaTable->setSortingEnabled(true);

    // set up organisms tab
    organismsModel = new QSqlTableModel(this);
    organismsModel->setTable("organisms");
    if (!tableFilter.isEmpty())
        organismsModel->setFilter(tableFilter);
    organismsModel->setEditStrategy(QSqlTableModel::OnManualSubmit);
    organismsModel->select();

    organismsTable = new QTableView(this);
    organismsTable->setModel(organismsModel);
    organismsTable->resizeColumnsToContents();
    organismsTable->sortByColumn(0,Qt::AscendingOrder);
    organismsTable->setSortingEnabled(true);

    // set up sensu tab
    sensuModel = new QSqlTableModel(this);
    sensuModel->setTable("sensu");
    if (!tableFilter.isEmpty())
        sensuModel->setFilter(tableFilter);
    sensuModel->setEditStrategy(QSqlTableModel::OnManualSubmit);
    sensuModel->select();

    sensuTable = new QTableView(this);
    sensuTable->setModel(sensuModel);
    sensuTable->resizeColumnsToContents();
    sensuTable->sortByColumn(0,Qt::AscendingOrder);
    sensuTable->setSortingEnabled(true);

    tabWidget = new QTabWidget(this);
    tabWidget->addTab(agentsTable,"Agents");
    tabWidget->addTab(determinationsTable,"Determinations");
    tabWidget->addTab(imagesTable,"Images");
    tabWidget->addTab(taxaTable,"Names");
    tabWidget->addTab(organismsTable,"Organisms");
    tabWidget->addTab(sensuTable,"Sensu");

    submitButton = new QPushButton(tr("&Save changes"));
    submitButton->setDefault(true);
    refreshButton = new QPushButton(tr("&Revert unsaved changes\nand refresh database"));
    deleteButton = new QPushButton(tr("&Delete selected rows"));
    quitButton = new QPushButton(tr("&Close"));

    buttonBox = new QDialogButtonBox(Qt::Vertical);
    buttonBox->addButton(submitButton, QDialogButtonBox::ActionRole);
    buttonBox->addButton(deleteButton, QDialogButtonBox::ActionRole);
    buttonBox->addButton(refreshButton, QDialogButtonBox::ActionRole);
    buttonBox->addButton(quitButton, QDialogButtonBox::ActionRole);

    connect(submitButton, SIGNAL(clicked()), this, SLOT(submit())); // this needs to save (submit) all changes from all tables
    connect(refreshButton, SIGNAL(clicked()), this, SLOT(refreshAll())); // this needs to refresh all tables to their current database values
    connect(deleteButton, SIGNAL(clicked()), this, SLOT(removeSelectedRows())); // this needs to remove selected rows ONLY from current tab
    connect(quitButton, SIGNAL(clicked()), this, SLOT(confirmClose()));

    QHBoxLayout *mainLayout = new QHBoxLayout;
    mainLayout->addWidget(tabWidget);
    mainLayout->addWidget(buttonBox);
    setLayout(mainLayout);

    setWindowTitle("Table view");

    QSqlDatabase db = QSqlDatabase::database();
    db.transaction();

    QSqlQuery qry;
    qry.prepare("SELECT value FROM settings WHERE setting = (?)");
    qry.addBindValue("view.table.location");
    qry.exec();
    if (qry.next())
        restoreGeometry(qry.value(0).toByteArray());

    bool wasMaximized = false;
    qry.prepare("SELECT value FROM settings WHERE setting = (?)");
    qry.addBindValue("view.table.fullscreen");
    qry.exec();
    if (qry.next())
        wasMaximized = qry.value(0).toBool();

    if (!db.commit())
    {
        qDebug() << "Problem committing changes to database in TableEditor::setupLayout()";
        db.rollback();
    }

    if (wasMaximized)
        this->showMaximized();
}

QString TableEditor::modifiedNow()
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

void TableEditor::confirmClose()
{
    QList<QSqlTableModel*> tableModels;
    tableModels << agentsModel << determinationsModel << imagesModel << taxaModel << organismsModel << sensuModel;

    QStringList modifiedTables;
    for (auto model : tableModels)
    {
        // see if there are any unsaved changed
        if (model->isDirty())
            modifiedTables.append(model->tableName());
    }

    QString warning;
    if (!modifiedTables.isEmpty())
    {
        if (modifiedTables.size() == 1)
        {
            warning = "There are unsaved changes to the " + modifiedTables.at(0) + " table. "
                      "Are you sure you want to close and lose those changes?";
        }
        else if (modifiedTables.size() > 1)
        {
            QString tableNames = modifiedTables.at(0);
            for (int i=1; i<modifiedTables.size(); i++)
            {
                if (i == modifiedTables.size() -1)
                    tableNames += " and " + modifiedTables.at(i);
                else
                    tableNames += ", " + modifiedTables.at(i);
            }
            warning = "There are unsaved changes to the " + tableNames +
                      " tables. Are you sure you want to close and lose those changes?";
        }

        if (QMessageBox::Yes == QMessageBox(QMessageBox::Information, "Discard unsaved changes?",
                                            warning,
                                            QMessageBox::Yes|QMessageBox::No).exec())
        {
            this->close();
        }
    }
    else
        this->close();
}
