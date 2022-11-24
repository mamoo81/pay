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

#include "NetworkLogClient.h"
#include "streaming/BufferPools.h"

#include <QSettings>
#include <QStandardPaths>

#include <streaming/MessageBuilder.h>

enum NetworkTags {
    Locator = 1, // a byte-array
    Content, // a byte-array
};

enum ContentPayloadTags {
    LogSection = 0,
    LogLevel,
    LogLine
};

NetworkLogClient::NetworkLogClient(boost::asio::io_service &ioService)
    : Log::Channel(NoTime),
      m_network(ioService)
{
    QString filename;
#if TARGET_OS_Android
    filename = QLatin1String("assets:/netlog.conf");
#else
    filename = QStandardPaths::locate(QStandardPaths::AppConfigLocation, "netlog.conf");
#endif
    QSettings settings(filename, QSettings::IniFormat);
    const QString server = settings.value("server").toString();
    const int port = settings.value("port", "21712").toInt();
    m_logPath = settings.value("logpath").toString().toStdString();
    if (!server.isEmpty() && port > 0 && port < 0xFFFF) {
        m_enabled = true;
        if (m_logPath.size() < 1 || m_logPath[0] != '/' || m_logPath[m_logPath.size() - 1] != '/')
            throw std::runtime_error("Incorrect or missing logpath config. Needs to start and end with a slash");
        m_con = m_network.connection({server.toStdString(), uint16_t(port)});
    }
}

void NetworkLogClient::pushLog(int64_t timeMillis, std::string *timestamp, const std::string &line, const char *filename, int lineNumber, const char *methodName, short logSection, short logLevel)
{
    /*
     * since this is primarily meant to be used on Android, we expect a release build to be used.
     * As a result things like filename, methodname and linenumber will simply not be present.
     * Additionally, in the constructor we stated we use "NoTime" which will cause the timestamp not to be built for us.
    */
    Q_UNUSED(timestamp);
    Q_UNUSED(filename);
    Q_UNUSED(methodName);

    if (!m_enabled)
        return;

    // first build the message content.
    Streaming::MessageBuilder payloadBuilder(Streaming::pool(line.size() + 20));
    payloadBuilder.add(LogSection, logSection);
    payloadBuilder.add(LogLevel, logLevel);
    payloadBuilder.add(LogLine, line);
    auto payload = payloadBuilder.message();

    // then build the locator
    auto &pool = Streaming::pool(m_logPath.size() + 20);
    pool.write(m_logPath);
    pool.write(std::to_string(timeMillis));
    auto locator = pool.commit();

    pool.reserve(payload.body().size() + locator.size() + 20);
    Streaming::MessageBuilder builder(pool);
    builder.add(Locator, locator);
    builder.add(Content, payload.body());
    try {
        m_con.send(builder.message(124, 0));
    } catch (const std::exception &e) {
        // most likely this is hit when the network layer ran out of buffers
        // and the connection is too slow or could never be established.
        m_enabled = false;
        logFatal() << "Failed to log due to" << e;
    }
}
