#include "overviewpage.h"
#include "ui_overviewpage.h"

#include "clientmodel.h"
#include "sandstorm.h"
#include "sandstormconfig.h"
#include "walletmodel.h"
#include "darksilkunits.h"
#include "optionsmodel.h"
#include "transactiontablemodel.h"
#include "transactionfilterproxy.h"
#include "guiutil.h"
#include "guiconstants.h"

#include <QAbstractItemDelegate>
#include <QPainter>
#include <QTimer>
#include <QDebug>
#include <QScrollArea>
#include <QScroller>

#define DECORATION_SIZE 64
#define NUM_ITEMS 6

class TxViewDelegate : public QAbstractItemDelegate
{
    Q_OBJECT
public:
    TxViewDelegate(): QAbstractItemDelegate(), unit(DarkSilkUnits::DRKSLK)
    {

    }

    inline void paint(QPainter *painter, const QStyleOptionViewItem &option,
                      const QModelIndex &index ) const
    {
        painter->save();

        QIcon icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
        QRect mainRect = option.rect;
        QRect decorationRect(mainRect.topLeft(), QSize(DECORATION_SIZE, DECORATION_SIZE));
        int xspace = DECORATION_SIZE + 8;
        int ypad = 6;
        int halfheight = (mainRect.height() - 2*ypad)/2;
        QRect amountRect(mainRect.left() + xspace, mainRect.top()+ypad, mainRect.width() - xspace, halfheight);
        QRect addressRect(mainRect.left() + xspace, mainRect.top()+ypad+halfheight, mainRect.width() - xspace, halfheight);
        icon.paint(painter, decorationRect);

        QDateTime date = index.data(TransactionTableModel::DateRole).toDateTime();
        QString address = index.data(Qt::DisplayRole).toString();
        qint64 amount = index.data(TransactionTableModel::AmountRole).toLongLong();
        bool confirmed = index.data(TransactionTableModel::ConfirmedRole).toBool();
        QVariant value = index.data(Qt::ForegroundRole);
        QColor foreground = option.palette.color(QPalette::Text);
        if(qVariantCanConvert<QColor>(value))
        {
            foreground = qvariant_cast<QColor>(value);
        }

        painter->setPen(fUseBlackTheme ? QColor(255, 255, 255) : foreground);
        painter->drawText(addressRect, Qt::AlignLeft|Qt::AlignVCenter, address);

        if(amount < 0)
        {
            foreground = COLOR_NEGATIVE;
        }
        else if(!confirmed)
        {
            foreground = COLOR_UNCONFIRMED;
        }
        else
        {
            foreground = option.palette.color(QPalette::Text);
        }
        painter->setPen(fUseBlackTheme ? QColor(255, 255, 255) : foreground);
        QString amountText = DarkSilkUnits::formatWithUnit(unit, amount, true);
        if(!confirmed)
        {
            amountText = QString("[") + amountText + QString("]");
        }
        painter->drawText(amountRect, Qt::AlignRight|Qt::AlignVCenter, amountText);

        painter->setPen(fUseBlackTheme ? QColor(80, 0, 120) : option.palette.color(QPalette::Text));
        painter->drawText(amountRect, Qt::AlignLeft|Qt::AlignVCenter, GUIUtil::dateTimeStr(date));

        painter->restore();
    }

    inline QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
    {
        return QSize(DECORATION_SIZE, DECORATION_SIZE);
    }

    int unit;

};
#include "overviewpage.moc"

