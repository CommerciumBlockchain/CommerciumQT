#include "rpc.h"

#include "addressbook.h"
#include "settings.h"
#include "senttxstore.h"
#include "turnstile.h"

using json = nlohmann::json;

RPC::RPC(MainWindow* main) {
    auto cl = new ConnectionLoader(main, this);

    // Execute the load connection async, so we can set up the rest of RPC properly. 
    QTimer::singleShot(1, [=]() { cl->loadConnection(); });

    this->main = main;
    this->ui = main->ui;

    this->turnstile = new Turnstile(this, main);

    // Setup balances table model
    balancesTableModel = new BalancesTableModel(main->ui->balancesTable);
    main->ui->balancesTable->setModel(balancesTableModel);

    // Setup transactions table model
    transactionsTableModel = new TxTableModel(ui->transactionsTable);
    main->ui->transactionsTable->setModel(transactionsTableModel);
    main->ui->transactionsTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);

    // Set up timer to refresh Price
    priceTimer = new QTimer(main);
    QObject::connect(priceTimer, &QTimer::timeout, [=]() {
        refreshCMMPrice();
    });
    priceTimer->start(Settings::priceRefreshSpeed);  // Every hour

    // Set up a timer to refresh the UI every few seconds
    timer = new QTimer(main);
    QObject::connect(timer, &QTimer::timeout, [=]() {
        refresh();
    });
    timer->start(Settings::updateSpeed);    

    // Set up the timer to watch for tx status
    txTimer = new QTimer(main);
    QObject::connect(txTimer, &QTimer::timeout, [=]() {
        watchTxStatus();
    });
    // Start at every 10s. When an operation is pending, this will change to every second
    txTimer->start(Settings::updateSpeed);  

    usedAddresses = new QMap<QString, bool>();
}

RPC::~RPC() {
    delete timer;
    delete txTimer;

    delete transactionsTableModel;
    delete balancesTableModel;
    delete turnstile;

    delete utxos;
    delete allBalances;
    delete usedAddresses;
    delete zaddresses;

    delete conn;
}

void RPC::setECommerciumd(QProcess* p) {
    ecommerciumd = p;

    if (ecommerciumd && ui->tabWidget->widget(4) == nullptr) {
        ui->tabWidget->addTab(main->commerciumdtab, "commerciumd");
    }
}

void RPC::setConnection(Connection* c) {
    if (c == nullptr) return;

    delete conn;
    this->conn = c;

    ui->statusBar->showMessage("Ready!");

    refreshCMMPrice();

    // Force update, because this might be coming from a settings update
    // where we need to immediately refresh
    refresh(true);
}

void RPC::getZAddresses(const std::function<void(json)>& cb) {
    json payload = {
        {"jsonrpc", "1.0"},
        {"id", "someid"},
        {"method", "z_listaddresses"},
    };

    conn->doRPCWithDefaultErrorHandling(payload, cb);
}

void RPC::getTransparentUnspent(const std::function<void(json)>& cb) {
    json payload = {
        {"jsonrpc", "1.0"},
        {"id", "someid"},
        {"method", "listunspent"},
        {"params", {0}}             // Get UTXOs with 0 confirmations as well.
    };

    conn->doRPCWithDefaultErrorHandling(payload, cb);
}

void RPC::getZUnspent(const std::function<void(json)>& cb) {
    json payload = {
        {"jsonrpc", "1.0"},
        {"id", "someid"},
        {"method", "z_listunspent"},
        {"params", {0}}             // Get UTXOs with 0 confirmations as well.
    };

    conn->doRPCWithDefaultErrorHandling(payload, cb);
}

void RPC::newZaddr(bool sapling, const std::function<void(json)>& cb) {
    json payload = {
        {"jsonrpc", "1.0"},
        {"id", "someid"},
        {"method", "z_getnewaddress"},
        {"params", { sapling ? "sapling" : "sprout" }},
    };
    
    conn->doRPCWithDefaultErrorHandling(payload, cb);
}

void RPC::newTaddr(const std::function<void(json)>& cb) {
    json payload = {
        {"jsonrpc", "1.0"},
        {"id", "someid"},
        {"method", "getnewaddress"},
    };

    conn->doRPCWithDefaultErrorHandling(payload, cb);
}

void RPC::getZPrivKey(QString addr, const std::function<void(json)>& cb) {
    json payload = {
        {"jsonrpc", "1.0"},
        {"id", "someid"},
        {"method", "z_exportkey"},
        {"params", { addr.toStdString() }},
    };
    
    conn->doRPCWithDefaultErrorHandling(payload, cb);
}

