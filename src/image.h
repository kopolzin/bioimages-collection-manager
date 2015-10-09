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

#ifndef IMAGE_H
#define IMAGE_H

#include <QtCore>

class Image
{
public:
    Image();
    ~Image();
    void Initialize();

    QString fileAndPath;
    QString groupOfSpecimen;
    QString portionOfSpecimen;
    QString viewOfSpecimen;
    QString date;
    QString time;
    QString timezone;

    QString fileName;
    QString focalLength;
    QString georeferenceRemarks;
    QString decimalLatitude;
    QString decimalLongitude;
    QString altitudeInMeters;
    QString width;
    QString height;
    QString occurrenceRemarks;
    QString geodeticDatum;
    QString coordinateUncertaintyInMeters;
    QString locality;
    QString countryCode;
    QString stateProvince;
    QString county;
    QString informationWithheld;
    QString dataGeneralizations;
    QString continent;
    QString geonamesAdmin;
    QString geonamesOther;
    QString identifier;
    QString lastModified;
    QString title;
    QString description;
    QString caption;
    QString photographerCode;
    QString dcterms_created;
    QString credit;
    QString copyrightOwnerID;
    QString copyrightYear;
    QString copyrightStatement;
    QString copyrightOwnerName;
    QString attributionLinkURL;
    QString urlToHighRes;
    QString usageTermsIndex;
    QString imageView;
    QString rating;
    QString depicts;
    QString suppress;
};

#endif // IMAGE_H
