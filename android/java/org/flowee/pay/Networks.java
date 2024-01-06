/*
 * This file is part of the Flowee project
 * Copyright (C) 2024 Tom Zander <tom@flowee.org>
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
package org.flowee.pay;

import android.content.Context;
import java.net.*;
import java.util.Enumeration;
import java.util.List;

public class Networks
{
    /**
     * This method figures out if we seem to have support for certain networks.
     *
     * We return a flags object with 1 for IPv4 availability plus 2 for v6.
     */
    public static int networkSupport() {
        boolean ipv4Available = false;
        boolean ipv6Available = false;

        try {
            Enumeration<NetworkInterface> networking = NetworkInterface.getNetworkInterfaces();
            while (networking.hasMoreElements()) {
                NetworkInterface i = networking.nextElement();
                if (i.isLoopback())
                    continue;
                if (!i.isUp())
                    continue;
                List<InterfaceAddress> addresses = i.getInterfaceAddresses();
                for (InterfaceAddress address : addresses) {
                    InetAddress ip = address.getAddress();
                    try {
                        Inet6Address ip6 = (Inet6Address) ip;
                        if (!ip6.isLinkLocalAddress()) ipv6Available = true;
                    } catch (ClassCastException e) { }
                    try {
                        Inet4Address ip4 = (Inet4Address) ip;
                        if (!ip4.isLinkLocalAddress()) ipv4Available = true;
                    } catch (ClassCastException e) { }
                }
            }
        } catch (SocketException e) {
            // sane default if we don't have permission to check stuff..
            ipv4Available = true;
        }

        // simple bitfield.
        int answer = 0;
        if (ipv4Available)
            answer += 1;
        if (ipv6Available)
            answer += 2;
        return answer;
    }
}
