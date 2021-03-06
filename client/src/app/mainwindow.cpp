/*
  This file is part of FatCRM, a desktop application for SugarCRM written by KDAB.

  Copyright (C) 2015-2017 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
  Authors: David Faure <david.faure@kdab.com>
           Michel Boyer de la Giroday <michel.giroday@kdab.com>
           Kevin Krammer <kevin.krammer@kdab.com>

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

#include "mainwindow.h"

#include "contactspage.h"
#include "accountspage.h"
#include "opportunitiespage.h"

#include "accountrepository.h"
#include "clientsettings.h"
#include "collectionmanager.h"
#include "configurationdialog.h"
#include "contactsimporter.h"
#include "contactsimportwizard.h"
#include "dbusinvokerinterface.h"
#include "dbuswinidprovider.h"
#include "enums.h"
#include "loadingoverlay.h"
#include "config-fatcrm-version.h"
#include "linkeditemsrepository.h"
#include "referenceddata.h"
#include "reportpage.h"
#include "resourceconfigdialog.h"
#include "fatcrm_client_debug.h"

#include "kdcrmdata/enumdefinitionattribute.h"

#include <AkonadiCore/AgentFilterProxyModel>
#include <AkonadiCore/AgentInstance>
#include <AkonadiCore/AgentInstanceModel>
#include <AkonadiCore/AgentManager>
#include <AkonadiCore/AttributeFactory>
#include <AkonadiWidgets/ControlGui>
#include <AkonadiCore/ServerManager>
using namespace Akonadi;

#include <KLocalizedString>

#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDockWidget>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QProgressBar>
#include <QTimer>
#include <QToolBar>

MainWindow::MainWindow(bool displayOverlay)
    : QMainWindow(),
      mProgressBar(nullptr),
      mProgressBarHideTimer(nullptr),
      mCollectionManager(new CollectionManager(this)),
      mLinkedItemsRepository(new LinkedItemsRepository(mCollectionManager, this)),
      mContactsModel(nullptr),
      mInitialLoadingDone(false),
      mDisplayOverlay(displayOverlay)
{
    mUi.setupUi(this);
    initialize(displayOverlay);

    /*
     * this creates an overlay in case Akonadi is not running,
     * allowing the user to restart it
     */
    Akonadi::ControlGui::widgetNeedsAkonadi(this);
    QMetaObject::invokeMethod(this, "slotDelayedInit", Qt::AutoConnection);

    (void)new DBusWinIdProvider(this);

    DBusInvokerInterface *iface = new DBusInvokerInterface(this);
    connect(iface, SIGNAL(importCsvFileRequested(QString)), this, SLOT(slotTryImportCsvFile(QString)));

    ClientSettings::self()->restoreWindowSize(QStringLiteral("main"), this);

    qApp->installEventFilter(this);
}

MainWindow::~MainWindow()
{
    ClientSettings::self()->saveWindowSize(QStringLiteral("main"), this);
    delete mResourceDialog;
}

bool MainWindow::eventFilter(QObject *object, QEvent *event)
{
    // Intercept mouse wheel events on closed combo boxes, because
    // it causes accidentally data changes in the UI.
    if (event->type() == QEvent::Wheel) {
        QComboBox *cb = qobject_cast<QComboBox*>(object);
        if (cb)
            return true;
    }

    return QMainWindow::eventFilter(object, event);
}

void MainWindow::slotDelayedInit()
{
    Q_FOREACH (const Page *page, mPages) {
        // Connect from this to pages - this is really just a way to avoid writing a loop at emit time...
        connect(this, SIGNAL(resourceSelected(QByteArray)),
                page, SLOT(slotResourceSelectionChanged(QByteArray)));
        connect(this, SIGNAL(onlineStatusChanged(bool)),
                page, SLOT(slotOnlineStatusChanged(bool)));
    }

    // initialize additional UI

    setupResourcesCombo();

    // no parent so it can have its own Akonadi-not-started overlay (bug in Akonadi::ErrorOverlay, fixed in f5f76cc, kdepimlibs >= 4.14.10)
    mResourceDialog = new ResourceConfigDialog;

    connect(mResourceDialog, SIGNAL(resourceSelected(Akonadi::AgentInstance)),
            this, SLOT(slotResourceSelected(Akonadi::AgentInstance)));

    connect(mCollectionManager, SIGNAL(collectionResult(QString,Akonadi::Collection)),
            this, SLOT(slotCollectionResult(QString,Akonadi::Collection)));

    initialResourceSelection();

    connect(Akonadi::ServerManager::self(), SIGNAL(started()),
            SLOT(slotServerStarted()));

    connect(AgentManager::self(), SIGNAL(instanceError(Akonadi::AgentInstance,QString)),
            this, SLOT(slotResourceError(Akonadi::AgentInstance,QString)));
    connect(AgentManager::self(), SIGNAL(instanceOnline(Akonadi::AgentInstance,bool)),
            this, SLOT(slotResourceOnline(Akonadi::AgentInstance,bool)));
    connect(AgentManager::self(), SIGNAL(instanceProgressChanged(Akonadi::AgentInstance)),
            this, SLOT(slotResourceProgress(Akonadi::AgentInstance)));
    connect(AgentManager::self(), SIGNAL(instanceStatusChanged(Akonadi::AgentInstance)),
            this, SLOT(slotResourceProgress(Akonadi::AgentInstance)));

    Akonadi::AttributeFactory::registerAttribute<EnumDefinitionAttribute>();
}

