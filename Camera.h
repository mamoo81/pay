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
#ifndef CAMERA_H
#define CAMERA_H

#include <QObject>
#include <QString>
#include <atomic>

#include <ZXing/DecodeHints.h>

class CheckState;

class Camera : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool showCamera READ showCamera NOTIFY showCameraChanged)
    Q_PROPERTY(bool authorized READ authorized NOTIFY authorizationChanged)
    Q_PROPERTY(bool denied READ denied NOTIFY authorizationChanged)
	Q_PROPERTY(QString text READ text NOTIFY textChanged)
    Q_PROPERTY(QObject* qmlCamera READ qmlCamera WRITE setQmlCamera NOTIFY qmlCameraChanged)
	Q_PROPERTY(QObject* videoSink READ videoSink WRITE setVideoSink NOTIFY videoSinkChanged)
public:
    explicit Camera(QObject *parent = nullptr);

    /// return true if the camera is authorized for usage.
    bool authorized() const;
    /// return true if the camera usage has been denied after being requested.
    bool denied() const;

    Q_INVOKABLE void activate();

    void setQmlCamera(QObject *object);
    QObject *qmlCamera() const;
    
    void setVideoSink(QObject *object);
    QObject *videoSink() const;
    
    QString text() const;
    void setText(const QString &text);
    
    bool showCamera() const;
    void setShowCamera(bool on);
    
signals:
    void authorizationChanged();
    void qmlCameraChanged();
    void videoSinkChanged();
    void textChanged();
    void showCameraChanged();

private:
    void fetchCameraDetails();

    // std::atomic<CheckState*> m_checkState;
    enum AskingState {
        NotAsked,
        Asking,
        Denied,
        Authorized
    };
    AskingState m_state;
    QObject *m_qmlCamera = nullptr;
    QObject *m_videoSink = nullptr;
    
    QString m_text;
    bool m_showCamera = false; // this is us telling QML to show the camera.
    
    ZXing::DecodeHints m_decodeHints;
};

#endif
