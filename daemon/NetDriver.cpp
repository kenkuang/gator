/**
 * Copyright (C) ARM Limited 2013-2016. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

// Define to get format macros from inttypes.h
#define __STDC_FORMAT_MACROS

#include "NetDriver.h"

#include <inttypes.h>
#include <unistd.h>

#include "Logging.h"
#include "SessionData.h"

class NetCounter : public DriverCounter
{
public:
    NetCounter(DriverCounter *next, char * const name, uint64_t * const value);
    ~NetCounter();

    int64_t read();

private:
    uint64_t * const mValue;
    uint64_t mPrev;

    // Intentionally unimplemented
    CLASS_DELETE_COPY_MOVE(NetCounter);
};

NetCounter::NetCounter(DriverCounter *next, char * const name, uint64_t * const value)
        : DriverCounter(next, name),
          mValue(value),
          mPrev(0)
{
}

NetCounter::~NetCounter()
{
}

int64_t NetCounter::read()
{
    int64_t result = *mValue - mPrev;
    mPrev = *mValue;
    return result;
}

NetDriver::NetDriver()
        : mBuf(),
          mReceiveBytes(0),
          mTransmitBytes(0)
{
}

NetDriver::~NetDriver()
{
}

void NetDriver::readEvents(mxml_node_t * const)
{
    if (access("/proc/net/dev", R_OK) == 0) {
        setCounters(new NetCounter(getCounters(), strdup("Linux_net_rx"), &mReceiveBytes));
        setCounters(new NetCounter(getCounters(), strdup("Linux_net_tx"), &mTransmitBytes));
    }
    else {
        logg.logSetup("Linux counters\nCannot access /proc/net/dev. Network transmit and receive counters not available.");
    }
}

bool NetDriver::doRead()
{
    if (!countersEnabled()) {
        return true;
    }

    if (!mBuf.read("/proc/net/dev")) {
        return false;
    }

    // Skip the header
    char *key;
    if (((key = strchr(mBuf.getBuf(), '\n')) == NULL) || ((key = strchr(key + 1, '\n')) == NULL)) {
        return false;
    }
    key = key + 1;

    mReceiveBytes = 0;
    mTransmitBytes = 0;

    char *colon;
    while ((colon = strchr(key, ':')) != NULL) {
        char *end = strchr(colon + 1, '\n');
        if (end != NULL) {
            *end = '\0';
        }
        *colon = '\0';

        uint64_t receiveBytes;
        uint64_t transmitBytes;
        const int count = sscanf(colon + 1, " %" SCNu64 " %*u %*u %*u %*u %*u %*u %*u %" SCNu64, &receiveBytes,
                                 &transmitBytes);
        if (count != 2) {
            return false;
        }
        mReceiveBytes += receiveBytes;
        mTransmitBytes += transmitBytes;

        if (end == NULL) {
            break;
        }
        key = end + 1;
    }

    return true;
}

void NetDriver::start()
{
    if (!doRead()) {
        logg.logError("Unable to read network stats");
        handleException();
    }
    // Initialize previous values
    for (DriverCounter *counter = getCounters(); counter != NULL; counter = counter->getNext()) {
        if (!counter->isEnabled()) {
            continue;
        }
        counter->read();
    }
}

void NetDriver::read(Buffer * const buffer)
{
    if (!doRead()) {
        logg.logError("Unable to read network stats");
        handleException();
    }
    super::read(buffer);
}
