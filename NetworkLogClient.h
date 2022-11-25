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
#ifndef NETWORKLOGCLIENT_H
#define NETWORKLOGCLIENT_H

#include <LogChannel.h>
#include <NetworkManager.h>

#include <boost/asio/io_service.hpp>

/**
 * This is a client for the Flowee debugging system which can log over the network.
 *
 * When doing development on Android we don't really have a good way to see what is going on
 * or run a second application showing the log-lines. So the simplest solution is to just use
 * what we already have in Flowee: a logging system with pluggable channels.
 */
class NetworkLogClient : public Log::Channel
{
public:
    NetworkLogClient(boost::asio::io_service& ioService);

    void pushLog(int64_t timeMillis, std::string *timestamp, const std::string &line, const char *filename, int lineNumber, const char *methodName, short logSection, short logLevel);

private:
    bool m_enabled = false;
    NetworkConnection m_con;
    std::string m_logPath;
};

#endif