void MainWindow::slotAboutApp()
{
    QMessageBox::about(this, i18n("About FatCRM"), i18n("A desktop application for SugarCRM\n\nVersion %1\n\n(C) 2010-2017 Klarälvdalens Datakonsult AB (KDAB)", QStringLiteral(FATCRM_VERSION_STRING)));
}

void MainWindow::initialize(bool displayOverlay)
{
    Q_INIT_RESOURCE(icons);

    mLoadingOverlay = new LoadingOverlay(mUi.tabWidget);
    if (!displayOverlay)
        slotHideOverlay();

    resize(900, 900);
    createActions();
    createTabs();
    setupActions();
    // initialize view actions
    mUi.actionSynchronize->setEnabled(false);
    mUi.actionFullReload->setEnabled(false);
    mUi.actionOfflineMode->setEnabled(false);

    mProgressBar = new QProgressBar(this);
    mProgressBar->setRange(0, 100);
    mProgressBar->setMaximumWidth(100);
    statusBar()->addPermanentWidget(mProgressBar);
    mProgressBar->hide();

    mProgressBarHideTimer = new QTimer(this);
    mProgressBarHideTimer->setInterval(1000);
    connect(mProgressBarHideTimer, SIGNAL(timeout()), mProgressBar, SLOT(hide()));

    connect(mLinkedItemsRepository, SIGNAL(notesLoaded(int)),
            this, SLOT(slotNotesLoaded(int)));
    connect(mLinkedItemsRepository, SIGNAL(emailsLoaded(int)),
            this, SLOT(slotEmailsLoaded(int)));
    connect(mLinkedItemsRepository, SIGNAL(documentsLoaded(int)),
            this, SLOT(slotDocumentsLoaded(int)));
}

void MainWindow::createActions()
{
    // the other menus are handled in Qt Designer

    mViewMenu = new QMenu(i18n("&View"), this);
    menuBar()->insertMenu(mUi.menuSettings->menuAction(), mViewMenu);
    QAction *printAction = new QAction(i18n("Print Report..."), this);
    printAction->setShortcut(QKeySequence::Print);
    printAction->setIcon(QIcon(":/icons/document-print-preview.png"));
    connect(printAction, SIGNAL(triggered()), this, SLOT(slotPrintReport()));
    mViewMenu->addAction(printAction);
    mViewMenu->addSeparator();

    mMainToolBar = addToolBar(i18n("Main ToolBar"));
    mResourceSelector = new QComboBox(this);
    mResourceSelectorAction = mMainToolBar->addWidget(mResourceSelector);
    mMainToolBar->addAction(mUi.actionSynchronize);

    mMainToolBar->addAction(printAction);
}

