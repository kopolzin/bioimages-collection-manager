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

#include <QDate>
#include <QTime>
#include <QDateTime>
#include <QTimeZone>
#include "image.h"

Image::Image()
{
    Initialize();
}

Image::~Image()
{

}

void Image::Initialize()
{
    QString currentDateTime = QDateTime::currentDateTime().toString("yyyy-MM-dd'T'hh:mm:ss");
    QString currentYear = QDateTime::currentDateTime().toString("yyyy");

    QDateTime currentTimeUTC = QDateTime::currentDateTime();
    QDateTime currentTimeLocal = currentTimeUTC;
    currentTimeUTC.setTimeSpec(Qt::UTC);

    int timezoneOffsetInt = currentTimeLocal.secsTo(currentTimeUTC) / 3600;
    QString timezoneOffset = QString::number(timezoneOffsetInt);

    // convert offset to format: +/-hh:mm
    if (timezoneOffset.contains("-"))
    {
        timezoneOffset.remove("-");
        if (timezoneOffset.toInt() < 10)
            timezoneOffset = "0" + timezoneOffset;
        timezoneOffset = "-" + timezoneOffset + ":00";
    }
    else
    {
        if (timezoneOffset.toInt() < 10)
            timezoneOffset = "0" + timezoneOffset;
        timezoneOffset = "+" + timezoneOffset + ":00";
    }

    fileAndPath = "";
    groupOfSpecimen = "unspecified";
    portionOfSpecimen = "unspecified";
    viewOfSpecimen = "unspecified";

    fileName = "";
    date = "";
    time = "";
    timezone = "";
    focalLength = "";
    georeferenceRemarks = "";
    decimalLatitude = "";                 // in decimal form
    decimalLongitude = "";                // in decimal form
    altitudeInMeters = "";                // in meters
    width = "";
    height = "";
    occurrenceRemarks = "";
    geodeticDatum = "EPSG:4326";
    coordinateUncertaintyInMeters = "";
    locality = "";
    countryCode = "";
    stateProvince = "";
    county = "";
    informationWithheld = "";
    dataGeneralizations = "";
    continent = "";
    geonamesAdmin = "";
    geonamesOther = "";
    identifier = "";
    lastModified = currentDateTime + timezoneOffset;
    title = "";
    description = "";
    caption = "";
    photographerCode = "";
    dcterms_created = "";
    credit = "";
    copyrightOwnerID = "";
    copyrightYear = currentYear;
    copyrightStatement = "";
    copyrightOwnerName = "";
    attributionLinkURL = "";
    urlToHighRes = "";
    usageTermsIndex = "4";
    imageView = "";
    rating = "5";
    depicts = "";
    suppress = "";
}
