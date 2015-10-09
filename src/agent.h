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

#ifndef AGENT_H
#define AGENT_H

#include <QtCore>

class Agent
{
public:
    Agent();
    Agent(const QString &ac, const QString &fn, const QString &uri, const QString &contact, const QString &mbID, const QString &lmod, const QString &atype);
    ~Agent();

    void setAgentCode(const QString &ac);
    void setFullName(const QString &fn);
    void setAgentURI(const QString &uri);
    void setContactURL(const QString &contact);
    void setMorphbankID(const QString &mbID);
    void setLastModified(const QString &lmod);
    void setAgentType(const QString &atype);
    QString getAgentCode();
    QString getFullName();
    QString getAgentURI();
    QString getContactURL();
    QString getMorphbankID();
    QString getLastModified();
    QString getAgentType();

private:
    QString agentCode;
    QString fullName;
    QString agentURI;
    QString contactURL;
    QString morphbankID;
    QString lastModified;
    QString agentType;
};

#endif // AGENT_H