void RPC::getTPrivKey(QString addr, const std::function<void(json)>& cb) {
    json payload = {
        {"jsonrpc", "1.0"},
        {"id", "someid"},
        {"method", "dumpprivkey"},
        {"params", { addr.toStdString() }},
    };
    
    conn->doRPCWithDefaultErrorHandling(payload, cb);
}

void RPC::importZPrivKey(QString addr, bool rescan, const std::function<void(json)>& cb) {
    json payload = {
        {"jsonrpc", "1.0"},
        {"id", "someid"},
        {"method", "z_importkey"},
        {"params", { addr.toStdString(), (rescan? "yes" : "no") }},
    };
    
    conn->doRPCWithDefaultErrorHandling(payload, cb);
}


void RPC::importTPrivKey(QString addr, bool rescan, const std::function<void(json)>& cb) {
    json payload = {
        {"jsonrpc", "1.0"},
        {"id", "someid"},
        {"method", "importprivkey"},
        {"params", { addr.toStdString(), (rescan? "yes" : "no") }},
    };
    
    conn->doRPCWithDefaultErrorHandling(payload, cb);
}


void RPC::getBalance(const std::function<void(json)>& cb) {
    json payload = {
        {"jsonrpc", "1.0"},
        {"id", "someid"},
        {"method", "z_gettotalbalance"},
        {"params", {0}}             // Get Unconfirmed balance as well.
    };

    conn->doRPCWithDefaultErrorHandling(payload, cb);
}

void RPC::getTransactions(const std::function<void(json)>& cb) {
    json payload = {
        {"jsonrpc", "1.0"},
        {"id", "someid"},
        {"method", "listtransactions"}
    };

    conn->doRPCWithDefaultErrorHandling(payload, cb);
}

void RPC::sendZTransaction(json params, const std::function<void(json)>& cb) {
    json payload = {
        {"jsonrpc", "1.0"},
        {"id", "someid"},
        {"method", "z_sendmany"},
        {"params", params}
    };

    conn->doRPCWithDefaultErrorHandling(payload, cb);
}

/**
 * Method to get all the private keys for both z and t addresses. It will make 2 batch calls,
 * combine the result, and call the callback with a single list containing both the t-addr and z-addr
 * private keys
 */ 
void RPC::getAllPrivKeys(const std::function<void(QList<QPair<QString, QString>>)> cb) {
    if (conn == nullptr) {
        // No connection, just return
        return;
    }

    // A special function that will call the callback when two lists have been added
    auto holder = new QPair<int, QList<QPair<QString, QString>>>();
    holder->first = 0;  // This is the number of times the callback has been called, initialized to 0
    auto fnCombineTwoLists = [=] (QList<QPair<QString, QString>> list) {
        // Increment the callback counter
        holder->first++;    

        // Add all
        std::copy(list.begin(), list.end(), std::back_inserter(holder->second));
        
        // And if the caller has been called twice, do the parent callback with the 
        // collected list
        if (holder->first == 2) {
            // Sort so z addresses are on top
            std::sort(holder->second.begin(), holder->second.end(), 
                        [=] (auto a, auto b) { return a.first > b.first; });

            cb(holder->second);
            delete holder;
        }            
    };

    // A utility fn to do the batch calling
    auto fnDoBatchGetPrivKeys = [=](json getAddressPayload, std::string privKeyDumpMethodName) {
        conn->doRPCWithDefaultErrorHandling(getAddressPayload, [=] (json resp) {
            QList<QString> addrs;
            for (auto addr : resp.get<json::array_t>()) {   
                addrs.push_back(QString::fromStdString(addr.get<json::string_t>()));
            }

            // Then, do a batch request to get all the private keys
            conn->doBatchRPC<QString>(
                addrs, 
                [=] (auto addr) {
                    json payload = {
                        {"jsonrpc", "1.0"},
                        {"id", "someid"},
                        {"method", privKeyDumpMethodName},
                        {"params", { addr.toStdString() }},
                    };
                    return payload;
                },
                [=] (QMap<QString, json>* privkeys) {
                    QList<QPair<QString, QString>> allTKeys;
                    for (QString addr: privkeys->keys()) {
                        allTKeys.push_back(
                            QPair<QString, QString>(
                                addr, 
                                QString::fromStdString(privkeys->value(addr).get<json::string_t>())));
                    }

                    fnCombineTwoLists(allTKeys);
                    delete privkeys;
                }
            );
        });
    };

    // First get all the t and z addresses.
    json payloadT = {
        {"jsonrpc", "1.0"},
        {"id", "someid"},
        {"method", "getaddressesbyaccount"},
        {"params", {""} }
    };

    json payloadZ = {
        {"jsonrpc", "1.0"},
        {"id", "someid"},
        {"method", "z_listaddresses"}
    };

    fnDoBatchGetPrivKeys(payloadT, "dumpprivkey");
    fnDoBatchGetPrivKeys(payloadZ, "z_exportkey");
}


