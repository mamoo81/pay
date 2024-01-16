/*
 * This file is part of the Flowee project
 * Copyright (C) 2021 Tom Zander <tom@flowee.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef NEWWALLETCONFIG_H
#define NEWWALLETCONFIG_H

#include <QObject>

class Wallet;

/**
 * This is a class that is given over to the QML side to finish
 * configuration of a new wallet.
 * Various setters and getters are available that can be used to config
 * a very newly created wallet.
 *
 * Please note that this object will only be valid until the end of the
 * handler which received it from the FloweePay object. Do NOT store
 * this object on any QML (or cpp) property!
 */
class NewWalletConfig : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString name READ name CONSTANT)
    Q_PROPERTY(bool forceSingleAddress READ forceSingleAddress WRITE setForceSingleAddress NOTIFY SIChanged)
    Q_PROPERTY(int accountId READ accountId CONSTANT FINAL)
public:
    NewWalletConfig() {}
    explicit NewWalletConfig(Wallet *wallet);
    ~NewWalletConfig() override;

    /// return the name of the wallet.
    QString name() const;
    /// A wallet with this set will never receive funds on anything but 1 address
    bool forceSingleAddress() const;
    void setForceSingleAddress(bool on);

    int accountId() const;

signals:
    void SIChanged();

private:
    Wallet *m_wallet = nullptr;
};

#endif
