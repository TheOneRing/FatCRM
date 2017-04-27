/*
  This file is part of FatCRM, a desktop application for SugarCRM written by KDAB.

  Copyright (C) 2015-2017 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
  Authors: Jeremy Entressangle <jeremy.entressangle@kdab.com>

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "sugarmockprotocol.h"
#include "sugarjob.h"
#include "accountshandler.h"
#include "opportunitieshandler.h"
#include "campaignshandler.h"
#include "leadshandler.h"
#include "contactshandler.h"

#include "campaignshandler.h"
#include "leadshandler.h"
#include "contactshandler.h"
#include "listentriesscope.h"

SugarMockProtocol::SugarMockProtocol()
    : mAccountHandler(0), mOpportunityHandler(0), mCampaignHandler(0), mLeadHandler(0), mContactHandler(0), mServerNotFound(false)
{
    mAccounts.resize(3);
    mOpportunities.resize(2);
    mCampaigns.resize(1);
    mLeads.resize(1);
    mContacts.resize(1);
}



int SugarMockProtocol::login(const QString &user, const QString &password, QString &sessionId, QString &errorMessage)
{
    if (!mServerNotFound) {
        if (user == "user" && password == "password") {
            sessionId = "1";
            return KJob::NoError;
        } else {
            int error = SugarJob::Errors::LoginError;
            errorMessage = QString("Login for user %1 on %2 failed: %3").arg(user).arg("test").arg(error);
            return error;
        }
    } else {
        return SugarJob::Errors::CouldNotConnectError;
    }
}

void SugarMockProtocol::logout()
{
}

void SugarMockProtocol::setSession(SugarSession *session)
{
    Q_UNUSED(session);
}

int SugarMockProtocol::getEntriesCount(const ListEntriesScope &scope, const QString &moduleName, const QString &query,
                                       int &entriesCount, QString &errorMessage)
{
    Q_UNUSED(scope);
    Q_UNUSED(query);
    Q_UNUSED(errorMessage);
    if (moduleName == "Accounts") {
        entriesCount = mAccounts.size();
    } else if (moduleName == "Opportunities") {
        entriesCount = mOpportunities.size();
    } else if (moduleName == "Campaigns") {
        entriesCount = mCampaigns.size();
    } else if (moduleName == "Leads") {
        entriesCount = mLeads.size();
    } else if (moduleName == "Contacts") {
        entriesCount = mContacts.size();
    } else {
        entriesCount = 0;
    }

    return 0;
}

QList<KDSoapGenerated::TNS__Entry_value> SugarMockProtocol::listAccount() const
{
    QList<KDSoapGenerated::TNS__Entry_value> items;
    for ( int i = 0; i < mAccounts.size(); ++i) {
        KDSoapGenerated::TNS__Entry_value entryValue;
        entryValue.setId(QString::number(i));
        entryValue.setModule_name("Accounts");
        KDSoapGenerated::TNS__Name_value_list nvl = mAccountHandler->sugarAccountToNameValueList(mAccounts.at(i));
        entryValue.setName_value_list(nvl);
        items.push_back(entryValue);
    }
    return items;
}

QList<KDSoapGenerated::TNS__Entry_value> SugarMockProtocol::listOpportunities() const
{
    QList<KDSoapGenerated::TNS__Entry_value> items;
    for ( int i = 0; i < mOpportunities.size(); ++i) {
        KDSoapGenerated::TNS__Entry_value entryValue;
        entryValue.setId(QString::number(i));
        entryValue.setModule_name("Opportunities");
        KDSoapGenerated::TNS__Name_value_list nvl = mOpportunityHandler->sugarOpportunityToNameValueList(mOpportunities.at(i));
        entryValue.setName_value_list(nvl);
        items.push_back(entryValue);
    }
    return items;
}

QList<KDSoapGenerated::TNS__Entry_value> SugarMockProtocol::listCampaigns() const
{
    QList<KDSoapGenerated::TNS__Entry_value> items;
    for ( int i = 0; i < mCampaigns.size(); ++i) {
        KDSoapGenerated::TNS__Entry_value entryValue;
        entryValue.setId(QString::number(i));
        entryValue.setModule_name("Campaigns");
        KDSoapGenerated::TNS__Name_value_list nvl = mCampaignHandler->sugarCampaignToNameValueList(mCampaigns.at(i));
        entryValue.setName_value_list(nvl);
        items.push_back(entryValue);
    }
    return items;
}

QList<KDSoapGenerated::TNS__Entry_value> SugarMockProtocol::listLeads() const
{
    QList<KDSoapGenerated::TNS__Entry_value> items;
    for ( int i = 0; i < mLeads.size(); ++i) {
        KDSoapGenerated::TNS__Entry_value entryValue;
        entryValue.setId(QString::number(i));
        entryValue.setModule_name("Leads");
        KDSoapGenerated::TNS__Name_value_list nvl = mLeadHandler->sugarLeadToNameValueList(mLeads.at(i));
        entryValue.setName_value_list(nvl);
        items.push_back(entryValue);
    }
    return items;
}

QList<KDSoapGenerated::TNS__Entry_value> SugarMockProtocol::listContacts() const
{
    QList<KDSoapGenerated::TNS__Entry_value> items;
    for ( int i = 0; i < mContacts.size(); ++i) {
        KDSoapGenerated::TNS__Entry_value entryValue;
        entryValue.setId(QString::number(i));
        entryValue.setModule_name("Contacts");
        KDSoapGenerated::TNS__Name_value_list nvl = mContactHandler->addresseeToNameValueList(mContacts.at(i));
        entryValue.setName_value_list(nvl);
        items.push_back(entryValue);
    }
    return items;
}

int SugarMockProtocol::listEntries(const ListEntriesScope &scope, const QString &moduleName, const QString &query, const QString &orderBy,
                                   const QStringList &selectedFields, EntriesListResult &entriesListResult, QString &errorMessage)
{
    Q_UNUSED(moduleName);
    Q_UNUSED(query); Q_UNUSED(orderBy);
    Q_UNUSED(selectedFields); Q_UNUSED(errorMessage);

    if (scope.offset() > 0) {
        entriesListResult.resultCount = 0;
    } else {
        if (moduleName == "Accounts") {
            entriesListResult.resultCount = mAccounts.size();
            entriesListResult.entryList.setItems(listAccount());
        } else if (moduleName == "Opportunities") {
            entriesListResult.resultCount = mOpportunities.size();
            entriesListResult.entryList.setItems(listOpportunities());
        } else if (moduleName == "Campaigns") {
            entriesListResult.resultCount = mCampaigns.size();
            entriesListResult.entryList.setItems(listCampaigns());
        } else if (moduleName == "Leads") {
            entriesListResult.resultCount = mLeads.size();
            entriesListResult.entryList.setItems(listLeads());
        } else if (moduleName == "Contacts") {
            entriesListResult.resultCount = mContacts.size();
            entriesListResult.entryList.setItems(listContacts());
        } else {
            entriesListResult.resultCount = -1;
        }
    }
    return 0;
}

void SugarMockProtocol::setAccountsHandler(AccountsHandler *handler)
{
    mAccountHandler = handler;
}

void SugarMockProtocol::setOpportunitiesHandler(OpportunitiesHandler *handler)
{
    mOpportunityHandler = handler;
}

void SugarMockProtocol::setCampaignsHandler(CampaignsHandler *handler)
{
    mCampaignHandler = handler;
}

void SugarMockProtocol::setLeadsHandler(LeadsHandler *handler)
{
    mLeadHandler = handler;
}

void SugarMockProtocol::setContactsHandler(ContactsHandler *handler)
{
    mContactHandler = handler;
}
