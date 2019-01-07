#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "addressbook.h"
#include "ui_confirm.h"
#include "ui_memodialog.h"
#include "settings.h"
#include "rpc.h"

using json = nlohmann::json;

void MainWindow::setupSendTab() {
    // Create the validator for send to/amount fields
    auto amtValidator = new QDoubleValidator(0, 21000000, 8, ui->Amount1);
    amtValidator->setNotation(QDoubleValidator::StandardNotation);
    ui->Amount1->setValidator(amtValidator);

    // Send button
    QObject::connect(ui->sendTransactionButton, &QPushButton::clicked, this, &MainWindow::sendButton);

    // Cancel Button
    QObject::connect(ui->cancelSendButton, &QPushButton::clicked, this, &MainWindow::cancelButton);

    // Input Combobox current text changed
    QObject::connect(ui->inputsCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &MainWindow::inputComboTextChanged);

    // Hook up add address button click
    QObject::connect(ui->addAddressButton, &QPushButton::clicked, this, &MainWindow::addAddressSection);

    // Max available Checkbox
    QObject::connect(ui->Max1, &QCheckBox::stateChanged, this, &MainWindow::maxAmountChecked);

    // The first Address button
    QObject::connect(ui->Address1, &QLineEdit::textChanged, [=] (auto text) {
        this->addressChanged(1, text);
    });

    // The first Memo button
    QObject::connect(ui->MemoBtn1, &QPushButton::clicked, [=] () {
        this->memoButtonClicked(1);
    });
    setMemoEnabled(1, false);
        
    // This is the damnest thing ever. If we do AddressBook::readFromStorage() directly, the whole file
    // doesn't get read. It needs to run in a timer after everything has finished to be able to read
    // the file properly. 
    QTimer::singleShot(2000, [=]() { updateLabelsAutoComplete(); });

    // The first address book button
    QObject::connect(ui->AddressBook1, &QPushButton::clicked, [=] () {
        AddressBook::open(this, ui->Address1);
    });

    // The first Amount button
    QObject::connect(ui->Amount1, &QLineEdit::textChanged, [=] (auto text) {
        this->amountChanged(1, text);
    });

    // Fee amount changed
    // Disable custom fees if settings say no
    ui->minerFeeAmt->setReadOnly(!Settings::getInstance()->getAllowCustomFees());
    QObject::connect(ui->minerFeeAmt, &QLineEdit::textChanged, [=](auto txt) {
        ui->lblMinerFeeUSD->setText(Settings::getUSDFormat(txt.toDouble()));
    });
    ui->minerFeeAmt->setText(Settings::getDecimalString(Settings::getMinerFee()));    

     // Set up focus enter to set fees
    QObject::connect(ui->tabWidget, &QTabWidget::currentChanged, [=] (int pos) {
        if (pos == 1) {
            QString txt = ui->minerFeeAmt->text();
            ui->lblMinerFeeUSD->setText(Settings::getUSDFormat(txt.toDouble()));
        }
    });
    //Fees validator
    auto feesValidator = new QDoubleValidator(0, 1, 8, ui->Amount1);
    feesValidator->setNotation(QDoubleValidator::StandardNotation);
    ui->minerFeeAmt->setValidator(feesValidator);

    // Font for the first Memo label
    QFont f = ui->Address1->font();
    f.setPointSize(f.pointSize() - 1);
    ui->MemoTxt1->setFont(f);
}

void MainWindow::updateLabelsAutoComplete() {
    QList<QString> list;
    auto labels = AddressBook::getInstance()->getAllAddressLabels();
    
    std::transform(labels.begin(), labels.end(), std::back_inserter(list), [=] (auto la) -> QString {
        return la.first % "/" % la.second;
    });
    
    delete labelCompleter;
    labelCompleter = new QCompleter(list, this);
    labelCompleter->setCaseSensitivity(Qt::CaseInsensitive);

    // Then, find all the address fields and update the completer.
    QRegExp re("Address[0-9]+", Qt::CaseInsensitive);
    for (auto target: ui->sendToWidgets->findChildren<QLineEdit *>(re)) {
        target->setCompleter(labelCompleter);
    }
}