OverviewPage::OverviewPage(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::OverviewPage),
    clientModel(0),
    walletModel(0),
    currentBalance(-1),
    currentStake(0),
    currentUnconfirmedBalance(-1),
    currentImmatureBalance(-1),
    txdelegate(new TxViewDelegate()),
    filter(0)
{
    ui->setupUi(this);

    ui->frameSandstorm->setVisible(true);

    QScroller::grabGesture(ui->scrollArea, QScroller::LeftMouseButtonGesture);
    ui->scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    ui->scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    ui->columnTwoWidget->setContentsMargins(0,0,0,0);
    ui->columnTwoWidget->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    ui->columnTwoWidget->setMinimumWidth(300);

    // Recent transactions
    ui->listTransactions->setItemDelegate(txdelegate);
    ui->listTransactions->setIconSize(QSize(DECORATION_SIZE, DECORATION_SIZE));
    ui->listTransactions->setMinimumHeight(NUM_ITEMS * (DECORATION_SIZE + 2));
    ui->listTransactions->setAttribute(Qt::WA_MacShowFocusRect, false);

    connect(ui->listTransactions, SIGNAL(clicked(QModelIndex)), this, SLOT(handleTransactionClicked(QModelIndex)));

    // init "out of sync" warning labels
    ui->labelWalletStatus->setText("(" + tr("out of sync") + ")");
    ui->labelTransactionsStatus->setText("(" + tr("out of sync") + ")");

    showingSandStormMessage = 0;
    sandstormActionCheck = 0;
    lastNewBlock = 0;

    if(fLiteMode){
        ui->frameSandstorm->setVisible(true);
    } else {
	qDebug() << "Sandstorm Status Timer";
        timer = new QTimer(this);
        connect(timer, SIGNAL(timeout()), this, SLOT(sandStormStatus()));
        timer->start(60000);
    }

    if(fStormNode || fLiteMode){
        ui->toggleSandstorm->setText("(" + tr("Disabled") + ")");
        ui->sandstormAuto->setText("(" + tr("Disabled") + ")");
        ui->sandstormReset->setText("(" + tr("Disabled") + ")");
        ui->toggleSandstorm->setEnabled(false);
    }else if(!fEnableSandstorm){
        ui->toggleSandstorm->setText(tr("Start Sandstorm"));
    } else {
        ui->toggleSandstorm->setText(tr("Stop Sandstorm"));
    }

    // start with displaying the "out of sync" warnings
    showOutOfSyncWarning(true);

    if (fUseBlackTheme)
    {
        const char* whiteLabelQSS = "QLabel { color: rgb(255,255,255); }";
        ui->labelBalance->setStyleSheet(whiteLabelQSS);
        ui->labelStake->setStyleSheet(whiteLabelQSS);
        ui->labelUnconfirmed->setStyleSheet(whiteLabelQSS);
        ui->labelImmature->setStyleSheet(whiteLabelQSS);
        ui->labelTotal->setStyleSheet(whiteLabelQSS);
    }
}

void OverviewPage::handleTransactionClicked(const QModelIndex &index)
{
    if(filter)
        emit transactionClicked(filter->mapToSource(index));
}

OverviewPage::~OverviewPage()
{
    if (!fLiteMode && !fStormNode) disconnect(timer, SIGNAL(timeout()), this, SLOT(sandStormStatus()));
    delete ui;
}

void OverviewPage::setBalance(qint64 balance, qint64 stake, qint64 unconfirmedBalance, qint64 immatureBalance, qint64 anonymizedBalance)
{
    int unit = walletModel->getOptionsModel()->getDisplayUnit();
    currentBalance = balance;
    currentStake = stake;
    currentUnconfirmedBalance = unconfirmedBalance;
    currentImmatureBalance = immatureBalance;
    currentAnonymizedBalance = anonymizedBalance;
    ui->labelBalance->setText(DarkSilkUnits::formatWithUnit(unit, balance));
    ui->labelStake->setText(DarkSilkUnits::formatWithUnit(unit, stake));
    ui->labelUnconfirmed->setText(DarkSilkUnits::formatWithUnit(unit, unconfirmedBalance));
    ui->labelImmature->setText(DarkSilkUnits::formatWithUnit(unit, immatureBalance));
    ui->labelTotal->setText(DarkSilkUnits::formatWithUnit(unit, balance + stake + unconfirmedBalance + immatureBalance));
    ui->labelAnonymized->setText(DarkSilkUnits::formatWithUnit(unit, anonymizedBalance));

    // only show immature (newly mined) balance if it's non-zero, so as not to complicate things
    // for the non-mining users
    bool showImmature = immatureBalance != 0;
    ui->labelImmature->setVisible(showImmature);
    ui->labelImmatureText->setVisible(showImmature);

    if(cachedTxLocks != nCompleteTXLocks){
        cachedTxLocks = nCompleteTXLocks;
        ui->listTransactions->update();
    }
}

void OverviewPage::setClientModel(ClientModel *model)
{
    this->clientModel = model;
    if(model)
    {
        // Show warning if this is a prerelease version
        connect(model, SIGNAL(alertsChanged(QString)), this, SLOT(updateAlerts(QString)));
        updateAlerts(model->getStatusBarWarnings());
    }
}