// Build the RPC JSON Parameters for this tx
void RPC::fillTxJsonParams(json& params, Tx tx) {   
    Q_ASSERT(params.is_array());
    // Get all the addresses and amounts
    json allRecepients = json::array();

    // For each addr/amt/memo, construct the JSON and also build the confirm dialog box    
    for (int i=0; i < tx.toAddrs.size(); i++) {
        auto toAddr = tx.toAddrs[i];

        // Construct the JSON params
        json rec = json::object();
        rec["address"]      = toAddr.addr.toStdString();
        // Force it through string for rounding. Without this, decimal points beyond 8 places
        // will appear, causing an "invalid amount" error
        rec["amount"]       = Settings::getDecimalString(toAddr.amount).toStdString(); //.toDouble(); 
        if (toAddr.addr.startsWith("z") && !toAddr.encodedMemo.trimmed().isEmpty())
            rec["memo"]     = toAddr.encodedMemo.toStdString();

        allRecepients.push_back(rec);
    }

    // Add sender    
    params.push_back(tx.fromAddr.toStdString());
    params.push_back(allRecepients);

    // Add fees if custom fees are allowed.
    if (Settings::getInstance()->getAllowCustomFees()) {
        params.push_back(1); // minconf
        params.push_back(tx.fee);
    }
}


void RPC::noConnection() {    
    QIcon i = QApplication::style()->standardIcon(QStyle::SP_MessageBoxCritical);
    main->statusIcon->setPixmap(i.pixmap(16, 16));
    main->statusIcon->setToolTip("");
    main->statusLabel->setText(QObject::tr("No Connection"));
    main->statusLabel->setToolTip("");
    main->ui->statusBar->showMessage(QObject::tr("No Connection"), 1000);

    // Clear balances table.
    QMap<QString, double> emptyBalances;
    QList<UnspentOutput>  emptyOutputs;
    balancesTableModel->setNewData(&emptyBalances, &emptyOutputs);

    // Clear Transactions table.
    QList<TransactionItem> emptyTxs;
    transactionsTableModel->addTData(emptyTxs);
    transactionsTableModel->addZRecvData(emptyTxs);
    transactionsTableModel->addZSentData(emptyTxs);

    // Clear balances
    ui->balSheilded->setText("");
    ui->balTransparent->setText("");
    ui->balTotal->setText("");

    ui->balSheilded->setToolTip("");
    ui->balTransparent->setToolTip("");
    ui->balTotal->setToolTip("");

    // Clear send tab from address
    ui->inputsCombo->clear();
}