void MainWindow::setupActions()
{
    const QIcon reloadIcon =
        (style() != nullptr ? style()->standardIcon(QStyle::SP_BrowserReload, nullptr, nullptr)
         : QIcon());
    if (!reloadIcon.isNull()) {
        mUi.actionSynchronize->setIcon(reloadIcon);
    }

    connect(mUi.actionCRMAccounts, SIGNAL(triggered()), SLOT(slotConfigureResources()));
    connect(mUi.actionImportContacts, SIGNAL(triggered()), this, SLOT(slotImportContacts()));
    connect(mUi.actionOfflineMode, SIGNAL(toggled(bool)), this, SLOT(slotToggleOffline(bool)));
    connect(mUi.actionSynchronize, SIGNAL(triggered()), this, SLOT(slotSynchronize()));
    connect(mUi.actionFullReload, SIGNAL(triggered()), this, SLOT(slotFullReload()));
    connect(mUi.actionQuit, SIGNAL(triggered()), this, SLOT(close()));

    connect(mUi.actionAboutFatCRM, SIGNAL(triggered()), this, SLOT(slotAboutApp()));

    connect(mUi.actionConfigureFatCRM, SIGNAL(triggered()), this, SLOT(slotConfigure()));

    Q_FOREACH (const Page *page, mPages) {
        connect(page, SIGNAL(statusMessage(QString)),
                this, SLOT(slotShowMessage(QString)));
        connect(page, SIGNAL(modelLoaded(DetailsType)),
                this, SLOT(slotModelLoaded(DetailsType)));
        connect(page, SIGNAL(synchronizeCollection(Akonadi::Collection)),
                this, SLOT(slotSynchronizeCollection(Akonadi::Collection)));
        connect(page, SIGNAL(openObject(DetailsType,QString)),
                this, SLOT(slotOpenObject(DetailsType,QString)));
    }
}

void MainWindow::slotResourceSelectionChanged(int index)
{
    if (mDisplayOverlay) {
        mLoadingOverlay->show();
        mLoadingOverlay->setMessage(i18n("Loading..."));
    }
    AgentInstance agent = mResourceSelector->itemData(index, AgentInstanceModel::InstanceRole).value<AgentInstance>();
    if (agent.isValid()) {
        const QByteArray identifier = agent.identifier().toLatin1();
        ClientSettings::self()->setDefaultResourceId(identifier);
        emit resourceSelected(identifier);
        slotResourceOnline(agent, agent.isOnline());
        mUi.actionSynchronize->setEnabled(true);
        mUi.actionFullReload->setEnabled(true);
        mUi.actionOfflineMode->setEnabled(true);
        mResourceDialog->resourceSelectionChanged(agent);
        slotResourceProgress(agent);
        ReferencedData::clearAll();
        AccountRepository::instance()->clear();
        mLinkedItemsRepository->clear();
        mCollectionManager->setResource(identifier);
        slotShowMessage(i18n("(0/5) Listing folders..."));
    } else {
        mUi.actionSynchronize->setEnabled(false);
        mUi.actionFullReload->setEnabled(false);
        mUi.actionOfflineMode->setEnabled(false);
    }
}

void MainWindow::slotResourceSelected(const Akonadi::AgentInstance &resource)
{
    for (int index = 0; index < mResourceSelector->count(); ++index) {
        const AgentInstance agent = mResourceSelector->itemData(index, AgentInstanceModel::InstanceRole).value<AgentInstance>();
        if (agent.isValid() && agent == resource) {
            mResourceSelector->setCurrentIndex(index);
            slotResourceSelectionChanged(index);
            return;
        }
    }
}

void MainWindow::slotServerStarted()
{
    qCDebug(FATCRM_CLIENT_LOG) << "Akonadi server started. Resource selector has" << mResourceSelector->count() << "items";
    if (mResourceSelector->count() > 0) {
        initialResourceSelection();
    }
}

void MainWindow::slotResourceCountChanged()
{
    // an empty combo or a combo with one item just looks stupid
    mResourceSelectorAction->setVisible(mResourceSelector->count() > 1);
}

void MainWindow::slotToggleOffline(bool offline)
{
    AgentInstance currentAgent = currentResource();
    const bool online = !offline;
    if (currentAgent.isValid() && currentAgent.isOnline() != online) {
        qCDebug(FATCRM_CLIENT_LOG) << "setting agent" << currentAgent.identifier() << "to online=" << online;
        currentAgent.setIsOnline(online);
    }
}

void MainWindow::slotSynchronize()
{
    AgentInstance currentAgent = currentResource();
    if (currentAgent.isValid()) {
        if (!currentAgent.isOnline())
            currentAgent.setIsOnline(true);
        currentAgent.synchronize();
    }
}

void MainWindow::slotSynchronizeCollection(const Collection &collection)
{
    AgentInstance currentAgent = currentResource();
    if (currentAgent.isValid()) {
        if (!currentAgent.isOnline())
            currentAgent.setIsOnline(true);
        AgentManager::self()->synchronizeCollection(collection);
    }
}

void MainWindow::slotFullReload()
{
    // Kick off one collection-modify job per collection, to clear the timestamp attribute
    // Once all these jobs are done, we'll trigger a resource synchronization
    mClearTimestampJobs = mCollectionManager->clearTimestamps();
    Q_FOREACH (KJob *modJob, mClearTimestampJobs) {
        connect(modJob, SIGNAL(result(KJob*)), this, SLOT(slotClearTimestampResult(KJob*)));
    }
}

