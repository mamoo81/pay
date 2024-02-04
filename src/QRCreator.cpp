/*
 * This file is part of the Flowee project
 * Copyright (C) 2018-2024 Tom Zander <tom@flowee.org>
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

#include <codecvt>
#include <locale>

QRCreator::QRCreator(QRType type)
    : QQuickImageProvider(QQmlImageProviderBase::Image),
    m_type(type)
{
}

QImage QRCreator::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    Q_UNUSED(size);
    Q_UNUSED(requestedSize);
    std::string data; // assumed utf8
    if (m_type == URLEncoded) {
        QUrl url(id); // go via URL to encode spaces and special chars
        data = url.toEncoded();
    } else if (m_type == RawString) {
        data = id.toUtf8();
    }

    auto writer = ZXing::MultiFormatWriter(ZXing::BarcodeFormat::QRCode).setMargin(16);
    /*
     * In newer versions of zxing there is a direct std::string version of the encode()
     * call which is nice to avoid the extra coversion.
     * For now we leave this extra code here as long as we are still able to compile and
     * run on Ubuntu 2022.04 (jammy) which doesn't have this new call.
     *
     * Ironically, the codecvt below code is deprecated in C++17, so you get warnings now.
     * Can't win this one, I guess...
     * But the promise is that it will be part of C++ till the 2026 release,
     * and I prefer it actually compiling on older zxing.  So maybe lets just
     * plan to remove the wstring conversion in a year or so (TZ: Feb 2024)
     */
    std::wstring wdata = std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(data);
    ZXing::BitMatrix matrix = writer.encode(wdata, 250, 250);

    QImage result = QImage(matrix.height(), matrix.width(), QImage::Format_RGB32);
    constexpr uint black = 0xFF000000;
    constexpr uint white = 0xFFFFFFFF;
    for (int y = 0; y < matrix.height(); ++y)
        for (int x = 0; x < matrix.width(); ++x)
            result.setPixel(x, y, matrix.get(x, y) ? black : white);

    return result;
}