// Refresh received z txs by calling z_listreceivedbyaddress/gettransaction
void RPC::refreshReceivedZTrans(QList<QString> zaddrs) {
    if  (conn == nullptr) 
        return noConnection();

    // We'll only refresh the received Z txs if settings allows us.
    if (!Settings::getInstance()->getSaveZtxs()) {
        QList<TransactionItem> emptylist;
        transactionsTableModel->addZRecvData(emptylist);
        return;
    }
        
    // This method is complicated because z_listreceivedbyaddress only returns the txid, and 
    // we have to make a follow up call to gettransaction to get details of that transaction. 
    // Additionally, it has to be done in batches, because there are multiple z-Addresses, 
    // and each z-Addr can have multiple received txs. 

    // 1. For each z-Addr, get list of received txs    
    conn->doBatchRPC<QString>(zaddrs,
        [=] (QString zaddr) {
            json payload = {
                {"jsonrpc", "1.0"},
                {"id", "z_lrba"},
                {"method", "z_listreceivedbyaddress"},
                {"params", {zaddr.toStdString(), 0}}      // Accept 0 conf as well.
            };

            return payload;
        },          
        [=] (QMap<QString, json>* zaddrTxids) {
            // Process all txids, removing duplicates. This can happen if the same address
            // appears multiple times in a single tx's outputs.
            QSet<QString> txids;
            QMap<QString, QString> memos;
            for (auto it = zaddrTxids->constBegin(); it != zaddrTxids->constEnd(); it++) {
                auto zaddr = it.key();
                for (auto& i : it.value().get<json::array_t>()) {   
                    // Mark the address as used
                    usedAddresses->insert(zaddr, true);

                    // Filter out change txs
                    if (! i["change"].get<json::boolean_t>()) {
                        auto txid = QString::fromStdString(i["txid"].get<json::string_t>());
                        txids.insert(txid);    

                        // Check for Memos
                        QString memoBytes = QString::fromStdString(i["memo"].get<json::string_t>());
                        if (!memoBytes.startsWith("f600"))  {
                            QString memo(QByteArray::fromHex(
                                            QByteArray::fromStdString(i["memo"].get<json::string_t>())));
                            if (!memo.trimmed().isEmpty())
                                memos[zaddr + txid] = memo;
                        }
                    }
                }                        
            }

            // 2. For all txids, go and get the details of that txid.
            conn->doBatchRPC<QString>(txids.toList(),
                [=] (QString txid) {
                    json payload = {
                        {"jsonrpc", "1.0"},
                        {"id",  "gettx"},
                        {"method", "gettransaction"},
                        {"params", {txid.toStdString()}}
                    };

                    return payload;
                },
                [=] (QMap<QString, json>* txidDetails) {
                    QList<TransactionItem> txdata;

                    // Combine them both together. For every zAddr's txid, get the amount, fee, confirmations and time
                    for (auto it = zaddrTxids->constBegin(); it != zaddrTxids->constEnd(); it++) {                        
                        for (auto& i : it.value().get<json::array_t>()) {   
                            // Filter out change txs
                            if (i["change"].get<json::boolean_t>())
                                continue;
                            
                            auto zaddr = it.key();
                            auto txid  = QString::fromStdString(i["txid"].get<json::string_t>());

                            // Lookup txid in the map
                            auto txidInfo = txidDetails->value(txid);

                            qint64 timestamp;
                            if (txidInfo.find("time") != txidInfo.end()) {
                                timestamp = txidInfo["time"].get<json::number_unsigned_t>();
                            } else {
                                timestamp = txidInfo["blocktime"].get<json::number_unsigned_t>();
                            }
                            
                            auto amount        = i["amount"].get<json::number_float_t>();
                            auto confirmations = (unsigned long)txidInfo["confirmations"].get<json::number_unsigned_t>();                            

                            TransactionItem tx{ QString("receive"), timestamp, zaddr, txid, amount, 
                                                confirmations, "", memos.value(zaddr + txid, "") };
                            txdata.push_front(tx);
                        }
                    }

                    transactionsTableModel->addZRecvData(txdata);

                    // Cleanup both responses;
                    delete zaddrTxids;
                    delete txidDetails;
                }
            );
        }
    );
} 

/// This will refresh all the balance data from commerciumd
void RPC::refresh(bool force) {
    if  (conn == nullptr) 
        return noConnection();

    getInfoThenRefresh(force);
}


