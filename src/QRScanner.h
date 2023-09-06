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
#ifndef QRSCANNER_H
#define QRSCANNER_H

#include <QObject>
#include <QString>
#include <atomic>

class QRScanner : public QObject
{
    Q_OBJECT
    Q_PROPERTY(ScanType scanType READ scanType WRITE setScanType NOTIFY scanTypeChanged REQUIRED)
    Q_PROPERTY(QString scanResult READ scanResult WRITE setScanResult RESET resetScanResult NOTIFY scanResultChanged)
    Q_PROPERTY(bool autostart READ autostart WRITE setAutostart NOTIFY autostartChanged)
    Q_PROPERTY(bool isScanning READ isScanning NOTIFY isScanningChanged)
    Q_PROPERTY(ResultSource resultSource READ resultSource NOTIFY scanResultChanged)
public:
    explicit QRScanner(QObject *parent = nullptr);

    Q_INVOKABLE void start();
    Q_INVOKABLE void abort();

    enum ScanType {
        Seed,
        PaymentDetails,
        PaymentDetailsTestnet
        // others like private key or cashId
    };
    Q_ENUM(ScanType)

    ScanType scanType() const;
    void setScanType(ScanType type);

    /// Notice that resultString may be empty if we didn't scan any valid QR
    void finishedScan(const QString &resultString);

    enum ResultSource {
        Camera,
        Clipboard
    };
    Q_ENUM(ResultSource)

    QString scanResult() const;
    void setScanResult(const QString &result, ResultSource source = Camera);
    void resetScanResult();

    bool autostart() const;
    void setAutostart(bool newAutostart);

    bool isScanning() const;
    void setIsScanning(bool now);

    ResultSource resultSource() const;

private slots:
    void completed();

signals:
    void finished();
    void scanTypeChanged();
    void scanResultChanged();
    void autostartChanged();
    void isScanningChanged();

private:
    ScanType m_scanType;
    ResultSource m_resultSource = Camera;
    QString m_scanResult;
    bool m_autostart = false;
    bool m_isScanning = false;
};

#endif
