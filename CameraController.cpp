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
#include "CameraController.h"
#include "QRScanner.h"

#include <ZXing/ReadBarcode.h>

#include <QImage>
#include <QVideoFrame>
#include <QVideoSink>
#include <QCamera>
#include <QGuiApplication>
#include <QPointer>
#include <QMutex>
#include <QThread>
#ifdef TARGET_OS_Android
#include <private/qandroidextras_p.h>
#endif

#include <Logger.h>

enum AskingState {
    NotAsked,
    Asking,
    Denied,
    Authorized
};

#include <QTimer>
class QRScanningThread;

class CameraControllerPrivate
{
public:
    explicit CameraControllerPrivate(CameraController *qq);
    ~CameraControllerPrivate();
    // Configure the camera
    void initCamera();
    // check if we need to load the camera.
    void checkState();

    AskingState state;
    QObject *camera = nullptr;
    QObject *videoSink = nullptr;

    QPointer<QRScanner> scanRequest;

    mutable QMutex lock;
    QVideoFrame currentFrame;

    bool cameraLoaded = false;
    bool cameraStarted = false;
    bool visible = false;
    int streamWidth = -1;
    int streamHeight = -1;

    QRScanningThread *m_scanningThread = nullptr;

    CameraController *q;
};

class QRScanningThread : public QThread
{
public:
    explicit QRScanningThread(CameraControllerPrivate *parent);

    QString text;

protected:
    void run();
private:
    std::vector<ZXing::Result> readBarcodes(const QImage &img) const;
    std::vector<ZXing::Result> readBarcodes(const QVideoFrame &frame) const;

    CameraControllerPrivate *m_parent;
    ZXing::DecodeHints m_decodeHints;
    QRScanner::ScanType m_scanType;
};


// --------------------------------------------------------------------

CameraControllerPrivate::CameraControllerPrivate(CameraController *qq)
    : state(NotAsked),
      q(qq)
{
#ifdef TARGET_OS_Android
    auto guiApp = qobject_cast<QGuiApplication*>(QCoreApplication::instance());
    assert(guiApp);
    QObject::connect(guiApp, &QGuiApplication::applicationStateChanged, qq, [=](Qt::ApplicationState appState) {
        if (appState == Qt::ApplicationInactive) {
            // when the user leaves the app screen, the permissions granted to us
            // may have changed, so we need to re-ask.
            state = NotAsked;
            if (cameraStarted) {
                // QtMultimedia doesn't like us not turning off the camera when we get small.
                // so make sure we do, we also cancel the scan request.
                lock.lock();
                cameraStarted = false;
                lock.unlock();
                emit q->cameraActiveChanged();
            }
        }
    });
#endif
}

void CameraControllerPrivate::initCamera()
{
    QCamera *cam = qobject_cast<QCamera *>(camera);
    if (!cam)
        return;
    QCameraFormat preferred;
    bool preferredIsCheap = false;
    for (const auto &format : cam->cameraDevice().videoFormats()) {
        bool formatIsCheap; // if true, we don't need to go via QImage (we avoid double conversion)
        switch (format.pixelFormat()) {
        case QVideoFrameFormat::Format_Invalid:
        case QVideoFrameFormat::Format_XRGB8888:
        case QVideoFrameFormat::Format_XBGR8888:
        case QVideoFrameFormat::Format_RGBA8888:
        case QVideoFrameFormat::Format_SamplerExternalOES:
        case QVideoFrameFormat::Format_Jpeg:
        case QVideoFrameFormat::Format_SamplerRect:
        case QVideoFrameFormat::Format_YUV420P10:
            formatIsCheap = false;
            break;
        default:
            formatIsCheap = true;
            break;
        }

        logInfo() << " + " << format.resolution().width()  << "x" << format.resolution().height()
                   << "::" << format.pixelFormat() << formatIsCheap << "framerate:"
                   << format.minFrameRate() << "-" << format.maxFrameRate();

        if (preferred.isNull()) {
            preferred = format;
            preferredIsCheap = formatIsCheap;
            logInfo() << "picked";
        }
        else {
            auto size = format.resolution();
            auto oldSize = preferred.resolution();
            if (preferredIsCheap && !formatIsCheap)
                continue;
            // avoid going for the biggest feed, but not too small either.
            if (oldSize.width() < 210 || (size.width() < oldSize.width() && size.width() >= 210)) {
                preferred = format;
                logInfo() << "picked";
            }
            else if (size == oldSize && format.maxFrameRate() < preferred.maxFrameRate()) {
                preferred = format;
                logInfo() << "picked";
            }
        }
    }
    logCritical().nospace() << "Changing camera resolution to " << preferred.resolution().width() << "x" << preferred.resolution().height();
    cam->setCameraFormat(preferred);
    cam->setFocusMode(QCamera::FocusModeAutoNear); // macro focus mode.
    cam->setWhiteBalanceMode(QCamera::WhiteBalanceShade); // avoid flash
}