void MainWindow::setDefaultPayFrom() {
    auto findMax = [=] (QString startsWith) {
        double max_amt = 0;
        int    idx     = -1;

        for (int i=0; i < ui->inputsCombo->count(); i++) {
            auto addr = ui->inputsCombo->itemText(i);
            if (addr.startsWith(startsWith)) {
                auto amt = rpc->getAllBalances()->value(addr);
                if (max_amt < amt) {
                    max_amt = amt;
                    idx = i;
                }
            }                
        }

        return idx;
    };

    // By default, select the z-address with the most balance from the inputs combo
    auto maxZ = findMax("z");
    if (maxZ >= 0) {
        ui->inputsCombo->setCurrentIndex(maxZ);                
    } else {
        auto maxT = findMax("C");
        maxT  = maxT >= 0 ? maxT : 0;
        ui->inputsCombo->setCurrentIndex(maxT);
    }
};

void MainWindow::inputComboTextChanged(int index) {
    auto addr   = ui->inputsCombo->itemText(index);
    auto bal    = rpc->getAllBalances()->value(addr);
    auto balFmt = Settings::getCMMDisplayFormat(bal);

    ui->sendAddressBalance->setText(balFmt);
    ui->sendAddressBalanceUSD->setText(Settings::getUSDFormat(bal));
}

    
void MainWindow::addAddressSection() {
    int itemNumber = ui->sendToWidgets->children().size() - 1;

    auto verticalGroupBox = new QGroupBox(ui->sendToWidgets);
    verticalGroupBox->setTitle(QString(tr("Recipient ")) % QString::number(itemNumber));
    verticalGroupBox->setObjectName(QString("AddressGroupBox") % QString::number(itemNumber));
    auto sendAddressLayout = new QVBoxLayout(verticalGroupBox);
    sendAddressLayout->setSpacing(6);
    sendAddressLayout->setContentsMargins(11, 11, 11, 11);

    auto horizontalLayout_12 = new QHBoxLayout();
    horizontalLayout_12->setSpacing(6);
    auto label_4 = new QLabel(verticalGroupBox);
    label_4->setText(tr("Address"));
    horizontalLayout_12->addWidget(label_4);

    auto Address1 = new QLineEdit(verticalGroupBox);
    Address1->setObjectName(QString("Address") % QString::number(itemNumber)); 
    Address1->setPlaceholderText(tr("Address"));
    QObject::connect(Address1, &QLineEdit::textChanged, [=] (auto text) {
        this->addressChanged(itemNumber, text);
    });
    Address1->setCompleter(labelCompleter);

    horizontalLayout_12->addWidget(Address1);

    auto addressBook1 = new QPushButton(verticalGroupBox);
    addressBook1->setObjectName(QStringLiteral("AddressBook") % QString::number(itemNumber));
    addressBook1->setText(tr("Address Book"));
    QObject::connect(addressBook1, &QPushButton::clicked, [=] () {
        AddressBook::open(this, Address1);
    });

    horizontalLayout_12->addWidget(addressBook1);

    sendAddressLayout->addLayout(horizontalLayout_12);

    auto horizontalLayout_13 = new QHBoxLayout();
    horizontalLayout_13->setSpacing(6);
        
    auto label_6 = new QLabel(verticalGroupBox);
    label_6->setText(tr("Amount"));
    horizontalLayout_13->addWidget(label_6);

    auto Amount1 = new QLineEdit(verticalGroupBox);
    Amount1->setPlaceholderText(tr("Amount"));    
    Amount1->setObjectName(QString("Amount") % QString::number(itemNumber));   
    Amount1->setBaseSize(QSize(200, 0));
    // Create the validator for send to/amount fields
    auto amtValidator = new QDoubleValidator(0, 21000000, 8, Amount1);
    Amount1->setValidator(amtValidator);
    QObject::connect(Amount1, &QLineEdit::textChanged, [=] (auto text) {
        this->amountChanged(itemNumber, text);
    });

    horizontalLayout_13->addWidget(Amount1);

    auto AmtUSD1 = new QLabel(verticalGroupBox);
    AmtUSD1->setObjectName(QString("AmtUSD") % QString::number(itemNumber));   
    horizontalLayout_13->addWidget(AmtUSD1);

    auto horizontalSpacer_4 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);
    horizontalLayout_13->addItem(horizontalSpacer_4);

    auto MemoBtn1 = new QPushButton(verticalGroupBox);
    MemoBtn1->setObjectName(QString("MemoBtn") % QString::number(itemNumber));
    MemoBtn1->setText(tr("Memo"));    
    // Connect Memo Clicked button
    QObject::connect(MemoBtn1, &QPushButton::clicked, [=] () {
        this->memoButtonClicked(itemNumber);
    });
    horizontalLayout_13->addWidget(MemoBtn1);
    setMemoEnabled(itemNumber, false);

    sendAddressLayout->addLayout(horizontalLayout_13);

    auto MemoTxt1 = new QLabel(verticalGroupBox);
    MemoTxt1->setObjectName(QString("MemoTxt") % QString::number(itemNumber));
    QFont font1 = Address1->font();
    font1.setPointSize(font1.pointSize()-1);
    MemoTxt1->setFont(font1);
    MemoTxt1->setWordWrap(true);
    sendAddressLayout->addWidget(MemoTxt1);

    ui->sendToLayout->insertWidget(itemNumber-1, verticalGroupBox);         

    // Set focus into the address
    Address1->setFocus();

    // Delay the call to scroll to allow the scroll window to adjust
    QTimer::singleShot(10, [=] () {ui->sendToScrollArea->ensureWidgetVisible(ui->addAddressButton);});                
}

