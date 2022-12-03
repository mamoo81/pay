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
    // Q_PROPERTY(bool showCamera READ showCamera NOTIFY showCameraChanged)
public:
    explicit QRScanner(QObject *parent = nullptr);

    Q_INVOKABLE void start();

private:
};

#endif