void MainWindow::slotShowMessage(const QString &message)
{
    qCDebug(FATCRM_CLIENT_LOG) << message;
    statusBar()->showMessage(message);
}

void MainWindow::slotModelLoaded(DetailsType type)
{
    // We load Opps, Accounts, Contacts, Notes and Emails in this order (see CollectionManager)
    //qCDebug(FATCRM_CLIENT_LOG) << typeToString(type) << "loaded";
    switch (type)
    {
    case Account:
        slotShowMessage(i18n("(2/6) Loading opportunities..."));
        break;
    case Opportunity:
        slotShowMessage(i18n("(3/6) Loading contacts..."));
        break;
    case Contact:
        slotShowMessage(i18n("(4/6) Loading notes..."));
        ReferencedData::emitInitialLoadingDoneForAll(); // fill combos
        slotHideOverlay();
        mLinkedItemsRepository->loadNotes();
        break;
    case Lead:
    case Campaign:
        // currently unused
        break;
    }
}

void MainWindow::slotNotesLoaded(int count)
{
    Q_UNUSED(count);
    slotShowMessage(i18n("(5/6) Loading emails..."));
    mLinkedItemsRepository->loadEmails();
}

void MainWindow::slotEmailsLoaded(int count)
{
    Q_UNUSED(count);
    slotShowMessage(i18n("(6/6) Loading documents..."));
    mLinkedItemsRepository->loadDocuments();
}

void MainWindow::slotDocumentsLoaded(int count)
{
    Q_UNUSED(count);
    initialLoadingDone();
}

void MainWindow::initialLoadingDone()
{
    mInitialLoadingDone = true;

    mLinkedItemsRepository->monitorChanges();
    slotShowMessage(i18n("Ready"));

    // Now is a good time to synchronize data from the server,
    // so that users don't start working on outdated data for the next 30 minutes.
    slotSynchronize();

    processPendingImports();
}

void MainWindow::addPage(Page *page)
{
    page->setCollectionManager(mCollectionManager);
    page->setLinkedItemsRepository(mLinkedItemsRepository);
    mPages << page;
}

void MainWindow::createTabs()
{
    mAccountPage = new AccountsPage(this);
    addPage(mAccountPage);
    mUi.tabWidget->addTab(mAccountPage, i18n("&Accounts"));

    OpportunitiesPage *opportunitiesPage = new OpportunitiesPage(this);
    addPage(opportunitiesPage);
    mUi.tabWidget->addTab(opportunitiesPage, i18n("&Opportunities"));

    connect(mAccountPage, &AccountsPage::requestNewOpportunity, opportunitiesPage, &OpportunitiesPage::createOpportunity);
    connect(opportunitiesPage, SIGNAL(modelCreated(ItemsTreeModel*)), this, SLOT(slotOppModelCreated(ItemsTreeModel*)));

#if 0
    Page *page = new LeadsPage(this);
    addPage(page);
    mUi.tabWidget->addTab(page, i18n("&Leads"));
#endif

    mContactsPage = new ContactsPage(this);
    addPage(mContactsPage);
    mUi.tabWidget->addTab(mContactsPage, i18n("&Contacts"));

    connect(mContactsPage, SIGNAL(modelCreated(ItemsTreeModel*)), this, SLOT(slotContactsModelCreated(ItemsTreeModel*)));

#if 0
    page = new CampaignsPage(this);
    addPage(page);
    mUi.tabWidget->addTab(page, i18n("&Campaigns"));
#endif

    mReportPage = new ReportPage(this);
    mUi.tabWidget->addTab(mReportPage, i18n("&Reports"));

    //set Opportunities page as current
    mUi.tabWidget->setCurrentIndex(1);
}

void MainWindow::slotConfigureResources()
{
    mResourceDialog->show();
    mResourceDialog->raise();
}

