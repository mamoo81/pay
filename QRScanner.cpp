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

#include "QRScanner.h"
#include "FloweePay.h"
#include "CameraController.h"

QRScanner::QRScanner(QObject *parent)
    : QObject(parent),
      m_scanType(static_cast<ScanType>(100))
{
}

void QRScanner::start()
{
    resetScanResult();
    if (m_scanType == static_cast<ScanType>(100))
        throw std::runtime_error("Required property scanType not set");
    FloweePay::instance()->cameraController()->startRequest(this);
}

void QRScanner::abort()
{
    FloweePay::instance()->cameraController()->abortRequest(this);
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
}

QString QRScanner::scanResult() const
{
    return m_scanResult;
}

void QRScanner::setScanResult(const QString &newScanResult)
{
    if (m_scanResult == newScanResult)
        return;
    m_scanResult = newScanResult;
    emit scanResultChanged();
}

void QRScanner::resetScanResult()
{
    setScanResult({});
}