void OverviewPage::setWalletModel(WalletModel *model)
{
    this->walletModel = model;
    if(model && model->getOptionsModel())
    {
        // Set up transaction list
        filter = new TransactionFilterProxy();
        filter->setSourceModel(model->getTransactionTableModel());
        filter->setLimit(NUM_ITEMS);
        filter->setDynamicSortFilter(true);
        filter->setSortRole(Qt::EditRole);
        filter->setShowInactive(false);
        filter->sort(TransactionTableModel::Status, Qt::DescendingOrder);

        ui->listTransactions->setModel(filter);
        ui->listTransactions->setModelColumn(TransactionTableModel::ToAddress);

        // Keep up to date with wallet
        setBalance(model->getBalance(), model->getStake(), model->getUnconfirmedBalance(), model->getImmatureBalance(), model->getAnonymizedBalance());
        connect(model, SIGNAL(balanceChanged(qint64, qint64, qint64, qint64, qint64)), this, SLOT(setBalance(qint64, qint64, qint64, qint64, qint64)));

        connect(model->getOptionsModel(), SIGNAL(displayUnitChanged(int)), this, SLOT(updateDisplayUnit()));

        connect(ui->sandstormAuto, SIGNAL(clicked()), this, SLOT(sandstormAuto()));
        connect(ui->sandstormReset, SIGNAL(clicked()), this, SLOT(sandstormReset()));
        connect(ui->toggleSandstorm, SIGNAL(clicked()), this, SLOT(toggleSandstorm()));
    }

    // update the display unit, to not use the default ("DRKSLK")
    updateDisplayUnit();
}

void OverviewPage::updateDisplayUnit()
{
    if(walletModel && walletModel->getOptionsModel())
    {
        if(currentBalance != -1)
            setBalance(currentBalance, walletModel->getStake(), currentUnconfirmedBalance, currentImmatureBalance, currentAnonymizedBalance);

        // Update txdelegate->unit with the current unit
        txdelegate->unit = walletModel->getOptionsModel()->getDisplayUnit();

        ui->listTransactions->update();
    }
}

void OverviewPage::updateAlerts(const QString &warnings)
{
    this->ui->labelAlerts->setVisible(!warnings.isEmpty());
    this->ui->labelAlerts->setText(warnings);
}

void OverviewPage::showOutOfSyncWarning(bool fShow)
{
    ui->labelWalletStatus->setVisible(fShow);
    ui->labelTransactionsStatus->setVisible(fShow);
}



void OverviewPage::updateSandstormProgress()
{
    qDebug() << "updateSandstormProgress()";
    if(IsInitialBlockDownload()) return;
    
    qDebug() << "updateSandstormProgress() getbalance";
    int64_t nBalance = pwalletMain->GetBalance();
    if(nBalance == 0)
    {
        ui->sandstormProgress->setValue(0);
        QString s(tr("No inputs detected"));
        ui->sandstormProgress->setToolTip(s);
        return;
    }

    //get denominated unconfirmed inputs
    if(pwalletMain->GetDenominatedBalance(true, true) > 0)
    {
        QString s(tr("Found unconfirmed denominated outputs, will wait till they confirm to recalculate."));
        ui->sandstormProgress->setToolTip(s);
        return;
    }

    //Get the anon threshold
    int64_t nMaxToAnonymize = nAnonymizeDarkSilkAmount*COIN;

    // If it's more than the wallet amount, limit to that.
    if(nMaxToAnonymize > nBalance) nMaxToAnonymize = nBalance;

    if(nMaxToAnonymize == 0) return;

    // calculate parts of the progress, each of them shouldn't be higher than 1:
    // mixing progress of denominated balance
    int64_t denominatedBalance = pwalletMain->GetDenominatedBalance();
    float denomPart = 0;
    if(denominatedBalance > 0)
    {
        denomPart = (float)pwalletMain->GetNormalizedAnonymizedBalance() / denominatedBalance;
        denomPart = denomPart > 1 ? 1 : denomPart;
        if(denomPart == 1 && nMaxToAnonymize > denominatedBalance) nMaxToAnonymize = denominatedBalance;
    }

    // % of fully anonymized balance
    float anonPart = 0;
    if(nMaxToAnonymize > 0)
    {
        anonPart = (float)pwalletMain->GetAnonymizedBalance() / nMaxToAnonymize;
        // if anonPart is > 1 then we are done, make denomPart equal 1 too
        anonPart = anonPart > 1 ? (denomPart = 1, 1) : anonPart;
    }

    // apply some weights to them (sum should be <=100) and calculate the whole progress
    int progress = 80 * denomPart + 20 * anonPart;
    if(progress >= 100) progress = 100;

    ui->sandstormProgress->setValue(progress);

    std::ostringstream convert;
    convert << "Progress: " << progress << "%, inputs have an average of " << pwalletMain->GetAverageAnonymizedRounds() << " of " << nSandstormRounds << " rounds";
    QString s(convert.str().c_str());
    ui->sandstormProgress->setToolTip(s);
}