void MainWindow::addressChanged(int itemNumber, const QString& text) {   
    auto addr = AddressBook::addressFromAddressLabel(text);
    setMemoEnabled(itemNumber, addr.startsWith("z"));
}

void MainWindow::amountChanged(int item, const QString& text) {
    auto usd = ui->sendToWidgets->findChild<QLabel*>(QString("AmtUSD") % QString::number(item));
    usd->setText(Settings::getUSDFormat(text.toDouble()));
}

void MainWindow::setMemoEnabled(int number, bool enabled) {
    auto memoBtn = ui->sendToWidgets->findChild<QPushButton*>(QString("MemoBtn") % QString::number(number));
     if (enabled) {
        memoBtn->setEnabled(true);
        memoBtn->setToolTip("");
    } else {
        memoBtn->setEnabled(false);
        memoBtn->setToolTip(tr("Only z-addresses can have memos"));
    }
}

void MainWindow::memoButtonClicked(int number, bool includeReplyTo) {
    // Memos can only be used with zAddrs. So check that first
    auto addr = ui->sendToWidgets->findChild<QLineEdit*>(QString("Address") + QString::number(number));
    if (!AddressBook::addressFromAddressLabel(addr->text()).startsWith("z")) {
        QMessageBox msg(QMessageBox::Critical, tr("Memos can only be used with z-addresses"),
        tr("The memo field can only be used with a z-address.\n") + addr->text() + tr("\ndoesn't look like a z-address"),
        QMessageBox::Ok, this);

        msg.exec();
        return;
    }

    // Get the current memo if it exists
    auto memoTxt = ui->sendToWidgets->findChild<QLabel *>(QString("MemoTxt") + QString::number(number));
    QString currentMemo = memoTxt->text();

    Ui_MemoDialog memoDialog;
    QDialog dialog(this);
    memoDialog.setupUi(&dialog);
    Settings::saveRestore(&dialog);

    QObject::connect(memoDialog.memoTxt, &QPlainTextEdit::textChanged, [=] () {
        QString txt = memoDialog.memoTxt->toPlainText();
        memoDialog.memoSize->setText(QString::number(txt.toUtf8().size()) + "/512");

        if (txt.toUtf8().size() <= 512) {
            // Everything is fine
            memoDialog.buttonBox->button(QDialogButtonBox::Ok)->setEnabled(true);
            memoDialog.memoSize->setStyleSheet("");
        }
        else {
           // Overweight
            memoDialog.buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
            memoDialog.memoSize->setStyleSheet("color: red;");
        }
        
    });

    auto fnAddReplyTo = [=, &dialog]() {
        QString replyTo = ui->inputsCombo->currentText();
        if (!Settings::isZAddress(replyTo)) {
            replyTo = rpc->getDefaultSaplingAddress();
            if (replyTo.isEmpty())
                return;
        }
        auto curText = memoDialog.memoTxt->toPlainText();
        if (curText.endsWith(replyTo))
            return;

        memoDialog.memoTxt->setPlainText(curText + "\n" + tr("Reply to") + ":\n" + replyTo);

        // MacOS has a really annoying bug where the Plaintext doesn't refresh when the content is
        // updated. So we do this ugly hack - resize the window slightly to force it to refresh
        dialog.setGeometry(dialog.geometry().adjusted(0,0,0,1));
        dialog.setGeometry(dialog.geometry().adjusted(0,0,0,-1));
    };

    // Insert From Address button
    QObject::connect(memoDialog.btnInsertFrom, &QPushButton::clicked, fnAddReplyTo);

    memoDialog.memoTxt->setPlainText(currentMemo);
    memoDialog.memoTxt->setFocus();

    if (includeReplyTo)
        fnAddReplyTo();

    if (dialog.exec() == QDialog::Accepted) {
        memoTxt->setText(memoDialog.memoTxt->toPlainText());
    }
}

