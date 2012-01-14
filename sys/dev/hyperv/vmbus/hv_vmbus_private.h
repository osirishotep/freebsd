/*****************************************************************************
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The following copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Copyright (c) 2010-2011, Citrix, Inc.
 *
 * Ported from lis21 code drop
 *
 * HyperV vmbus private definition file
 *
 *****************************************************************************/

/*
 * Copyright (c) 2009, Microsoft Corporation - All rights reserved.
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Authors:
 *   Haiyang Zhang <haiyangz@microsoft.com>
 *   Hank Janssen  <hjanssen@microsoft.com>
 */

#ifndef __HV_VMBUS_PRIVATE_H__
#define __HV_VMBUS_PRIVATE_H__

#include <sys/param.h>
#include <sys/mbuf.h>
#include <sys/mutex.h>

#ifdef REMOVED
/* Fixme:  Removed */
#include "Hv.h"
#include "VmbusApi.h"
#include "Channel.h"
#include "ChannelMgmt.h"
#include "ChannelInterface.h"
//#include "ChannelMessages.h"
#include "RingBuffer.h"
//#include "Packet.h"
#include "List.h"
#include "timesync_ic.h"
#endif

//
// Defines
//

// Maximum channels is determined by the size of the interrupt page which is PAGE_SIZE. 1/2 of PAGE_SIZE is for
// send endpoint interrupt and the other is receive endpoint interrupt
#define MAX_NUM_CHANNELS				(PAGE_SIZE >> 1) << 3  // 16348 channels
// The value here must be in multiple of 32
// TODO: Need to make this configurable
#define MAX_NUM_CHANNELS_SUPPORTED		256

//
// Data types
//

typedef enum {
	Disconnected, Connecting, Connected, Disconnecting
} VMBUS_CONNECT_STATE;

#define MAX_SIZE_CHANNEL_MESSAGE			HV_MESSAGE_PAYLOAD_BYTE_COUNT

typedef struct _VMBUS_CONNECTION {

	VMBUS_CONNECT_STATE ConnectState;

	UINT32 NextGpadlHandle;

	// Represents channel interrupts. Each bit position
	// represents a channel.
	// When a channel sends an interrupt via VMBUS, it 
	// finds its bit in the sendInterruptPage, set it and 
	// calls Hv to generate a port event. The other end
	// receives the port event and parse the recvInterruptPage
	// to see which bit is set
	void* InterruptPage;
	void* SendInterruptPage;
	void* RecvInterruptPage;

	// 2 pages - 1st page for parent->child notification and 2nd is child->parent notification
	void* MonitorPages;
	LIST_ENTRY ChannelMsgList;
	struct mtx *ChannelMsgLock;

	// List of channels
	LIST_ENTRY ChannelList;
	struct mtx *ChannelLock;

	HANDLE WorkQueue;
} VMBUS_CONNECTION;

typedef struct _VMBUS_MSGINFO {
	// Bookkeeping stuff
	LIST_ENTRY MsgListEntry;

	// Synchronize the request/response if needed
	HANDLE WaitEvent;

	// The message itself
	unsigned char Msg[0];
} VMBUS_MSGINFO;
//
// Externs
//
extern VMBUS_CONNECTION gVmbusConnection;
//
// General vmbus interface
//
extern DEVICE_OBJECT*
VmbusChildDeviceCreate(GUID deviceType, GUID deviceInstance, void *context);

extern int
VmbusChildDeviceAdd(DEVICE_OBJECT* Device);

extern void
VmbusChildDeviceRemove(DEVICE_OBJECT* Device);
//extern void
//VmbusChildDeviceDestroy(
//	DEVICE_OBJECT*);
extern VMBUS_CHANNEL*
GetChannelFromRelId(UINT32 relId);

//
// Connection interface
//
extern int
VmbusConnect(void);

extern int
VmbusDisconnect(void);

extern int
VmbusPostMessage(void * buffer, SIZE_T bufSize);

extern int
VmbusSetEvent(UINT32 childRelId);

extern void
VmbusOnEvents(void);

#endif  /* __HV_VMBUS_PRIVATE_H__ */

