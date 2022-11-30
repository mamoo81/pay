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
#include "Camera.h"

#include <QCamera>
#include <QGuiApplication>
#ifdef TARGET_OS_Android
#include <private/qandroidextras_p.h>
#endif

#include <Logger.h>

Camera::Camera(QObject *parent)
    : QObject(parent),
      m_state(NotAsked)
{
    auto guiApp = qobject_cast<QGuiApplication*>(QCoreApplication::instance());
    assert(guiApp);
    connect(guiApp, &QGuiApplication::applicationStateChanged, this, [=](Qt::ApplicationState state) {
        if (state == Qt::ApplicationInactive) {
            // when the user leaves the app screen, the permissions granted to us
            // may have changed, so we need to re-ask.
            m_state = NotAsked;
        }
    });
}

bool Camera::authorized() const
{
    return m_state == Authorized;
}

bool Camera::denied() const
{
    return m_state == Denied;
}

void Camera::activate()
{
    if (m_state == NotAsked) {
#ifdef TARGET_OS_Android
#if QT_VERSION < QT_VERSION_CHECK(6, 5, 0)
        // we expect that in Qt65 this API will be changed or removed as the QCoreApplication::requestPermission will then become available.
        // for now, use the private APIs (since 6.5 is still 4 months from release)

        m_state = Asking;
        logCritical() << "Starting to ask for permission for camera usage";
        auto future = QtAndroidPrivate::checkPermission(QtAndroidPrivate::Camera);
        future.then([=](QtAndroidPrivate::PermissionResult res) {
            logCritical() << "Check permission returned: " << res;
            if (res == QtAndroidPrivate::PermissionResult::Authorized) {
                m_state = Authorized;
                emit authorizationChanged();
            }
            else {
                logCritical() << "We are starting to request permissions";
                // then ask
                auto future = QtAndroidPrivate::requestPermission(QtAndroidPrivate::Camera);
                future.then([=](QtAndroidPrivate::PermissionResult res) {
                    logCritical() << "Permission request result: " << res << (res == QtAndroidPrivate::Authorized);
                    if (res == QtAndroidPrivate::Authorized)
                        m_state = Authorized;
                    else
                        m_state = Denied;
                    emit authorizationChanged();
                });
            }
        });
#endif // version check
#else
        m_state = Authorized;
        emit authorizationChanged();
#endif
    }
}

void Camera::setQmlCamera(QObject *object)
{
    if (object == m_qmlCamera)
        return;
    m_qmlCamera = object;
    emit qmlCameraChanged();
    fetchCameraDetails();
}

QObject *Camera::qmlCamera() const
{
    return m_qmlCamera;
}

void Camera::fetchCameraDetails()
{
    QCamera *camera = qobject_cast<QCamera *>(m_qmlCamera);
    if (!camera)
        return;
    QCameraFormat preferred;
    for (const auto &format : camera->cameraDevice().videoFormats()) {
        if (preferred.isNull()) {
            preferred = format;
        }
        else {
            auto size = format.resolution();
            auto oldSize = preferred.resolution();
            // avoid going for the biggest feed, but not too small either.
            if (oldSize.width() < 190 || (size.width() < oldSize.width() && size.width() >= 190)) {
                preferred = format;
            }
            else if (size == oldSize && format.maxFrameRate() < preferred.maxFrameRate()) {
                preferred = format;
            }
        }
    }
    logInfo().nospace() << "Changing camera resolution to " << preferred.resolution().width() << "x" << preferred.resolution().height();
    camera->setCameraFormat(preferred);
    camera->setFocusMode(QCamera::FocusModeAutoNear); // macro focus mode.
    camera->setWhiteBalanceMode(QCamera::WhiteBalanceShade); // avoid flash

    camera->start();
}
