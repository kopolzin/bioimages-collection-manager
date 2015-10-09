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

#include <QDateTime>
#include "organism.h"

Organism::Organism()
{
    Initialize();
}

Organism::~Organism()
{

}

void Organism::Initialize()
{
    QString currentDateTime = QDateTime::currentDateTime().toString("yyyy-MM-dd'T'hh:mm:ss");

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

    identifier = "";
    establishmentMeans = "uncertain";
    lastModified = currentDateTime + timezoneOffset;
    organismRemarks = "";
    collectionCode = "";
    catalogNumber = "";
    georeferenceRemarks = "";
    decimalLatitude = "";
    decimalLongitude = "";
    altitudeInMeters = "";
    organismName = "";
    organismScope = "multicellular organism";
    cameo = "";
    notes = "";
    suppress = "";
}
