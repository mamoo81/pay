/*
 * This file is part of the Flowee project
 * Copyright (C) 2018-2023 Tom Zander <tom@flowee.org>
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
#include "QRCreator.h"

#include <Logger.h>
// cmake ensures the presence of the ZXing lib.
#include <ZXing/BarcodeFormat.h>
#include <ZXing/MultiFormatWriter.h>
#include <ZXing/BitMatrix.h>

QRCreator::QRCreator(QRType type)
    : QQuickImageProvider(QQmlImageProviderBase::Image),
    m_type(type)
{
}

QImage QRCreator::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    Q_UNUSED(size);
    Q_UNUSED(requestedSize);
    std::string data; // assumed utf8 by zxing
    if (m_type == URLEncoded) {
        QUrl url(id); // go via URL to encode spaces and special chars
        data = url.toEncoded();
    } else if (m_type == RawString) {
        data = id.toUtf8();
    }

    auto writer = ZXing::MultiFormatWriter(ZXing::BarcodeFormat::QRCode).setMargin(16);
    ZXing::BitMatrix matrix = writer.encode(data, 250, 250);

    QImage result = QImage(matrix.height(), matrix.width(), QImage::Format_RGB32);
    constexpr uint black = 0xFF000000;
    constexpr uint white = 0xFFFFFFFF;
    for (int y = 0; y < matrix.height(); ++y)
        for (int x = 0; x < matrix.width(); ++x)
            result.setPixel(x, y, matrix.get(x, y) ? black : white);

    return result;
}