void OverviewPage::sandStormStatus()
{
    int nBestHeight = pindexBest->nHeight;

    if(nBestHeight != sandStormPool.cachedNumBlocks)
    {
        //we we're processing lots of blocks, we'll just leave
        if(GetTime() - lastNewBlock < 10) return;
        lastNewBlock = GetTime();

        updateSandstormProgress();

        QString strSettings(" " + tr("Rounds"));
        strSettings.prepend(QString::number(nSandstormRounds)).prepend(" / ");
        strSettings.prepend(DarkSilkUnits::formatWithUnit(
            walletModel->getOptionsModel()->getDisplayUnit(),
            nAnonymizeDarkSilkAmount * COIN)
        );

        ui->labelAmountRounds->setText(strSettings);
    }

    if(!fEnableSandstorm) {
        if(nBestHeight != sandStormPool.cachedNumBlocks)
        {
            sandStormPool.cachedNumBlocks = nBestHeight;

            ui->sandstormEnabled->setText(tr("Disabled"));
            ui->sandstormStatus->setText("");
            ui->toggleSandstorm->setText(tr("Start Sandstorm"));
        }

        return;
    }

    // check sandstorm status and unlock if needed
    if(nBestHeight != sandStormPool.cachedNumBlocks)
    {
        // Balance and number of transactions might have changed
        sandStormPool.cachedNumBlocks = nBestHeight;

        /* *******************************************************/

        ui->sandstormEnabled->setText(tr("Enabled"));
    }

    int state = sandStormPool.GetState();
    int entries = sandStormPool.GetEntriesCount();
    int accepted = sandStormPool.GetLastEntryAccepted();

    /* ** @TODO this string creation really needs some clean ups ---vertoe ** */
    std::ostringstream convert;

    if(state == POOL_STATUS_IDLE) {
        convert << tr("Sandstorm is idle.").toStdString();
    } else if(state == POOL_STATUS_ACCEPTING_ENTRIES) {
        if(entries == 0) {
            if(sandStormPool.strAutoDenomResult.size() == 0){
                convert << tr("Mixing in progress...").toStdString();
            } else {
                convert << sandStormPool.strAutoDenomResult;
            }
            showingSandStormMessage = 0;
        } else if (accepted == 1) {
            convert << tr("Sandstorm request complete: Your transaction was accepted into the pool!").toStdString();
            if(showingSandStormMessage % 10 > 8) {
                sandStormPool.lastEntryAccepted = 0;
                showingSandStormMessage = 0;
            }
        } else {
            if(showingSandStormMessage % 70 <= 40) convert << tr("Submitted following entries to stormnode:").toStdString() << " " << entries << "/" << sandStormPool.GetMaxPoolTransactions();
            else if(showingSandStormMessage % 70 <= 50) convert << tr("Submitted to stormnode, waiting for more entries").toStdString() << " (" << entries << "/" << sandStormPool.GetMaxPoolTransactions() << " ) .";
            else if(showingSandStormMessage % 70 <= 60) convert << tr("Submitted to stormnode, waiting for more entries").toStdString() << " (" << entries << "/" << sandStormPool.GetMaxPoolTransactions() << " ) ..";
            else if(showingSandStormMessage % 70 <= 70) convert << tr("Submitted to stormnode, waiting for more entries").toStdString() << " (" << entries << "/" << sandStormPool.GetMaxPoolTransactions() << " ) ...";
        }
    } else if(state == POOL_STATUS_SIGNING) {
        if(showingSandStormMessage % 70 <= 10) convert << tr("Found enough users, signing ...").toStdString();
        else if(showingSandStormMessage % 70 <= 20) convert << tr("Found enough users, signing ( waiting").toStdString() << ". )";
        else if(showingSandStormMessage % 70 <= 30) convert << tr("Found enough users, signing ( waiting").toStdString() << ".. )";
        else if(showingSandStormMessage % 70 <= 40) convert << tr("Found enough users, signing ( waiting").toStdString() << "... )";
    } else if(state == POOL_STATUS_TRANSMISSION) {
        convert << tr("Transmitting final transaction.").toStdString();
    } else if (state == POOL_STATUS_IDLE) {
        convert << tr("Sandstorm is idle.").toStdString();
    } else if (state == POOL_STATUS_FINALIZE_TRANSACTION) {
        convert << tr("Finalizing transaction.").toStdString();
    } else if(state == POOL_STATUS_ERROR) {
        convert << tr("Sandstorm request incomplete:").toStdString() << " " << sandStormPool.lastMessage << ". " << tr("Will retry...").toStdString();
    } else if(state == POOL_STATUS_SUCCESS) {
        convert << tr("Sandstorm request complete:").toStdString() << " " << sandStormPool.lastMessage;
    } else if(state == POOL_STATUS_QUEUE) {
        if(showingSandStormMessage % 70 <= 50) convert << tr("Submitted to stormnode, waiting in queue").toStdString() << ". )";
        else if(showingSandStormMessage % 70 <= 60) convert << tr("Submitted to stormnode, waiting in queue").toStdString() << ".. )";
        else if(showingSandStormMessage % 70 <= 70) convert << tr("Submitted to stormnode, waiting in queue").toStdString() << "... )";
    } else {
        convert << tr("Unknown state:").toStdString() << " id = " << state;
    }

    if(state == POOL_STATUS_ERROR || state == POOL_STATUS_SUCCESS) sandStormPool.Check();

    QString s(convert.str().c_str());
    s = tr("Last Sandstorm message:\n") + s;

    if(s != ui->sandstormStatus->text())
        LogPrintf("Last Sandstorm message: %s\n", convert.str().c_str());

    ui->sandstormStatus->setText(s);

    if(sandStormPool.sessionDenom == 0){
        ui->labelSubmittedDenom->setText(tr("N/A"));
    } else {
        std::string out;
        sandStormPool.GetDenominationsToString(sandStormPool.sessionDenom, out);
        QString s2(out.c_str());
        ui->labelSubmittedDenom->setText(s2);
    }

    showingSandStormMessage++;
    sandstormActionCheck++;

    // Get SandStorm Denomination Status
}

