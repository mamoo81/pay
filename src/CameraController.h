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
#ifndef CAMERACONTROLLER_H
#define CAMERACONTROLLER_H

#include <QObject>
#include <QString>
#include <atomic>

class CameraControllerPrivate;
class QRScanner;

/**
 * This class works together with the QRScannerOverlay QML file.
 *
 * a. user action causes a QRScanner QML object to be populated and activated,
      which is backed by a cpp class of the same name.
 * b. we get a 'startRequest' call, we set loadCamera to true.
 * c. we check for authorization, if granted we set visible to true.
 * d. the QML populates our qmlCamera and videoSink properties
 * e. we check the camera setup, setting the best resolution and stuff.
 * f. we 'start' the camera by setting the cameraActive bool to true, which the QML uses.
 * g. when we finish scanning the QR we turn the camera off in the same manner.
 *    we also set visible to false.
 */
class CameraController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool visible READ visible NOTIFY visibleChanged)
    Q_PROPERTY(bool loadCamera READ loadCamera NOTIFY loadCameraChanged)
    Q_PROPERTY(bool cameraActive READ cameraActive NOTIFY cameraActiveChanged)
    Q_PROPERTY(QObject* camera READ camera WRITE setCamera NOTIFY cameraChanged)
    Q_PROPERTY(QObject* videoSink READ videoSink WRITE setVideoSink NOTIFY videoSinkChanged)
public:
    explicit CameraController(QObject *parent = nullptr);

    void startRequest(QRScanner *request);
    void abortRequest(QRScanner *request);

    Q_INVOKABLE void abort();

    void setCamera(QObject *object);
    QObject *camera() const;

    void setVideoSink(QObject *object);
    QObject *videoSink() const;

    bool loadCamera() const;
    bool cameraActive() const;
    bool visible() const;

signals:
    void cameraChanged();
    void videoSinkChanged();
    void textChanged();
    void loadCameraChanged();
    void cameraActiveChanged();
    void visibleChanged();

    // \internal (used to move thread)
    void startCheckState();

private slots:
    void initialize();
    void qrScanFinished();
    void checkState();

private:
    CameraControllerPrivate * d;
};

#endif
