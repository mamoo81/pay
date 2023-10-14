/*
 * This file is part of the Flowee project
 * Copyright (C) 2023 Tom Zander <tom@flowee.org>
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
#ifndef QMLCLIPBOARDHELPER_H
#define QMLCLIPBOARDHELPER_H

#include <QObject>

/**
 * This is a QML component aimed to make usage of clipboard for UX purposes
 * easier.
 *
 * This component reads the clipboard, it is not meant to write to the clipboard.
 * Please see FloweePay::copyToClipboard(const QString &text); for that.
 */
class QMLClipboardHelper : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString text READ clipboardText NOTIFY textChanged)
    Q_PROPERTY(Types filter READ filter WRITE setFilter NOTIFY filterChanged)
public:
    explicit QMLClipboardHelper(QObject *parent = nullptr);

    enum Type {
        NoFilter = 0,
        Addresses = 1 ,
        LegacyAddresses = 2,
        AddressUrl = 4,
        Mnemonic = 8
    };
    Q_ENUM(Type)
    Q_DECLARE_FLAGS(Types, Type)

    QString clipboardText() const;

    void setFilter(Types filters);
    Types filter() const;

signals:
    void textChanged();
    void filterChanged();

private:
    void parseClipboard();
    void setClipboardText(const QString &text);
    void delayedParse();

    Types m_filter;
    QString m_text;
    bool m_delayedParseStarted = false;
};

#endif
