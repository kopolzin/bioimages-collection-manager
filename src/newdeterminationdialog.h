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

#ifndef NEWDETERMINATIONDIALOG_H
#define NEWDETERMINATIONDIALOG_H

#include <QDialog>
#include <QCompleter>
#include "determination.h"

namespace Ui {
class NewDeterminationDialog;
}

class NewDeterminationDialog : public QDialog
{
    Q_OBJECT

public:
    explicit NewDeterminationDialog(const QString &organismID, const QStringList &allGenera,  const QStringList &allCommonNames, const QStringList &agents, const QStringList &sensus, QWidget *parent = 0);
    ~NewDeterminationDialog();

    Determination getDetermination();
    bool newSensu();

private slots:
    void on_genusSearch_textChanged(const QString &arg1);
    void on_commonNameSearch_textChanged(const QString &arg1);
    void on_identifiedBy_currentTextChanged(const QString &arg1);
    void on_dateIdentified_textEdited(const QString &arg1);
    void on_nameAccordingToID_currentTextChanged(const QString &arg1);
    void on_identificationRemarks_textEdited(const QString &arg1);
    void on_cancelButton_clicked();
    void on_doneButton_clicked();
    void on_newSensuButton_clicked();
    void on_manualEntry_clicked();

private:
    Ui::NewDeterminationDialog *ui;

    QString dsw_identified;
    QString identifiedBy;
    QString dwc_dateIdentified;
    QString dwc_identificationRemarks;
    QString tsnID;
    QString nameAccordingToID;
    QString dcterms_modified;
    QString suppress;
    void setTaxaFromTSNID();
    QPointer<QCompleter> completeGenera;
    QPointer<QCompleter> completeCommonNames;
    QPointer<QCompleter> completeSensu;
    QStringList allGenera;
    QStringList allCommonNames;
    bool sensuAdded;
    QStringList allSensus;
    QString modifiedNow();
};

#endif // NEWDETERMINATIONDIALOG_H
