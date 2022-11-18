/*
 * This file is part of the Flowee project
 * Copyright (C) 2022 Tom Zander <tom@flowee.org>
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
#ifndef WALLETENUMS_H
#define WALLETENUMS_H

#include <QObject>

class WalletEnums : public QObject
{
    Q_OBJECT
public:
    WalletEnums(QObject *parent = nullptr);

    enum StringType {
        Unknown = 0,
        PrivateKey,
        CashPKH,
        CashSH,
        LegacyPKH,
        LegacySH,
        PartialMnemonic,
        PartialMnemonicWithTypo,
        CorrectMnemonic,
        MissingLexicon
    };

    enum Include {
        IncludeNothing = 0,
        IncludeRejected = 1,
        IncludeUnconfirmed = 2,
        IncludeConfirmed = 4,

        IncludeAll = IncludeRejected  | IncludeUnconfirmed | IncludeConfirmed
    };
    Q_DECLARE_FLAGS(Includes, Include)
    Q_FLAG(Includes)
    Q_ENUM(StringType UnitOfBitcoin)
};

#endif
