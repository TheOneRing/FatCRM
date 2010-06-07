#ifndef CAMPAIGNSPAGE_H
#define CAMPAIGNSPAGE_H

#include "ui_campaignspage.h"

#include <akonadi/collection.h>
#include <akonadi/entitytreemodel.h>

#include <QtGui/QWidget>

namespace Akonadi
{
    class ChangeRecorder;
    class Item;
    class ItemModel;
}

class KJob;
class QModelIndex;

class CampaignsPage : public QWidget
{
    Q_OBJECT
public:
    explicit CampaignsPage( QWidget *parent = 0 );

    ~CampaignsPage();

     void addCampaignsData();

Q_SIGNALS:
    void campaignItemChanged();
    void statusMessage( const QString& );
    void showDetails();

public Q_SLOTS:
    void syncronize();

private:
    void initialize();
    void setupCachePolicy();

    Akonadi::ChangeRecorder *mChangeRecorder;
    Akonadi::Collection mCampaignsCollection;
    Ui_CampaignsPage mUi;

private Q_SLOTS:
    void slotResourceSelectionChanged( const QByteArray &identifier );
    void slotCollectionFetchResult( KJob *job );
    void slotCampaignChanged( const Akonadi::Item &item );
    void slotNewCampaignClicked();
    void slotFilterChanged( const QString& text );
    void slotAddCampaign();
    void slotModifyCampaign();
    void slotRemoveCampaign();
    void slotSetCurrent( const QModelIndex&,int,int );
    void cachePolicyJobCompleted( KJob* );
};

#endif /* CAMPAIGNSPAGE_H */