void OverviewPage::sandstormAuto(){
    sandStormPool.DoAutomaticDenominating();
}

void OverviewPage::sandstormReset(){
    sandStormPool.Reset();

    QMessageBox::warning(this, tr("Sandstorm"),
        tr("Sandstorm was successfully reset."),
        QMessageBox::Ok, QMessageBox::Ok);
}

void OverviewPage::toggleSandstorm(){
    if(!fEnableSandstorm){
        int64_t balance = pwalletMain->GetBalance();
        float minAmount = 1.49 * COIN;
        if(balance < minAmount){
            QString strMinAmount(
                DarkSilkUnits::formatWithUnit(
                    walletModel->getOptionsModel()->getDisplayUnit(),
                    minAmount));
            QMessageBox::warning(this, tr("Sandstorm"),
                tr("Sandstorm requires at least %1 to use.").arg(strMinAmount),
                QMessageBox::Ok, QMessageBox::Ok);
            return;
        }

        // if wallet is locked, ask for a passphrase
        if (walletModel->getEncryptionStatus() == WalletModel::Locked)
        {
            WalletModel::UnlockContext ctx(walletModel->requestUnlock());
            if(!ctx.isValid())
            {
                //unlock was cancelled
                sandStormPool.cachedNumBlocks = 0;
                QMessageBox::warning(this, tr("Sandstorm"),
                    tr("Wallet is locked and user declined to unlock. Disabling Sandstorm."),
                    QMessageBox::Ok, QMessageBox::Ok);
                if (fDebug) LogPrintf("Wallet is locked and user declined to unlock. Disabling Sandstorm.\n");
                return;
            }
        }

    }

    sandStormPool.cachedNumBlocks = 0;
    fEnableSandstorm = !fEnableSandstorm;

    if(!fEnableSandstorm){
        ui->toggleSandstorm->setText(tr("Start Sandstorm"));
    } else {
        ui->toggleSandstorm->setText(tr("Stop Sandstorm"));

        /* show sandstorm configuration if client has defaults set */

        if(nAnonymizeDarkSilkAmount == 0){
            SandstormConfig dlg(this);
            dlg.setModel(walletModel);
            dlg.exec();
        }

        sandStormPool.DoAutomaticDenominating();
    }
}
