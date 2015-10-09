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

#include "agent.h"

Agent::Agent()
{

}

Agent::Agent(const QString &ac, const QString &fn, const QString &uri, const QString &contact, const QString &mbID, const QString &lmod, const QString &atype)
{
    agentCode = ac;
    fullName = fn;
    agentURI = uri;
    contactURL = contact;
    morphbankID = mbID;
    lastModified = lmod;
    agentType = atype;
}

Agent::~Agent()
{

}

void Agent::setAgentCode(const QString &ac)
{
    agentCode = ac;
}

void Agent::setFullName(const QString &fn)
{
    fullName = fn;
}

void Agent::setAgentURI(const QString &uri)
{
    agentURI = uri;
}

void Agent::setContactURL(const QString &contact)
{
    contactURL = contact;
}

void Agent::setMorphbankID(const QString &mb)
{
    morphbankID = mb;
}

void Agent::setLastModified(const QString &lmod)
{
    lastModified = lmod;
}

void Agent::setAgentType(const QString &atype)
{
    agentType = atype;
}

QString Agent::getAgentCode()
{
    return agentCode;
}

QString Agent::getFullName()
{
    return fullName;
}

QString Agent::getAgentURI()
{
    return agentURI;
}

QString Agent::getContactURL()
{
    return contactURL;
}

QString Agent::getMorphbankID()
{
    return morphbankID;
}

QString Agent::getLastModified()
{
    return lastModified;
}

QString Agent::getAgentType()
{
    return agentType;
}