void RPC::getInfoThenRefresh(bool force) {
    if  (conn == nullptr) 
        return noConnection();

    json payload = {
        {"jsonrpc", "1.0"},
        {"id", "someid"},
        {"method", "getinfo"}
    };

    static bool prevCallSucceeded = false;
    conn->doRPC(payload, [=] (const json& reply) {   
        prevCallSucceeded = true;
        // Testnet?
        if (!reply["testnet"].is_null()) {
            Settings::getInstance()->setTestnet(reply["testnet"].get<json::boolean_t>());
        };

        // Connected, so display checkmark.
        QIcon i(":/icons/res/connected.gif");
        main->statusIcon->setPixmap(i.pixmap(16, 16));

        static int    lastBlock = 0;
        int curBlock  = reply["blocks"].get<json::number_integer_t>();

        if ( force || (curBlock != lastBlock) ) {
            // Something changed, so refresh everything.
            lastBlock = curBlock;

            refreshBalances();        
            refreshAddresses(); // This calls refreshZSentTransactions() and refreshReceivedZTrans()
            refreshTransactions();
        }

        int connections = reply["connections"].get<json::number_integer_t>();
        Settings::getInstance()->setPeers(connections);

        if (connections == 0) {
            // If there are no peers connected, then the internet is probably off or something else is wrong. 
            QIcon i = QApplication::style()->standardIcon(QStyle::SP_MessageBoxWarning);
            main->statusIcon->setPixmap(i.pixmap(16, 16));
        }

        // Get network sol/s
        if (ecommerciumd) {
            json payload = {
                {"jsonrpc", "1.0"},
                {"id", "someid"},
                {"method", "getnetworksolps"}
            };

            conn->doRPCIgnoreError(payload, [=](const json& reply) {
                qint64 solrate = reply.get<json::number_unsigned_t>();

                ui->numconnections->setText(QString::number(connections));
                ui->solrate->setText(QString::number(solrate) % " Sol/s");
            });
        } 

        // Call to see if the blockchain is syncing. 
        json payload = {
            {"jsonrpc", "1.0"},
            {"id", "someid"},
            {"method", "getblockchaininfo"}
        };

        conn->doRPCIgnoreError(payload, [=](const json& reply) {
            auto progress    = reply["verificationprogress"].get<double>();
            bool isSyncing   = progress < 0.9999; // 99.99%
            int  blockNumber = reply["blocks"].get<json::number_unsigned_t>();

            int estimatedheight = 0;
            if (reply.find("estimatedheight") != reply.end()) {
                estimatedheight = reply["estimatedheight"].get<json::number_unsigned_t>();
            }

            Settings::getInstance()->setSyncing(isSyncing);
            Settings::getInstance()->setBlockNumber(blockNumber);

            // Update commerciumd tab if it exists
            if (ecommerciumd) {
                if (isSyncing) {
                    QString txt = QString::number(blockNumber);
                    if (estimatedheight > 0) {
                        txt = txt % " / ~" % QString::number(estimatedheight);
                        // If estimated height is available, then use the download blocks 
                        // as the progress instead of verification progress.
                        progress = (double)blockNumber / (double)estimatedheight;
                    }
                    txt = txt %  " ( " % QString::number(progress * 100, 'f', 0) % "% )";
                    ui->blockheight->setText(txt);
                    ui->heightLabel->setText(QObject::tr("Downloading blocks"));
                } else {
                    ui->blockheight->setText(QString::number(blockNumber));
                    ui->heightLabel->setText(QObject::tr("Block height"));
                }
            }

            // Update the status bar
            QString statusText = QString() %
                (isSyncing ? QObject::tr("Syncing") : QObject::tr("Connected")) %
                " (" %
                (Settings::getInstance()->isTestnet() ? QObject::tr("testnet:") : "") %
                QString::number(blockNumber) %
                (isSyncing ? ("/" % QString::number(progress*100, 'f', 0) % "%") : QString()) %
                ")";
            main->statusLabel->setText(statusText);   

            auto cmmPrice = Settings::getUSDFormat(1);
            QString tooltip;
            if (connections > 0) {
                tooltip = QObject::tr("Connected to commerciumd");
            }
            else {
                tooltip = QObject::tr("commerciumd has no peer connections");
            }

            if (!cmmPrice.isEmpty()) {
                tooltip = "1 CMM = " % cmmPrice % "\n" % tooltip;
            }
            main->statusLabel->setToolTip(tooltip);
            main->statusIcon->setToolTip(tooltip);
        });

    }, [=](QNetworkReply* reply, const json&) {
        // commerciumd has probably disappeared.
        this->noConnection();

        // Prevent multiple dialog boxes, because these are called async
        static bool shown = false;
        if (!shown && prevCallSucceeded) { // show error only first time
            shown = true;
            QMessageBox::critical(main, QObject::tr("Connection Error"), QObject::tr("There was an error connecting to commerciumd. The error was") + ": \n\n"
                + reply->errorString(), QMessageBox::StandardButton::Ok);
            shown = false;
        }

        prevCallSucceeded = false;
    });
}

void RPC::refreshAddresses() {
    if  (conn == nullptr) 
        return noConnection();

    delete zaddresses;
    zaddresses = new QList<QString>();

    getZAddresses([=] (json reply) {
        for (auto& it : reply.get<json::array_t>()) {   
            auto addr = QString::fromStdString(it.get<json::string_t>());
            zaddresses->push_back(addr);
        }

        // Refresh the sent and received txs from all these z-addresses
        refreshSentZTrans();
        refreshReceivedZTrans(*zaddresses);
    });
}