void MainWindow::removeExtraAddresses() {
    // The last one is a spacer, so ignore that
    int totalItems = ui->sendToWidgets->children().size() - 2; 

    // Clear the first recipient fields
    auto addr = ui->sendToWidgets->findChild<QLineEdit*>(QString("Address1"));
    addr->clear();
    auto amt  = ui->sendToWidgets->findChild<QLineEdit*>(QString("Amount1"));
    amt->clear();
    auto amtUSD  = ui->sendToWidgets->findChild<QLabel*>(QString("AmtUSD1"));
    amtUSD->clear();
    auto max  = ui->sendToWidgets->findChild<QCheckBox*>(QString("Max1"));
    max->setChecked(false);
    auto memo = ui->sendToWidgets->findChild<QLabel*>(QString("MemoTxt1"));
    memo->clear();

    // Disable first memo btn
    setMemoEnabled(1, false);

    // Reset the fee
    ui->minerFeeAmt->setText(Settings::getDecimalString(Settings::getMinerFee()));

    // Start the deletion after the first item, since we want to keep 1 send field there all there
    for (int i=1; i < totalItems; i++) {
        auto addressGroupBox = ui->sendToWidgets->findChild<QGroupBox*>(QString("AddressGroupBox") % QString::number(i+1));
            
        delete addressGroupBox;
    }    
}

