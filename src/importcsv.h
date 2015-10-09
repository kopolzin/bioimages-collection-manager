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

#ifndef IMPORTCSV_H
#define IMPORTCSV_H

#include <QtCore>

class ImportCSV
{
public:
    ImportCSV();
    ~ImportCSV();
    void extractSensu(const QString &CSVPath, const QString &table);
    void extractOrganisms(const QString &CSVPath, const QString &table);
    void extractDeterminations(const QString &CSVPath, const QString &table);
    void extractAgents(const QString &CSVPath, const QString &table);
    QStringList extractImageNames(const QString &CSVPath, const QString &table);
    void extractTaxa(const QString &CSVPath, const QString &table);
    void extractSingleColumn(const QString &CSVPath, const QString &table, const QString &field, bool identifierIsFileName, bool hasHeader, bool idInFirstColumn, const QString separator);
private:
    QString modifiedNow();
    QStringList parseCSV(const QString &line, int numFields);
    QStringList parseCSV(const QString &line, int numFields, const QString &sep);
};

#endif // IMPORTCSV_H