// Function to create the data model and update the views, used below.
void RPC::updateUI(bool anyUnconfirmed) {
    // See if the turnstile migration has any steps that need to be done.
    turnstile->executeMigrationStep();
    
    ui->unconfirmedWarning->setVisible(anyUnconfirmed);

    // Update balances model data, which will update the table too
    balancesTableModel->setNewData(allBalances, utxos);

    // Add all the addresses into the inputs combo box
    auto lastFromAddr = ui->inputsCombo->currentText();

    ui->inputsCombo->clear();
    auto i = allBalances->constBegin();
    while (i != allBalances->constEnd()) {
         ui->inputsCombo->addItem(i.key(), i.value());
        if (i.key() == lastFromAddr) ui->inputsCombo->setCurrentText(i.key());

        ++i;
    }

    if (lastFromAddr.isEmpty()) {
        main->setDefaultPayFrom();
    }
};

// Function to process reply of the listunspent and z_listunspent API calls, used below.
bool RPC::processUnspent(const json& reply) {
    bool anyUnconfirmed = false;
    for (auto& it : reply.get<json::array_t>()) {
        QString qsAddr = QString::fromStdString(it["address"]);
        auto confirmations = it["confirmations"].get<json::number_unsigned_t>();
        if (confirmations == 0) {
            anyUnconfirmed = true;
        }

        utxos->push_back(
            UnspentOutput{ qsAddr, QString::fromStdString(it["txid"]),
                            Settings::getDecimalString(it["amount"].get<json::number_float_t>()),
                            (int)confirmations, it["spendable"].get<json::boolean_t>() });

        (*allBalances)[qsAddr] = (*allBalances)[qsAddr] + it["amount"].get<json::number_float_t>();
    }
    return anyUnconfirmed;
};

void RPC::refreshBalances() {    
    if  (conn == nullptr) 
        return noConnection();

    // 1. Get the Balances
    getBalance([=] (json reply) {    
        auto balT = QString::fromStdString(reply["transparent"]).toDouble();
        auto balZ = QString::fromStdString(reply["private"]).toDouble();
        auto tot  = QString::fromStdString(reply["total"]).toDouble();

        ui->balSheilded   ->setText(Settings::getCMMDisplayFormat(balZ));
        ui->balTransparent->setText(Settings::getCMMDisplayFormat(balT));
        ui->balTotal      ->setText(Settings::getCMMDisplayFormat(tot));

        ui->balSheilded   ->setToolTip(Settings::getUSDFormat(balZ));
        ui->balTransparent->setToolTip(Settings::getUSDFormat(balT));
        ui->balTotal      ->setToolTip(Settings::getUSDFormat(tot));
    });

    // 2. Get the UTXOs
    // First, create a new UTXO list, deleting the old one;
    delete utxos;
    utxos = new QList<UnspentOutput>();
    delete allBalances;
    allBalances = new QMap<QString, double>();

    // Call the Transparent and Z unspent APIs serially and then, once they're done, update the UI
    getTransparentUnspent([=] (json reply) {
        auto anyTUnconfirmed = processUnspent(reply);

        getZUnspent([=] (json reply) {
            auto anyZUnconfirmed = processUnspent(reply);

            updateUI(anyTUnconfirmed || anyZUnconfirmed);    
        });        
    });
}

void RPC::refreshTransactions() {    
    if  (conn == nullptr) 
        return noConnection();

    getTransactions([=] (json reply) {
        QList<TransactionItem> txdata;

        for (auto& it : reply.get<json::array_t>()) {  
            double fee = 0;
            if (!it["fee"].is_null()) {
                fee = it["fee"].get<json::number_float_t>();
            }

            QString address = (it["address"].is_null() ? "" : QString::fromStdString(it["address"]));

            TransactionItem tx{
                QString::fromStdString(it["category"]),
                (qint64)it["time"].get<json::number_unsigned_t>(),
                address,
                QString::fromStdString(it["txid"]),
                it["amount"].get<json::number_float_t>() + fee,
                (unsigned long)it["confirmations"].get<json::number_unsigned_t>(),
                "", "" };

            txdata.push_back(tx);
            if (!address.isEmpty())
                usedAddresses->insert(address, true);
        }

        // Update model data, which updates the table view
        transactionsTableModel->addTData(txdata);        
    });
}