void MainWindow::maxAmountChecked(int checked) {
    if (checked == Qt::Checked) {
        ui->Amount1->setReadOnly(true);
        if (rpc->getAllBalances() == nullptr) return;
           
        // Calculate maximum amount
        double sumAllAmounts = 0.0;
        // Calculate all other amounts
        int totalItems = ui->sendToWidgets->children().size() - 2;   // The last one is a spacer, so ignore that        
        // Start counting the sum skipping the first one, because the MAX button is on the first one, and we don't
        // want to include it in the sum. 
        for (int i=1; i < totalItems; i++) {
            auto amt  = ui->sendToWidgets->findChild<QLineEdit*>(QString("Amount")  % QString::number(i+1));
            sumAllAmounts += amt->text().toDouble();
        }

        if (Settings::getInstance()->getAllowCustomFees()) {
            sumAllAmounts = ui->minerFeeAmt->text().toDouble();
        }
        else {
            sumAllAmounts += Settings::getMinerFee();
        }

        auto addr = ui->inputsCombo->currentText();

        auto maxamount  = rpc->getAllBalances()->value(addr) - sumAllAmounts;
        maxamount       = (maxamount < 0) ? 0 : maxamount;
            
        ui->Amount1->setText(Settings::getDecimalString(maxamount));
    } else if (checked == Qt::Unchecked) {
        // Just remove the readonly part, don't change the content
        ui->Amount1->setReadOnly(false);
    }
}

// Create a Tx from the current state of the send page. 
Tx MainWindow::createTxFromSendPage() {
    Tx tx;

    bool sendChangeToSapling = Settings::getInstance()->getAutoShield();

    // Gather the from / to addresses 
    tx.fromAddr = ui->inputsCombo->currentText();
    sendChangeToSapling = sendChangeToSapling && Settings::isTAddress(tx.fromAddr);

    // For each addr/amt in the sendTo tab
    int totalItems = ui->sendToWidgets->children().size() - 2;   // The last one is a spacer, so ignore that        
    double totalAmt = 0;
    for (int i=0; i < totalItems; i++) {
        QString addr = ui->sendToWidgets->findChild<QLineEdit*>(QString("Address") % QString::number(i+1))->text().trimmed();
        // Remove label if it exists
        addr = AddressBook::addressFromAddressLabel(addr);
        
        // If address is sprout, then we can't send change to sapling, because of turnstile.
        sendChangeToSapling = sendChangeToSapling && !Settings::getInstance()->isSproutAddress(addr);

        double  amt  = ui->sendToWidgets->findChild<QLineEdit*>(QString("Amount")  % QString::number(i+1))->text().trimmed().toDouble();        
        totalAmt += amt;
        QString memo = ui->sendToWidgets->findChild<QLabel*>(QString("MemoTxt")  % QString::number(i+1))->text().trimmed();
        
        tx.toAddrs.push_back( ToFields{addr, amt, memo, memo.toUtf8().toHex()} );
    }

    if (Settings::getInstance()->getAllowCustomFees()) {
        tx.fee = ui->minerFeeAmt->text().toDouble();
    }
    else {
        tx.fee = Settings::getMinerFee();
    }

    if (Settings::getInstance()->getAutoShield() && sendChangeToSapling) {
        auto saplingAddr = std::find_if(rpc->getAllZAddresses()->begin(), rpc->getAllZAddresses()->end(), [=](auto i) -> bool { 
            // We're finding a sapling address that is not one of the To addresses, because commercium doesn't allow duplicated addresses
            bool isSapling = Settings::getInstance()->isSaplingAddress(i); 
            if (!isSapling) return false;

            // Also check all the To addresses
            for (auto t : tx.toAddrs) {
                if (t.addr == i)
                    return false;
            }

            return true;
        });

        if (saplingAddr != rpc->getAllZAddresses()->end()) {
            double change = rpc->getAllBalances()->value(tx.fromAddr) - totalAmt - tx.fee;

            if (Settings::getDecimalString(change) != "0") {
                QString changeMemo = tr("Change from ") + tx.fromAddr;
                tx.toAddrs.push_back(ToFields{ *saplingAddr, change, changeMemo, changeMemo.toUtf8().toHex() });
            }
        }
    }
    
    return tx;
}