void MainWindow::setupResourcesCombo()
{
    // monitor Akonadi agents so we can check for KDCRM specific resources
    AgentInstanceModel *agentModel = new AgentInstanceModel(this);
    AgentFilterProxyModel *agentFilterModel = new AgentFilterProxyModel(this);
    agentFilterModel->addCapabilityFilter(QStringLiteral("KDCRM").toLatin1());
    agentFilterModel->setSourceModel(agentModel);

    mResourceSelector->setModel(agentFilterModel);

    connect(mResourceSelector, SIGNAL(activated(int)),
            this, SLOT(slotResourceSelectionChanged(int)));
    connect(mResourceSelector->model(), SIGNAL(rowsInserted(QModelIndex,int,int)),
            this, SLOT(slotResourceCountChanged()));
    connect(mResourceSelector->model(), SIGNAL(rowsRemoved(QModelIndex,int,int)),
            this, SLOT(slotResourceCountChanged()));
    slotResourceCountChanged();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    event->ignore();

    Q_FOREACH (Page *page, mPages) {
        if(!page->queryClose()) {
            return;
        }
    }

    event->accept();
}

void MainWindow::slotResourceError(const AgentInstance &resource, const QString &message)
{
    const AgentInstance currentAgent = currentResource();
    if (currentAgent.isValid() && currentAgent.identifier() == resource.identifier()) {
        slotShowMessage(message);
    }
}

void MainWindow::updateWindowTitle(bool online)
{
    if (mResourceSelector->count() == 1) {
        setWindowTitle(online ? i18n("FatCRM") : i18n("FatCRM (offline)"));
    } else {
        const int index = mResourceSelector->currentIndex();
        const QString context = mResourceSelector->itemText(index);
        const QString contextTitle =
                online ? i18n("FatCRM: %1", context)
                       : i18n("FatCRM: %1 (offline)", context);
        setWindowTitle(contextTitle);
    }
}

void MainWindow::slotResourceOnline(const AgentInstance &resource, bool online)
{
    const AgentInstance currentAgent = currentResource();
    if (currentAgent.isValid() && currentAgent.identifier() == resource.identifier()) {
        updateWindowTitle(online);
        mUi.actionOfflineMode->setChecked(!online);
        mUi.actionImportContacts->setEnabled(online);
        emit onlineStatusChanged(online); // update details dialog
    }
}

void MainWindow::slotResourceProgress(const AgentInstance &resource)
{
    const AgentInstance currentAgent = currentResource();
    if (currentAgent.isValid() && currentAgent.identifier() == resource.identifier()) {
        const int progress = resource.progress();
        const QString message = resource.statusMessage();
        AgentInstance::Status status = resource.status();
        if (status == AgentInstance::Broken
                || status == AgentInstance::NotConfigured
                || !resource.isOnline()) {
            mProgressBar->hide();
            if (!mUi.actionOfflineMode->isChecked()) {
                statusBar()->showMessage(message);
                return;
            }
        } else if (status == AgentInstance::Idle) {
            statusBar()->clearMessage();
            mProgressBarHideTimer->start();
        } else {
            qCDebug(FATCRM_CLIENT_LOG) << progress << message;
            mProgressBar->show();
            mProgressBar->setValue(progress);
            if (progress == 100) {
                mProgressBarHideTimer->start();
            } else {
                mProgressBarHideTimer->stop();
            }
        }
        if (!message.isEmpty()) {
            statusBar()->showMessage(message);
        }
    }
}

void MainWindow::slotImportContacts()
{
    const QString csvFile = QFileDialog::getOpenFileName(this, i18n("Select contacts file"), QString(), QStringLiteral("*.csv"));
    if (!csvFile.isEmpty()) {
        slotImportCsvFile(csvFile);
    }
}

void MainWindow::slotConfigure()
{
    ConfigurationDialog dlg;
    dlg.exec();
}

void MainWindow::slotPrintReport()
{
    Page *page = currentPage();
    if (page)
        page->printReport();
}

void MainWindow::slotCollectionResult(const QString &mimeType, const Collection &collection)
{
    if (mimeType == SugarAccount::mimeType()) {
        slotShowMessage(i18n("(1/6) Loading accounts..."));
    }
    foreach(Page *page, mPages) {
        if (page->mimeType() == mimeType) {
            page->setCollection(collection);
            return;
        }
    }
    if (mimeType == SugarNote::mimeType()) {
        mLinkedItemsRepository->setNotesCollection(collection);
    } else if (mimeType == SugarEmail::mimeType()) {
        mLinkedItemsRepository->setEmailsCollection(collection);
    } else if (mimeType == SugarDocument::mimeType()) {
        mLinkedItemsRepository->setDocumentsCollection(collection);
    }
}

void MainWindow::slotOppModelCreated(ItemsTreeModel *model)
{
    mReportPage->setOppModel(model);
}

void MainWindow::slotContactsModelCreated(ItemsTreeModel *model)
{
    mContactsModel = model;
}