void CameraControllerPrivate::checkState()
{
    if (state != Authorized)
        return;
    if (!cameraLoaded) {
        cameraLoaded = true;
        emit q->loadCameraChanged();
    }
    if (!visible) {
        visible = true;
        emit q->visibleChanged();
    }
    if (camera && videoSink && !cameraStarted && scanRequest.get()) {
        logFatal() << "Camera active is now true";
        cameraStarted = true;
        emit q->cameraActiveChanged();
        QCamera *cam = qobject_cast<QCamera *>(camera);
        if (cam && cam->error() != QCamera::NoError)
            logFatal() << "CameraController found cam error:" << cam->errorString();

        auto sink = qobject_cast<QVideoSink*>(videoSink);
        assert(sink); // likely bug in your QML
        QObject::connect(sink, &QVideoSink::videoFrameChanged, q, [=](const QVideoFrame &frame) {
            currentFrame = frame;

            if (!m_scanningThread) {
                m_scanningThread = new QRScanningThread(this);
                QObject::connect (m_scanningThread, SIGNAL(finished()), q, SLOT(qrScanFinished()), Qt::QueuedConnection);
                m_scanningThread->start();
            }
        });
    }
}

// --------------------------------------------------------------------

QRScanningThread::QRScanningThread(CameraControllerPrivate *parent)
    : m_parent(parent),
      m_scanType(parent->scanRequest->scanType())
{
    m_decodeHints.setFormats(ZXing::BarcodeFormat::QRCode);
    m_decodeHints.setTryHarder(true);
}

void QRScanningThread::run()
{
    while (true) {
        m_parent->lock.lock();
        bool exit = !m_parent->cameraStarted;
        QVideoFrame frame = m_parent->currentFrame;
        m_parent->lock.unlock();
        if (exit)
            return;

        auto results = readBarcodes(frame);
        for (const auto &result : results) {
            const auto &bytes = result.bytes();
            logFatal() << "result:" << QString::fromUtf8(reinterpret_cast<const char*>(bytes.data()), bytes.size());

            switch (m_scanType) {
            case QRScanner::Seed: {
                // We can't be certain something is a seed here, we can just check the basics
                // only normal characters and spaces
                QString possibleSeed = QString::fromUtf8(reinterpret_cast<const char*>(bytes.data()), bytes.size());
                int wordCount = 0;
                bool seenSpace = false;
                bool failedChecks = false;
                for (auto i = 0; i < possibleSeed.size(); ++i) {
                    auto c = possibleSeed.at(i);
                    if (c.isDigit() || c.isSymbol())
                        failedChecks = true;
                    else if (c.isSpace()) {
                        if (seenSpace || i == 0)
                            failedChecks = true; // double space or leading space
                        seenSpace = true;
                        ++wordCount;
                    }
                    else if (c.isLetter()) {
                        seenSpace = false;
                    }
                    if (failedChecks)
                        break;
                }
                if (!failedChecks && (wordCount == 12 || wordCount == 24)) {
                    text = possibleSeed;
                    return; // makes the 'QThread::finished()' signal get emitted.
                }
                break;
            }
            case QRScanner::PaymentDetails:
                // starts with bitcoincash: (which is 12 chars, including that colon)
                if (bytes.size() > 12 + 40 && memcmp("bitcoincash:", bytes.data(), 12) == 0) {
                    text = QString::fromUtf8(reinterpret_cast<const char*>(bytes.data()), bytes.size());
                    return;
                }
                break;
            case QRScanner::PaymentDetailsTestnet:
                assert(false);  // TODO
                break;
            default:
                logFatal() << "Unknown scanType provided";
                return;
            }
        }

        // No matches. We go for another round.
        // we assume at least 33ms has passed (at 30 frames that is the frame-rate) and we'll be parsing the next frame on loop
    }
}