bool MainWindow::confirmTx(Tx tx) {
    auto fnSplitAddressForWrap = [=] (const QString& a) -> QString {
        if (!a.startsWith("z")) return a;

        auto half = a.length() / 2;
        auto splitted = a.left(half) + "\n" + a.right(a.length() - half);
        return splitted;
    };


    // Show a confirmation dialog
    QDialog d(this);
    Ui_confirm confirm;
    confirm.setupUi(&d);

    // Remove all existing address/amt qlabels on the confirm dialog.
    int totalConfirmAddrItems = confirm.sendToAddrs->children().size();
    for (int i = 0; i < totalConfirmAddrItems / 3; i++) {
        auto addr   = confirm.sendToAddrs->findChild<QLabel*>(QString("Addr")   % QString::number(i+1));
        auto amt    = confirm.sendToAddrs->findChild<QLabel*>(QString("Amt")    % QString::number(i+1));
        auto memo   = confirm.sendToAddrs->findChild<QLabel*>(QString("Memo")   % QString::number(i+1));
        auto amtUSD = confirm.sendToAddrs->findChild<QLabel*>(QString("AmtUSD") % QString::number(i+1));
        auto spacer = confirm.sendToAddrs->findChild<QLabel*>(QString("spacer") % QString::number(i+1));

        delete memo;
        delete addr;
        delete amt;
        delete amtUSD;
        delete spacer;
    }

    // Remove the fee labels
    delete confirm.sendToAddrs->findChild<QLabel*>("labelMinerFee");
    delete confirm.sendToAddrs->findChild<QLabel*>("minerFee");
    delete confirm.sendToAddrs->findChild<QLabel*>("minerFeeUSD");
    
    // For each addr/amt/memo, construct the JSON and also build the confirm dialog box    
    int row = 0;
    double totalSpending = 0;

    for (int i=0; i < tx.toAddrs.size(); i++) {
        auto toAddr = tx.toAddrs[i];

        // Add new Address widgets instead of the same one.
        {
            // Address
            auto Addr = new QLabel(confirm.sendToAddrs);
            Addr->setObjectName(QString("Addr") % QString::number(i + 1));
            Addr->setWordWrap(true);
            Addr->setText(fnSplitAddressForWrap(toAddr.addr));
            confirm.gridLayout->addWidget(Addr, row, 0, 1, 1);

            // Amount (CMM)
            auto Amt = new QLabel(confirm.sendToAddrs);
            Amt->setObjectName(QString("Amt") % QString::number(i + 1));
            Amt->setText(Settings::getCMMDisplayFormat(toAddr.amount));
            Amt->setAlignment(Qt::AlignRight | Qt::AlignTrailing | Qt::AlignVCenter);
            confirm.gridLayout->addWidget(Amt, row, 1, 1, 1);
            totalSpending += toAddr.amount;

            // Amount (USD)
            auto AmtUSD = new QLabel(confirm.sendToAddrs);
            AmtUSD->setObjectName(QString("AmtUSD") % QString::number(i + 1));
            AmtUSD->setText(Settings::getUSDFormat(toAddr.amount));
            AmtUSD->setAlignment(Qt::AlignRight | Qt::AlignTrailing | Qt::AlignVCenter);
            confirm.gridLayout->addWidget(AmtUSD, row, 2, 1, 1);            

            // Memo
            if (toAddr.addr.startsWith("z") && !toAddr.txtMemo.isEmpty()) {
                row++;
                auto Memo = new QLabel(confirm.sendToAddrs);
                Memo->setObjectName(QStringLiteral("Memo") % QString::number(i + 1));
                Memo->setText(toAddr.txtMemo);
                QFont font1 = Addr->font();
                font1.setPointSize(font1.pointSize() - 1);
                Memo->setFont(font1);
                Memo->setWordWrap(true);

                confirm.gridLayout->addWidget(Memo, row, 0, 1, 3);
            }

            row ++;

            // Add an empty spacer to create a blank space
            auto spacer = new QLabel(confirm.sendToAddrs);
            spacer->setObjectName(QString("spacer") % QString::number(i + 1));
            confirm.gridLayout->addWidget(spacer, row, 0, 1, 1);

            row++;
        }
    }

    // Add fees
    {
        auto labelMinerFee = new QLabel(confirm.sendToAddrs);
        labelMinerFee->setObjectName(QStringLiteral("labelMinerFee"));
        confirm.gridLayout->addWidget(labelMinerFee, row, 0, 1, 1);
        labelMinerFee->setText(tr("Miner Fee"));

        auto minerFee = new QLabel(confirm.sendToAddrs);
        QSizePolicy sizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        minerFee->setSizePolicy(sizePolicy);
        minerFee->setObjectName(QStringLiteral("minerFee"));
        minerFee->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
        confirm.gridLayout->addWidget(minerFee, row, 1, 1, 1);
        minerFee->setText(Settings::getCMMDisplayFormat(tx.fee));
        totalSpending += tx.fee;

        auto minerFeeUSD = new QLabel(confirm.sendToAddrs);
        QSizePolicy sizePolicy1(QSizePolicy::Minimum, QSizePolicy::Preferred);
        minerFeeUSD->setSizePolicy(sizePolicy1);
        minerFeeUSD->setObjectName(QStringLiteral("minerFeeUSD"));
        minerFeeUSD->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
        confirm.gridLayout->addWidget(minerFeeUSD, row, 2, 1, 1);
        minerFeeUSD->setText(Settings::getUSDFormat(tx.fee));

        if (Settings::getInstance()->getAllowCustomFees() && tx.fee != Settings::getMinerFee()) {
            confirm.warningLabel->setVisible(true);            
        } else {
            // Default fee
            confirm.warningLabel->setVisible(false);
        }
    }

    // Syncing warning
    confirm.syncingWarning->setVisible(Settings::getInstance()->isSyncing());

    // No peers warning
    confirm.nopeersWarning->setVisible(Settings::getInstance()->getPeers() == 0);

    // And FromAddress in the confirm dialog 
    confirm.sendFrom->setText(fnSplitAddressForWrap(tx.fromAddr));
    QString tooltip = tr("Current balance      : ") +
        Settings::getCMMUSDDisplayFormat(rpc->getAllBalances()->value(tx.fromAddr));
    tooltip += "\n" + tr("Balance after this Tx: ") +
        Settings::getCMMUSDDisplayFormat(rpc->getAllBalances()->value(tx.fromAddr) - totalSpending);
    confirm.sendFrom->setToolTip(tooltip);

    // Show the dialog and submit it if the user confirms
    if (d.exec() == QDialog::Accepted) {        
        // Then delete the additional fields from the sendTo tab
        removeExtraAddresses();
        return true;
    } else {
        return false;
    }        
}

// Send button clicked
void MainWindow::sendButton() {
    Tx tx = createTxFromSendPage();

    QString error = doSendTxValidations(tx);
    if (!error.isEmpty()) {
        // Something went wrong, so show an error and exit
        QMessageBox msg(QMessageBox::Critical, tr("Transaction Error"), error,
                        QMessageBox::Ok, this);

        msg.exec();

        // abort the Tx
        return;
    }
    
    // Show a dialog to confirm the Tx
    if (confirmTx(tx)) {
        json params = json::array();
        rpc->fillTxJsonParams(params, tx);
        std::cout << std::setw(2) << params << std::endl;

        // And send the Tx
        rpc->sendZTransaction(params, [=](const json& reply) {
            QString opid = QString::fromStdString(reply.get<json::string_t>());
            ui->statusBar->showMessage(tr("Computing Tx: ") % opid);

            // And then start monitoring the transaction
            rpc->addNewTxToWatch(tx, opid);
        });
    }        
}

QString MainWindow::doSendTxValidations(Tx tx) {

    return QString();
}

void MainWindow::cancelButton() {
    removeExtraAddresses();
}