void MainWindow::slotOpenObject(DetailsType type, const QString &id)
{
    Page *page = pageForType(type);
    if (page) {
        page->openWidget(id);
    } else {
        qCDebug(FATCRM_CLIENT_LOG) << "No page for type" << type;
    }
}

void MainWindow::slotClearTimestampResult(KJob *job)
{
    mClearTimestampJobs.removeAll(job);
    if (mClearTimestampJobs.isEmpty()) {
        slotSynchronize();
    }
}

void MainWindow::slotTryImportCsvFile(const QString &filePath)
{
    if (mInitialLoadingDone)
        slotImportCsvFile(filePath);
    else
        mPendingImportPaths.append(filePath);
}

void MainWindow::slotImportCsvFile(const QString &filePath)
{
    ContactsImporter importer;
    if (importer.importFile(filePath)) {
        const QVector<ContactsSet> contacts = importer.contacts();
        // non modal so that we can use FatCRM to search for accounts/contacts.
        // no parent widget so it can be minimized separately, shows up separate in the taskbar, etc.
        ContactsImportWizard *importWizard = new ContactsImportWizard(nullptr);
        importWizard->setAccountCollection(mAccountPage->collection());
        importWizard->setContactsCollection(mContactsPage->collection());
        importWizard->setImportedContacts(contacts);
        importWizard->setContactsModel(mContactsModel);
        connect(importWizard, SIGNAL(openFutureContact(Akonadi::Item::Id)),
                mContactsPage, SLOT(slotOpenFutureContact(Akonadi::Item::Id)));
        importWizard->show();
        raiseMainWindowAndDialog(importWizard);
    } else {
        QMessageBox::warning(this, i18nc("@title:window", "Failed to import CSV file"),
                             i18n("Error importing %1", filePath));
    }
}

Page *MainWindow::currentPage() const
{
    const int index = mUi.tabWidget->currentIndex();
    if (index >= 0 && index <= mPages.count())
        return mPages[ index ];
    return nullptr;
}

AgentInstance MainWindow::currentResource() const
{
    const int index = mResourceSelector->currentIndex();
    return mResourceSelector->itemData(index, AgentInstanceModel::InstanceRole).value<AgentInstance>();
}

void MainWindow::initialResourceSelection()
{
    const int selectors = mResourceSelector->count();
    if (selectors == 1) {
        slotResourceSelectionChanged(mResourceSelector->currentIndex());
        mResourceDialog->hide();
    } else if (selectors == 0) {
        mLoadingOverlay->setMessage(i18n("Configure a SugarCRM resource in order to use FatCRM."));
        showResourceDialog();
    } else {
        const int resourceIndex = resourceIndexFor(ClientSettings::self()->defaultResourceId());
        if (resourceIndex != -1) {
            mResourceSelector->setCurrentIndex(resourceIndex);
            slotResourceSelectionChanged(resourceIndex);
            mResourceDialog->hide();
            return;
        }

        mResourceSelector->setCurrentIndex(-1);
        mLoadingOverlay->setMessage(i18n("Choose a SugarCRM resource."));
        showResourceDialog();
    }
}

void MainWindow::showResourceDialog()
{
    mResourceDialog->show();
    // delay mResourceDialog->raise() so it happens after MainWindow::show() (from main.cpp)
    // This is part of the "mResourceDialog has no parent" workaround
    QMetaObject::invokeMethod(mResourceDialog, "raise", Qt::QueuedConnection);
}

int MainWindow::resourceIndexFor(const QString &id) const
{
    if (id.isEmpty())
        return -1;

    for (int index = 0; index < mResourceSelector->count(); ++index) {
        const AgentInstance agent = mResourceSelector->itemData(index, AgentInstanceModel::InstanceRole).value<AgentInstance>();
        if (agent.isValid() && (agent.identifier() == id)) {
            return index;
        }
    }

    return -1;
}

void MainWindow::raiseMainWindowAndDialog(QWidget *dialog)
{
    setWindowState(windowState() & ~Qt::WindowMinimized);
    raise();

    dialog->raise();
    dialog->activateWindow();
}

Page *MainWindow::pageForType(DetailsType type) const
{
    foreach(Page *page, mPages) {
        if (page->detailsType() == type) {
            return page;
        }
    }
    return nullptr;
}

void MainWindow::processPendingImports()
{
    foreach (const QString &filePath, mPendingImportPaths)
        slotImportCsvFile(filePath);

    mPendingImportPaths.clear();
}

void MainWindow::slotHideOverlay()
{
    mLoadingOverlay->hide();
}
