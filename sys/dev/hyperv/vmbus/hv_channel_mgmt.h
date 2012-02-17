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
 * Channel management definition file
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

#ifndef __HV_CHANNEL_MGMT_H__
#define __HV_CHANNEL_MGMT_H__

#include "hv_vmbus_var.h"
#include <dev/hyperv/include/hv_list.h>
#include <dev/hyperv/include/hv_channel_messages.h>
#include "hv_ring_buffer.h"

typedef void (*PFN_CHANNEL_CALLBACK)(void *context);

typedef enum {
	CHANNEL_OFFER_STATE,
	CHANNEL_OPENING_STATE,
	CHANNEL_OPEN_STATE,
} VMBUS_CHANNEL_STATE;

typedef struct _VMBUS_CHANNEL {
	LIST_ENTRY ListEntry;

	DEVICE_OBJECT* DeviceObject;

	HANDLE PollTimer; // SA-111 workaround

	VMBUS_CHANNEL_STATE State;

	VMBUS_CHANNEL_OFFER_CHANNEL OfferMsg;
	// These are based on the OfferMsg.MonitorId. Save it here for easy access.
	uint8_t MonitorGroup;
	uint8_t MonitorBit;

	uint32_t RingBufferGpadlHandle;

	// Allocated memory for ring buffer
	void *RingBufferPages;
	uint32_t RingBufferPageCount;
	RING_BUFFER_INFO Outbound;	// send to parent
	RING_BUFFER_INFO Inbound;	// receive from parent
	struct mtx *InboundLock;
	HANDLE ControlWQ;

	// Channel callback are invoked in this workqueue context
	//HANDLE				dataWorkQueue;

	PFN_CHANNEL_CALLBACK OnChannelCallback;
	void *ChannelCallbackContext;

} VMBUS_CHANNEL;

typedef struct _VMBUS_CHANNEL_DEBUG_INFO {
	uint32_t RelId;
	VMBUS_CHANNEL_STATE State;
	GUID InterfaceType;
	GUID InterfaceInstance;
	uint32_t MonitorId;
	uint32_t ServerMonitorPending;
	uint32_t ServerMonitorLatency;
	uint32_t ServerMonitorConnectionId;
	uint32_t ClientMonitorPending;
	uint32_t ClientMonitorLatency;
	uint32_t ClientMonitorConnectionId;

	RING_BUFFER_DEBUG_INFO Inbound;
	RING_BUFFER_DEBUG_INFO Outbound;
} VMBUS_CHANNEL_DEBUG_INFO;

typedef union {
	VMBUS_CHANNEL_VERSION_SUPPORTED VersionSupported;
	VMBUS_CHANNEL_OPEN_RESULT OpenResult;
	VMBUS_CHANNEL_GPADL_TORNDOWN GpadlTorndown;
	VMBUS_CHANNEL_GPADL_CREATED GpadlCreated;
	VMBUS_CHANNEL_VERSION_RESPONSE VersionResponse;
} VMBUS_CHANNEL_MESSAGE_RESPONSE;

// Represents each channel msg on the vmbus connection
// This is a variable-size data structure depending on
// the msg type itself
typedef struct _VMBUS_CHANNEL_MSGINFO {
	// Bookkeeping stuff
	LIST_ENTRY MsgListEntry;

	// So far, this is only used to handle gpadl body message
	LIST_ENTRY SubMsgList;

	// Synchronize the request/response if needed
	HANDLE WaitEvent;

	VMBUS_CHANNEL_MESSAGE_RESPONSE Response;

	uint32_t MessageSize;
	// The channel message that goes out on the "wire".
	// It will contain at minimum the VMBUS_CHANNEL_MESSAGE_HEADER header
	unsigned char Msg[0];
} VMBUS_CHANNEL_MSGINFO;

extern VMBUS_CHANNEL*
AllocVmbusChannel(void);

extern void
FreeVmbusChannel(VMBUS_CHANNEL *Channel);

extern void
VmbusOnChannelMessage(void *Context);

extern int
VmbusChannelRequestOffers(void);

extern void
VmbusChannelReleaseUnattachedChannels(void);

#endif  /* __HV_CHANNEL_MGMT_H__ */

