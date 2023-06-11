/*
 * This file is part of the Flowee project
 * Copyright (C) 2022-2023 Tom Zander <tom@flowee.org>
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

#include "QRScanner.h"
#include "FloweePay.h"
#include "CameraController.h"

#include <QTimer>

QRScanner::QRScanner(QObject *parent)
    : QObject(parent),
      m_scanType(static_cast<ScanType>(100))
{
    // after construction and after QML setting all the properties, run the 'completed' slot.
    QTimer::singleShot(1, this, SLOT(completed()));
}

QRScanner::~QRScanner()
{
    abort();
}

void QRScanner::start()
{
    resetScanResult();
    if (m_scanType == static_cast<ScanType>(100))
        throw std::runtime_error("Required property scanType not set");
    setIsScanning(true);
    FloweePay::instance()->cameraController()->startRequest(this);
}

void QRScanner::abort()
{
    FloweePay::instance()->cameraController()->abortRequest(this);
    setIsScanning(false);
}

QRScanner::ScanType QRScanner::scanType() const
{
    return m_scanType;
}

void QRScanner::setScanType(ScanType type)
{
    if (m_scanType == type)
        return;
    m_scanType = type;
    emit scanTypeChanged();
}

void QRScanner::finishedScan(const QString &resultString)
{
    setScanResult(resultString);
    emit finished();
    setIsScanning(false);
}

QString QRScanner::scanResult() const
{
    return m_scanResult;
}

void QRScanner::setScanResult(const QString &result)
{
    if (m_scanResult == result || result.isEmpty())
        return;
    m_scanResult = result;
    emit scanResultChanged();
    setIsScanning(false);
}

void QRScanner::resetScanResult()
{
    if (!m_scanResult.isEmpty()) {
        m_scanResult.clear();
        emit scanResultChanged();
    }
}

bool QRScanner::autostart() const
{
    return m_autostart;
}

void QRScanner::setAutostart(bool newAutostart)
{
    if (m_autostart == newAutostart)
        return;
    m_autostart = newAutostart;
    emit autostartChanged();
}

bool QRScanner::isScanning() const
{
    return m_isScanning;
}

void QRScanner::setIsScanning(bool now)
{
    if (m_isScanning == now)
        return;
    m_isScanning = now;
    emit isScanningChanged();
}

void QRScanner::completed()
{
    if (m_autostart)
        start();
}