// Read sent Z transactions from the file.
void RPC::refreshSentZTrans() {
    if  (conn == nullptr) 
        return noConnection();

    auto sentZTxs = SentTxStore::readSentTxFile();

    // If there are no sent z txs, then empty the table. 
    // This happens when you clear history.
    if (sentZTxs.isEmpty()) {
        transactionsTableModel->addZSentData(sentZTxs);
        return;
    }

    QList<QString> txids;

    for (auto sentTx: sentZTxs) {
        txids.push_back(sentTx.txid);
    }

    // Look up all the txids to get the confirmation count for them. 
    conn->doBatchRPC<QString>(txids,
        [=] (QString txid) {
            json payload = {
                {"jsonrpc", "1.0"},
                {"id", "senttxid"},
                {"method", "gettransaction"},
                {"params", {txid.toStdString()}} 
            };

            return payload;
        },          
        [=] (QMap<QString, json>* txidList) {
            auto newSentZTxs = sentZTxs;
            // Update the original sent list with the confirmation count
            // TODO: This whole thing is kinda inefficient. We should probably just update the file
            // with the confirmed block number, so we don't have to keep calling gettransaction for the
            // sent items.
            for (TransactionItem& sentTx: newSentZTxs) {
                auto j = txidList->value(sentTx.txid);
                if (j.is_null())
                    continue;
                auto error = j["confirmations"].is_null();
                if (!error)
                    sentTx.confirmations = j["confirmations"].get<json::number_unsigned_t>();
            }
            
            transactionsTableModel->addZSentData(newSentZTxs);
            delete txidList;
        }
     );
}

void RPC::addNewTxToWatch(Tx tx, const QString& newOpid) {    
    watchingOps.insert(newOpid, tx);

    watchTxStatus();
}

void RPC::watchTxStatus() {
    if  (conn == nullptr) 
        return noConnection();

    // Make an RPC to load pending operation statues
    json payload = {
        {"jsonrpc", "1.0"},
        {"id", "someid"},
        {"method", "z_getoperationstatus"},
    };

    conn->doRPCIgnoreError(payload, [=] (const json& reply) {
        // There's an array for each item in the status
        for (auto& it : reply.get<json::array_t>()) {  
            // If we were watching this Tx and its status became "success", then we'll show a status bar alert
            QString id = QString::fromStdString(it["id"]);
            if (watchingOps.contains(id)) {
                // And if it ended up successful
                QString status = QString::fromStdString(it["status"]);
                if (status == "success") {
                    auto txid = QString::fromStdString(it["result"]["txid"]);
                    
                    SentTxStore::addToSentTx(watchingOps.value(id), txid);

                    main->ui->statusBar->showMessage(Settings::txidStatusMessage + " " + txid);
                    main->loadingLabel->setVisible(false);

                    watchingOps.remove(id);

                    // Refresh balances to show unconfirmed balances                    
                    refresh(true);  
                } else if (status == "failed") {
                    // If it failed, then we'll actually show a warning. 
                    auto errorMsg = QString::fromStdString(it["error"]["message"]);
                    QMessageBox msg(
                        QMessageBox::Critical,
                        QObject::tr("Transaction Error"), 
                        QObject::tr("The transaction with id ") % id % QObject::tr(" failed. The error was") + ":\n\n" + errorMsg,
                        QMessageBox::Ok,
                        main
                    );
                    
                    watchingOps.remove(id);
                    
                    main->ui->statusBar->showMessage(QObject::tr(" Tx ") % id % QObject::tr(" failed"), 15 * 1000);
                    main->loadingLabel->setVisible(false);

                    msg.exec();                                                  
                } 
            }

            if (watchingOps.isEmpty()) {
                txTimer->start(Settings::updateSpeed);
            } else {
                txTimer->start(Settings::quickUpdateSpeed);
            }
        }

        // If there is some op that we are watching, then show the loading bar, otherwise hide it
        if (watchingOps.empty()) {
            main->loadingLabel->setVisible(false);
        } else {
            main->loadingLabel->setVisible(true);
            main->loadingLabel->setToolTip(QString::number(watchingOps.size()) + QObject::tr(" tx computing. This can take several minutes."));
        }
    });
}

