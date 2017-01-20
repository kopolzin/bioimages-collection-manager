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

#ifndef MERGETABLES_H
#define MERGETABLES_H

#include <QDialog>
#include <QtWidgets>

#include "image.h"
#include "determination.h"
#include "organism.h"
#include "sensu.h"
#include "agent.h"
#include "taxa.h"

QT_BEGIN_NAMESPACE
class QDialogButtonBox;
class QPushButton;
class QSqlTableModel;
QT_END_NAMESPACE

class MergeTables : public QWidget
{
    Q_OBJECT

public:
    explicit MergeTables(QWidget *parent = 0);
    explicit MergeTables(const QString &firstPrefix, const QString &secondPrefix, QWidget *parent = 0);
    explicit MergeTables(const QString &firstPrefix, const QString &secondPrefix, const QString &oneTable, QWidget *parent = 0);
    ~MergeTables();
    void displayChoices();
    void setSilentMerge(bool state);

signals:
    void finished();
    void loaded();
    void canceled();

private slots:
    void submit();
    void selectAllLocal();
    void unselectAllLocal();
    void confirmClose();

private:
    QSize sizeHint() const;
    void resizeEvent(QResizeEvent *);
    void moveEvent(QMoveEvent *);
    void changeEvent(QEvent *event);
    void setupLayout();
    QString modifiedNow();
    bool updating;
    bool merging;
    bool silentMerge;
    QString onlyTable;

    QString ageT;
    QString imaT;
    QString detT;
    QString orgT;
    QString senT;
    QString namT;
    QString age2T;
    QString ima2T;
    QString det2T;
    QString org2T;
    QString sen2T;
    QString nam2T;

    QString databaseVersion;
    QList<Image> loadImages(const QString &t, const QString &whereStatement);
    QList<Agent> loadAgents(const QString &t, const QString &whereStatement);
    QList<Determination> loadDeterminations(const QString &t, const QString &whereStatement);
    QList<Organism> loadOrganisms(const QString &t, const QString &whereStatement);
    QList<Sensu> loadSensu(const QString &t, const QString &whereStatement);
    QList<Taxa> loadTaxa(const QString &t, const QString &whereStatement);

    QPointer<QTableView> agentsTable;
    QPointer<QTableView> determinationsTable;
    QPointer<QTableView> imagesTable;
    QPointer<QTableView> taxaTable;
    QPointer<QTableView> organismsTable;
    QPointer<QTableView> sensuTable;

    QStringList imageIDs; // list of dcterms_identifier of conflicting records
    QStringList newImageIDs; // list of dcterms_identifier in 'images' but not in 'tmp_images'
    QStringList agentIDs; // list of dcterms_identifier of conflicting records
    QStringList newAgentIDs; // list of dcterms_identifier in 'agents' but not in 'tmp_agents'
    QStringList determinationIDs; // list of dcterms_identifier of conflicting records
    QStringList newDeterminationIDs; // list of dcterms_identifier in 'determinations' but not in 'tmp_determinations'
    QStringList organismIDs; // list of dcterms_identifier of conflicting records
    QStringList newOrganismIDs; // list of dcterms_identifier in 'organisms' but not in 'tmp_organisms'
    QStringList sensuIDs; // list of dcterms_identifier of conflicting records
    QStringList newSensuIDs; // list of dcterms_identifier in 'sensu' but not in 'tmp_sensu'
    QStringList taxaIDs; // list of dcterms_identifier of conflicting records
    QStringList newTaxaIDs; // list of dcterms_identifier in 'tmp_taxa' but not in 'taxa'

    void mergeNonConflicts();
    void findDifferences();
    void alterTables();
    void findImageDiff();
    void findAgentDiff();
    void findDeterminationDiff();
    void findOrganismDiff();
    void findSensuDiff();
    void findTaxaDiff();
};

#endif // MERGETABLES_H
