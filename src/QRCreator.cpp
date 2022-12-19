/*
 * This file is part of the Flowee project
 * Copyright (C) 2018-2022 Tom Zander <tom@flowee.org>
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
// cmake requires the presence of the lib.
#include <qrencode.h>

QRCreator::QRCreator()
    : QQuickImageProvider(QQmlImageProviderBase::Image)
{
}

QImage QRCreator::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    Q_UNUSED(size);
    Q_UNUSED(requestedSize);
    QUrl url(id); // go via URL to encode spaces and special chars
    QRcode *code = QRcode_encodeString(url.toEncoded().constData(), 0, QR_ECLEVEL_L, QR_MODE_8, 1);
    if (code == nullptr) { // failed to encode.
        QImage blank = QImage(37, 37, QImage::Format_RGB32);
        blank.fill(0x232629); // gray
        return blank;
    }
    QImage result = QImage(code->width + 8, code->width + 8, QImage::Format_RGB32);
    result.fill(0xffffff);
    unsigned char *p = code->data;
    for (int y = 0; y < code->width; y++) {
        for (int x = 0; x < code->width; x++) {
            result.setPixel(x + 4, y + 4, ((*p & 1) ? 0x0 : 0xffffff));
            ++p;
        }
    }
    QRcode_free(code);
    return result;
}
