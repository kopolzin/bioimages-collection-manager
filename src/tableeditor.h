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

#ifndef TABLEEDITOR_H
#define TABLEEDITOR_H

#include <QDialog>
#include <QtWidgets>

QT_BEGIN_NAMESPACE
class QDialogButtonBox;
class QPushButton;
class QSqlTableModel;
QT_END_NAMESPACE

class TableEditor : public QWidget
{
    Q_OBJECT

public:
    explicit TableEditor(const QString &incTableFilter, QWidget *parent = 0);
    explicit TableEditor(QWidget *parent = 0);

private slots:
    void submit();
    void removeSelectedRows();
    void revertAll();
    void refreshAll();
    void confirmClose();

private:
    QSize sizeHint() const;
    void resizeEvent(QResizeEvent *);
    void moveEvent(QMoveEvent *);
    void changeEvent(QEvent *event);
    void setupLayout();
    QString tableFilter;
    QString modifiedNow();
    QStringList reversions;

    QPushButton *submitButton;
    QPushButton *refreshButton;
    QPushButton *deleteButton;
    QPushButton *quitButton;
    QDialogButtonBox *buttonBox;
    QSqlTableModel *agentsModel;
    QTableView *agentsTable;
    QSqlTableModel *determinationsModel;
    QTableView *determinationsTable;
    QSqlTableModel *imagesModel;
    QTableView *imagesTable;
    QSqlTableModel *taxaModel;
    QTableView *taxaTable;
    QSqlTableModel *organismsModel;
    QTableView *organismsTable;
    QSqlTableModel *sensuModel;
    QTableView *sensuTable;
    QTabWidget *tabWidget;
};

#endif