// Get the CMM->USD price from coinmarketcap using their API
void RPC::refreshCMMPrice() {
    if  (conn == nullptr) 
        return noConnection();

    QUrl cmcURL("https://api.coinmarketcap.com/v1/ticker/");

    QNetworkRequest req;
    req.setUrl(cmcURL);
    
    QNetworkReply *reply = conn->restclient->get(req);

    QObject::connect(reply, &QNetworkReply::finished, [=] {
        reply->deleteLater();

        try {
            if (reply->error() != QNetworkReply::NoError) {
                auto parsed = json::parse(reply->readAll(), nullptr, false);
                if (!parsed.is_discarded() && !parsed["error"]["message"].is_null()) {
                    qDebug() << QString::fromStdString(parsed["error"]["message"]);    
                } else {
                    qDebug() << reply->errorString();
                }
                Settings::getInstance()->setCMMPrice(0);
                return;
            } 

            auto all = reply->readAll();
            
            auto parsed = json::parse(all, nullptr, false);
            if (parsed.is_discarded()) {
                Settings::getInstance()->setCMMPrice(0);
                return;
            }

            for (const json& item : parsed.get<json::array_t>()) {
                if (item["symbol"].get<json::string_t>() == "CMM") {
                    QString price = QString::fromStdString(item["price_usd"].get<json::string_t>());
                    qDebug() << "CMM Price=" << price;
                    Settings::getInstance()->setCMMPrice(price.toDouble());

                    return;
                }
            }
        } catch (...) {
            // If anything at all goes wrong, just set the price to 0 and move on.
            qDebug() << QString("Caught something nasty");
        }

        // If nothing, then set the price to 0;
        Settings::getInstance()->setCMMPrice(0);
    });
}

void RPC::shutdownCommerciumd() {
    // Shutdown embedded commerciumd if it was started
    if (ecommerciumd == nullptr || conn == nullptr) {
        // No commerciumd running internally, just return
        return;
    }

    json payload = {
        {"jsonrpc", "1.0"},
        {"id", "someid"},
        {"method", "stop"}
    };
    
    conn->doRPCWithDefaultErrorHandling(payload, [=](auto) {});
    conn->shutdown();

    QDialog d(main);
    Ui_ConnectionDialog connD;
    connD.setupUi(&d);
    connD.topIcon->setBasePixmap(QIcon(":/icons/res/icon.ico").pixmap(256, 256));
    connD.status->setText(QObject::tr("Please wait for cmm-qt-wallet to exit"));
    connD.statusDetail->setText(QObject::tr("Waiting for commerciumd to exit"));

    QTimer waiter(main);

    // We capture by reference all the local variables because of the d.exec() 
    // below, which blocks this function until we exit. 
    int waitCount = 0;
    QObject::connect(&waiter, &QTimer::timeout, [&] () {
        waitCount++;

        if ((ecommerciumd->atEnd() && ecommerciumd->processId() == 0) ||
            waitCount > 30 || 
            conn->config->commerciumDaemon)  {   // If commerciumd is daemon, then we don't have to do anything else
            qDebug() << "Ended";
            waiter.stop();
            QTimer::singleShot(1000, [&]() { d.accept(); });
        } else {
            qDebug() << "Not ended, continuing to wait...";
        }
    });
    waiter.start(1000);

    // Wait for the commercium process to exit.     
    d.exec(); 
}


// Fetch the Z-board topics list
void RPC::getZboardTopics(std::function<void(QMap<QString, QString>)> cb) {
    if (conn == nullptr)
        return noConnection();

    QUrl cmcURL("http://z-board.net/listTopics");

    QNetworkRequest req;
    req.setUrl(cmcURL);

    QNetworkReply *reply = conn->restclient->get(req);

    QObject::connect(reply, &QNetworkReply::finished, [=] {
        reply->deleteLater();

        try {
            if (reply->error() != QNetworkReply::NoError) {
                auto parsed = json::parse(reply->readAll(), nullptr, false);
                if (!parsed.is_discarded() && !parsed["error"]["message"].is_null()) {
                    qDebug() << QString::fromStdString(parsed["error"]["message"]);
                }
                else {
                    qDebug() << reply->errorString();
                }
                return;
            }

            auto all = reply->readAll();

            auto parsed = json::parse(all, nullptr, false);
            if (parsed.is_discarded()) {
                return;
            }

            QMap<QString, QString> topics;
            for (const json& item : parsed["topics"].get<json::array_t>()) {
                if (item.find("addr") == item.end() || item.find("topicName") == item.end())
                    return;

                QString addr  = QString::fromStdString(item["addr"].get<json::string_t>());
                QString topic = QString::fromStdString(item["topicName"].get<json::string_t>());
                
                topics.insert(topic, addr);
            }

            cb(topics);
        }
        catch (...) {
            // If anything at all goes wrong, just set the price to 0 and move on.
            qDebug() << QString("Caught something nasty");
        }
    });
}

/** 
 * Get a Sapling address from the user's wallet
 */ 
QString RPC::getDefaultSaplingAddress() {
    for (QString addr: *zaddresses) {
        if (Settings::getInstance()->isSaplingAddress(addr))
            return addr;
    }

    return QString();
}
