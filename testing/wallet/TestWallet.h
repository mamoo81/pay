#ifndef TEST_WALLET_H
#define TEST_WALLET_H

#include <QObject>

class TestWallet : public QObject
{
    Q_OBJECT
private slots:
    void transactionOrdering();
};

#endif