std::vector<ZXing::Result> QRScanningThread::readBarcodes(const QImage &img) const
{
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
        return ZXing::ReadBarcodes({gray.bits(), gray.width(), gray.height(), ZXing::ImageFormat::Lum, static_cast<int>(gray.bytesPerLine())}, m_decodeHints);
    }

    ZXing::ImageView buf(img.bits(), img.width(), img.height(), zxImageFormat, static_cast<int>(img.bytesPerLine()));
    return ZXing::ReadBarcodes(buf, m_decodeHints);
}

std::vector<ZXing::Result> QRScanningThread::readBarcodes(const QVideoFrame &frame) const
{
    ZXing::ImageFormat fmt = ZXing::ImageFormat::None;
    int pixStride = 0;
    int pixOffset = 0;

    // note that the comments behind the values are the Qt5 formats.
    switch (frame.pixelFormat()) {
    case QVideoFrameFormat::Format_ARGB8888: // ARGB32
    case QVideoFrameFormat::Format_ARGB8888_Premultiplied: // ARGB32_Premultiplied
    case QVideoFrameFormat::Format_RGBX8888: // RGB32
        fmt = ZXing::ImageFormat::BGRX;
        break;
    case QVideoFrameFormat::Format_BGRA8888: // BGRA32
    case QVideoFrameFormat::Format_BGRA8888_Premultiplied: // BGRA32_Premultiplied
    case QVideoFrameFormat::Format_BGRX8888: // BGR32
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
        fmt = ZXing::ImageFormat::RGBX;
#else
        fmt = ImageFormat::XBGR;
#endif
        break;
    case QVideoFrameFormat::Format_P010:
    case QVideoFrameFormat::Format_P016:
         fmt = ZXing::ImageFormat::Lum, pixStride = 1; break;
    case QVideoFrameFormat::Format_AYUV: // AYUV444
    case QVideoFrameFormat::Format_AYUV_Premultiplied: // AYUV444_Premultiplied
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
        fmt = ZXing::ImageFormat::Lum, pixStride = 4, pixOffset = 3;
#else
        fmt = ImageFormat::Lum, pixStride = 4, pixOffset = 2;
#endif
        break;
    case QVideoFrameFormat::Format_YUV420P: // YUV420P
    case QVideoFrameFormat::Format_NV12: // NV12
    case QVideoFrameFormat::Format_NV21: // NV21
    case QVideoFrameFormat::Format_IMC1: // IMC1
    case QVideoFrameFormat::Format_IMC2: // IMC2
    case QVideoFrameFormat::Format_IMC3: // IMC3
    case QVideoFrameFormat::Format_IMC4: // IMC4
    case QVideoFrameFormat::Format_YV12: // YV12
    case QVideoFrameFormat::Format_Y8: // Y8
    case QVideoFrameFormat::Format_YUV422P: // YUV422P
        fmt = ZXing::ImageFormat::Lum; break;
    case QVideoFrameFormat::Format_UYVY: // UYVY
        fmt = ZXing::ImageFormat::Lum, pixStride = 2, pixOffset = 1; break;
    case QVideoFrameFormat::Format_YUYV: // YUYV
        fmt = ZXing::ImageFormat::Lum, pixStride = 2; break;
    case QVideoFrameFormat::Format_Y16: // Y16
        fmt = ZXing::ImageFormat::Lum, pixStride = 2, pixOffset = 1; break;

    case QVideoFrameFormat::Format_ABGR8888: // ABGR32
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
        fmt = ZXing::ImageFormat::RGBX;
#else
        fmt = ZXing::ImageFormat::XBGR;
#endif
        break;
    default: break;
    }

    if (fmt != ZXing::ImageFormat::None) {
        auto img = frame; // shallow copy just get access to non-const map() function
        if (!img.isValid() || !img.map(QVideoFrame::ReadOnly)){
            logDebug() << "invalid QVideoFrame: could not map memory";
            return {};
        }
        QScopeGuard unmap([&] { img.unmap(); });

        constexpr int FirstPlane = 0;
        return ZXing::ReadBarcodes(
            {img.bits(FirstPlane) + pixOffset, img.width(), img.height(), fmt, img.bytesPerLine(FirstPlane), pixStride}, m_decodeHints);
    } else {
        return readBarcodes(frame.toImage());
    }
}


