/*
 * This file is part of the Flowee project
 * Copyright (C) 2022 Tom Zander <tom@flowee.org>
 * Copyright (C) 2020 Axel Waggershauser <awagger@gmail.com>
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

#include <ZXing/ReadBarcode.h> 

#include <QImage>
#include <QVideoFrame>
#include <QVideoSink>
#include <QCamera>
#include <QGuiApplication>
#ifdef TARGET_OS_Android
#include <private/qandroidextras_p.h>
#endif

#include <Logger.h>

namespace {

std::vector<ZXing::Result> ReadBarcodes(const QImage &img, const ZXing::DecodeHints& hints) {
    auto imageFormat = img.format();
    if (imageFormat == QImage::Format_Invalid) // likely a damaged frame in the feed
        return {};
    auto zxImageFormat = ZXing::ImageFormat::None;
    switch (imageFormat) {
    case QImage::Format_ARGB32:
    case QImage::Format_RGB32:
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
        zxImageFormat =  ZXing::ImageFormat::BGRX;
#else
        zxImageFormat =  ImageFormat::XRGB;
#endif
        break;
    case QImage::Format_RGB888: zxImageFormat = ZXing::ImageFormat::RGB; break;
    case QImage::Format_RGBX8888:
    case QImage::Format_RGBA8888: zxImageFormat = ZXing::ImageFormat::RGBX; break;
    case QImage::Format_Grayscale8: zxImageFormat = ZXing::ImageFormat::Lum; break;
    default: break;
    }
    
    if (zxImageFormat == ZXing::ImageFormat::None) {
        QImage gray = img.convertToFormat(QImage::Format_Grayscale8) ;
		return ZXing::ReadBarcodes({gray.bits(), gray.width(), gray.height(), ZXing::ImageFormat::Lum, static_cast<int>(gray.bytesPerLine())}, hints);
    }

    ZXing::ImageView buf(img.bits(), img.width(), img.height(), zxImageFormat, static_cast<int>(img.bytesPerLine()));
    return ZXing::ReadBarcodes(buf, hints);
}

std::vector<ZXing::Result> ReadBarcodes(const QVideoFrame &frame, const ZXing::DecodeHints &hints) {
	auto img = frame; // shallow copy just get access to non-const map() function
	if (!frame.isValid() || !img.map(QVideoFrame::ReadOnly)){
		logDebug() << "invalid QVideoFrame: could not map memory";
		return {};
	}
	auto unmap = qScopeGuard([&] { img.unmap(); });

    ZXing::ImageFormat fmt = ZXing::ImageFormat::None;
	int pixStride = 0;
	int pixOffset = 0;

#define FORMAT(F5, F6) QVideoFrameFormat::Format_##F6

	switch (img.pixelFormat()) {
	case FORMAT(ARGB32, ARGB8888):
	case FORMAT(ARGB32_Premultiplied, ARGB8888_Premultiplied):
	case FORMAT(RGB32, RGBX8888):
		fmt = ZXing::ImageFormat::BGRX;
		break;

	case FORMAT(BGRA32, BGRA8888):
	case FORMAT(BGRA32_Premultiplied, BGRA8888_Premultiplied):
	case FORMAT(BGR32, BGRX8888):
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
		fmt = ZXing::ImageFormat::RGBX;
#else
		fmt = ImageFormat::XBGR;
#endif
		break;

	case QVideoFrameFormat::Format_P010:
	case QVideoFrameFormat::Format_P016: fmt = ZXing::ImageFormat::Lum, pixStride = 1; break;

	case FORMAT(AYUV444, AYUV):
	case FORMAT(AYUV444_Premultiplied, AYUV_Premultiplied):
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
		fmt = ZXing::ImageFormat::Lum, pixStride = 4, pixOffset = 3;
#else
		fmt = ImageFormat::Lum, pixStride = 4, pixOffset = 2;
#endif
		break;

	case FORMAT(YUV420P, YUV420P):
	case FORMAT(NV12, NV12):
	case FORMAT(NV21, NV21):
	case FORMAT(IMC1, IMC1):
	case FORMAT(IMC2, IMC2):
	case FORMAT(IMC3, IMC3):
	case FORMAT(IMC4, IMC4):
	case FORMAT(YV12, YV12): fmt = ZXing::ImageFormat::Lum; break;
	case FORMAT(UYVY, UYVY): fmt = ZXing::ImageFormat::Lum, pixStride = 2, pixOffset = 1; break;
	case FORMAT(YUYV, YUYV): fmt = ZXing::ImageFormat::Lum, pixStride = 2; break;

	case FORMAT(Y8, Y8): fmt = ZXing::ImageFormat::Lum; break;
	case FORMAT(Y16, Y16): fmt = ZXing::ImageFormat::Lum, pixStride = 2, pixOffset = 1; break;

	case FORMAT(ABGR32, ABGR8888):
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
		fmt = ZXing::ImageFormat::RGBX;
#else
		fmt = ZXing::ImageFormat::XBGR;
#endif
		break;
	case FORMAT(YUV422P, YUV422P): fmt = ZXing::ImageFormat::Lum; break;
	default: break;
	}

    constexpr int FirstPlane = 0;
	if (fmt != ZXing::ImageFormat::None) {
		return ZXing::ReadBarcodes(
			{img.bits(FirstPlane) + pixOffset, img.width(), img.height(), fmt, img.bytesPerLine(FirstPlane), pixStride}, hints);
	} else {
		return ReadBarcodes(img.toImage(), hints);
	}
}

}


Camera::Camera(QObject *parent)
    : QObject(parent),
      m_state(NotAsked)
{
    m_decodeHints.setFormats(ZXing::BarcodeFormat::QRCode);
    m_decodeHints.setTryHarder(true);
    
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

void Camera::setVideoSink(QObject *object)
{
    if (m_videoSink == object)
        return;
    auto old = qobject_cast<QVideoSink*>(m_videoSink);
    if (old)
        disconnect(old, nullptr, this, nullptr);
    m_videoSink = object;
    emit videoSinkChanged();

    auto sink = qobject_cast<QVideoSink*>(m_videoSink);
    if (sink) {
        connect(sink, &QVideoSink::videoFrameChanged, this, [=](const QVideoFrame &image) {
            auto res = ReadBarcodes(image, m_decodeHints);
            if (res.empty())
                return;
            const auto &bytes = res.at(0).bytes();
            setText(QString::fromUtf8(reinterpret_cast<const char*>(bytes.data()), bytes.size()));
        });
    }
}

QObject *Camera::videoSink() const
{
    return m_videoSink;
}

QString Camera::text() const
{
    return m_text;
}

void Camera::setText(const QString &text)
{
    if (m_text == text)
        return;
    m_text = text;
    emit textChanged();
    logFatal() << "Scanned: " << text;
    
    // TODO check barcode for usefulness (encoding etc).
    
    QCamera *camera = qobject_cast<QCamera *>(m_qmlCamera);
    if (!camera)
        return;
    camera->stop();
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
