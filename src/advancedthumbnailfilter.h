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

#ifndef ADVANCEDTHUMBNAILFILTER_H
#define ADVANCEDTHUMBNAILFILTER_H

#include <QDialog>

namespace Ui {
class AdvancedThumbnailFilter;
}

class AdvancedThumbnailFilter : public QDialog
{
    Q_OBJECT

public:
    explicit AdvancedThumbnailFilter(QWidget *parent = 0);
    ~AdvancedThumbnailFilter();
    QString getAdvancedFilter();

private slots:
    void on_doneButton_clicked();
    void on_cancelButton_clicked();
    void on_loadSavedFilterDropdown_currentIndexChanged(const QString &arg1);
    void on_saveFilterButton_clicked();
    void on_deleteButton_clicked();

private:
    Ui::AdvancedThumbnailFilter *ui;
    void populateDropdown();

    QHash<QString,QString> nicknameToQuery;
    QString selectedFilter;
};

#endif // ADVANCEDTHUMBNAILFILTER_H