// --------------------------------------------------------------------

CameraController::CameraController(QObject *parent)
    : QObject(parent),
    d(new CameraControllerPrivate(this))
{
}

void CameraController::startRequest(QRScanner *request)
{
    assert(request);
    d->scanRequest = request;
    if (d->state == NotAsked) {
#ifdef TARGET_OS_Android
#if QT_VERSION < QT_VERSION_CHECK(6, 5, 0)
        // we expect that in Qt65 this API will be changed or removed as the QCoreApplication::requestPermission will then become available.
        // for now, use the private APIs (since 6.5 is still 4 months from release)

        d->state = Asking;
        logCritical() << "Starting to ask for permission for camera usage";
        auto future = QtAndroidPrivate::checkPermission(QtAndroidPrivate::Camera);
        future.then([=](QtAndroidPrivate::PermissionResult res) {
            logCritical() << "Check permission returned: " << res;
            if (res == QtAndroidPrivate::PermissionResult::Authorized) {
                d->state = Authorized;
                d->checkState();
            }
            else {
                logCritical() << "We are starting to request permissions";
                // then ask
                auto future = QtAndroidPrivate::requestPermission(QtAndroidPrivate::Camera);
                future.then([=](QtAndroidPrivate::PermissionResult res) {
                    logCritical() << "Permission request result: " << res << (res == QtAndroidPrivate::Authorized);
                    if (res == QtAndroidPrivate::Authorized) {
                        d->state = Authorized;
                        d->checkState();
                    } else {
                        d->state = Denied;
                    }
                });
            }
        });
#endif // version check
#else
        d->state = Authorized;
#endif
    }
    d->checkState();
}

void CameraController::abortRequest(QRScanner *request)
{
    if (d->scanRequest == request) {
        // The scanning thread will abort and nicely shutdown on change of this variable
        d->lock.lock();
        d->cameraStarted = false;
        d->lock.unlock();
        emit cameraActiveChanged();
    }
}

void CameraController::abort()
{
    abortRequest(d->scanRequest);
}

void CameraController::setCamera(QObject *object)
{
    if (object == d->camera)
        return;
    d->camera = object;
    emit cameraChanged();
    d->initCamera();
}

QObject *CameraController::camera() const
{
    return d->camera;
}

void CameraController::setVideoSink(QObject *object)
{
    if (d->videoSink == object)
        return;
    auto old = qobject_cast<QVideoSink*>(d->videoSink);
    if (old)
        QObject::disconnect(old, nullptr, this, nullptr);
    d->videoSink = object;
    emit videoSinkChanged();
}

QObject *CameraController::videoSink() const
{
    return d->videoSink;
}

bool CameraController::loadCamera() const
{
    return d->cameraLoaded;
}

bool CameraController::cameraActive() const
{
    return d->cameraStarted;
}

bool CameraController::visible() const
{
    return d->visible;
}

void CameraController::qrScanFinished()
{
    assert(d->m_scanningThread);
    logFatal() << " -> " << d->m_scanningThread->text;
    auto resultText = d->m_scanningThread->text;

    delete d->m_scanningThread;
    d->m_scanningThread = nullptr;

    d->cameraStarted = false;
    emit cameraActiveChanged();
    d->visible = false;
    emit visibleChanged();
    QObject::disconnect(d->videoSink, nullptr, this, nullptr);

    if (d->scanRequest) {
        d->scanRequest->finishedScan(resultText);
        d->scanRequest = nullptr;
    }
}
