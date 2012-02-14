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
 * Copyright (c) 2010-2012, Citrix, Inc.
 *
 * Ported from lis21 code drop
 *
 * HyperV Synic API header
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

#ifndef __HV_SYNIC_API_H__
#define __HV_SYNIC_API_H__


//
// Define the virtual APIC registers
//
#define HV_X64_MSR_EOI                  (0x40000070)
#define HV_X64_MSR_ICR                  (0x40000071)
#define HV_X64_MSR_TPR                  (0x40000072)
#define HV_X64_MSR_APIC_ASSIST_PAGE     (0x40000073)

//
// Define version of the synthetic interrupt controller.
//
#define HV_SYNIC_VERSION        (1)

//
// Define synthetic interrupt controller model specific registers.
//
#define HV_X64_MSR_SCONTROL   (0x40000080)
#define HV_X64_MSR_SVERSION   (0x40000081)
#define HV_X64_MSR_SIEFP      (0x40000082)
#define HV_X64_MSR_SIMP       (0x40000083)
#define HV_X64_MSR_EOM        (0x40000084)
#define HV_X64_MSR_SINT0      (0x40000090)
#define HV_X64_MSR_SINT1      (0x40000091)
#define HV_X64_MSR_SINT2      (0x40000092)
#define HV_X64_MSR_SINT3      (0x40000093)
#define HV_X64_MSR_SINT4      (0x40000094)
#define HV_X64_MSR_SINT5      (0x40000095)
#define HV_X64_MSR_SINT6      (0x40000096)
#define HV_X64_MSR_SINT7      (0x40000097)
#define HV_X64_MSR_SINT8      (0x40000098)
#define HV_X64_MSR_SINT9      (0x40000099)
#define HV_X64_MSR_SINT10     (0x4000009A)
#define HV_X64_MSR_SINT11     (0x4000009B)
#define HV_X64_MSR_SINT12     (0x4000009C)
#define HV_X64_MSR_SINT13     (0x4000009D)
#define HV_X64_MSR_SINT14     (0x4000009E)
#define HV_X64_MSR_SINT15     (0x4000009F)

//
// Define the expected SynIC version.
//
#define HV_SYNIC_VERSION_1 (0x1)

//
// Define synthetic interrupt controller message constants.
//
#define HV_MESSAGE_SIZE                 (256)
#define HV_MESSAGE_PAYLOAD_BYTE_COUNT   (240)
#define HV_MESSAGE_PAYLOAD_QWORD_COUNT  (30)
#define HV_ANY_VP                       (0xFFFFFFFF)

//
// Define synthetic interrupt controller flag constants.
//
#define HV_EVENT_FLAGS_COUNT        (256 * 8)
#define HV_EVENT_FLAGS_BYTE_COUNT   (256)
#define HV_EVENT_FLAGS_DWORD_COUNT  (256 / sizeof(uint32_t))

//
// Define hypervisor message types.
//
typedef enum _HV_MESSAGE_TYPE {
    HvMessageTypeNone = 0x00000000,

    //
    // Memory access messages.
    //
    HvMessageTypeUnmappedGpa = 0x80000000,
    HvMessageTypeGpaIntercept = 0x80000001,

    //
    // Timer notification messages.
    //
    HvMessageTimerExpired = 0x80000010,

    //
    // Error messages.
    //
    HvMessageTypeInvalidVpRegisterValue = 0x80000020,
    HvMessageTypeUnrecoverableException = 0x80000021,
    HvMessageTypeUnsupportedFeature = 0x80000022,

    //
    // Trace buffer complete messages.
    //
    HvMessageTypeEventLogBufferComplete = 0x80000040,

    //
    // Platform-specific processor intercept messages.
    //
    HvMessageTypeX64IoPortIntercept = 0x80010000,
    HvMessageTypeX64MsrIntercept = 0x80010001,
    HvMessageTypeX64CpuidIntercept = 0x80010002,
    HvMessageTypeX64ExceptionIntercept = 0x80010003,
    HvMessageTypeX64ApicEoi = 0x80010004,
    HvMessageTypeX64LegacyFpError = 0x80010005
} HV_MESSAGE_TYPE, *PHV_MESSAGE_TYPE;

//
// Define the number of synthetic interrupt sources.
//
#define HV_SYNIC_SINT_COUNT (16)
#define HV_SYNIC_STIMER_COUNT (4)

//
// Define the synthetic interrupt source index type.
//
typedef uint32_t HV_SYNIC_SINT_INDEX, *PHV_SYNIC_SINT_INDEX;

//
// Define partition identifier type.
//
typedef uint64_t HV_PARTITION_ID, *PHV_PARTITION_ID;

//
// Define invalid partition identifier.
//
#define HV_PARTITION_ID_INVALID ((HV_PARTITION_ID) 0x0)

//
// Define connection identifier type.
//
typedef union _HV_CONNECTION_ID {
    uint32_t Asuint32_t;

    struct {
        uint32_t Id:24;
        uint32_t Reserved:8;
    } u;

} HV_CONNECTION_ID, *PHV_CONNECTION_ID;

//
// Define port identifier type.
//
typedef union _HV_PORT_ID {
    uint32_t Asuint32_t;

    struct {
        uint32_t Id:24;
        uint32_t Reserved:8;
    } u ;

} HV_PORT_ID, *PHV_PORT_ID;

//
// Define port type.
//
typedef enum _HV_PORT_TYPE {
    HvPortTypeMessage   = 1,
    HvPortTypeEvent     = 2,
    HvPortTypeMonitor   = 3
} HV_PORT_TYPE, *PHV_PORT_TYPE;

//
// Define port information structure.
//
typedef struct _HV_PORT_INFO {
    HV_PORT_TYPE PortType;
    uint32_t Padding;

    union {
        struct {
            HV_SYNIC_SINT_INDEX TargetSint;
            HV_VP_INDEX TargetVp;
            uint64_t RsvdZ;
        } MessagePortInfo;

        struct {
            HV_SYNIC_SINT_INDEX TargetSint;
            HV_VP_INDEX TargetVp;
            uint16_t BaseFlagNumber;
            uint16_t FlagCount;
            uint32_t RsvdZ;
        } EventPortInfo;

        struct {
            HV_GPA MonitorAddress;
            uint64_t RsvdZ;
        } MonitorPortInfo;
    };
} HV_PORT_INFO, *PHV_PORT_INFO;

typedef const HV_PORT_INFO *PCHV_PORT_INFO;

typedef struct _HV_CONNECTION_INFO {
    HV_PORT_TYPE PortType;
    uint32_t Padding;

    union {
        struct {
            uint64_t RsvdZ;
        } MessageConnectionInfo;

        struct {
            uint64_t RsvdZ;
        } EventConnectionInfo;

        struct {
            HV_GPA MonitorAddress;
        } MonitorConnectionInfo;
    };
} HV_CONNECTION_INFO, *PHV_CONNECTION_INFO;

typedef const HV_CONNECTION_INFO *PCHV_CONNECTION_INFO;

//
// Define synthetic interrupt controller message flags.
//
typedef union _HV_MESSAGE_FLAGS {
    uint8_t Asuint8_t;
    struct {
        uint8_t MessagePending:1;
        uint8_t Reserved:7;
    };
} HV_MESSAGE_FLAGS, *PHV_MESSAGE_FLAGS;


//
// Define synthetic interrupt controller message header.
//
typedef struct _HV_MESSAGE_HEADER {
    HV_MESSAGE_TYPE     MessageType;
    uint8_t               PayloadSize;
    HV_MESSAGE_FLAGS    MessageFlags;
    uint8_t               Reserved[2];
    union {
        HV_PARTITION_ID Sender;
        HV_PORT_ID      Port;
    };
} HV_MESSAGE_HEADER, *PHV_MESSAGE_HEADER;

//
// Define timer message payload structure.
//
typedef struct _HV_TIMER_MESSAGE_PAYLOAD {
    uint32_t          TimerIndex;
    uint32_t          Reserved;
    HV_NANO100_TIME ExpirationTime;     // When the timer expired
    HV_NANO100_TIME DeliveryTime;       // When the message was delivered
} HV_TIMER_MESSAGE_PAYLOAD, *PHV_TIMER_MESSAGE_PAYLOAD;

//
// Define synthetic interrupt controller message format.
//

typedef struct _HV_MESSAGE {
    HV_MESSAGE_HEADER Header;
    union {
        uint64_t Payload[HV_MESSAGE_PAYLOAD_QWORD_COUNT];
    } u ;
} HV_MESSAGE, *PHV_MESSAGE;

//
// Define the number of message buffers associated with each port.
//
#define HV_PORT_MESSAGE_BUFFER_COUNT (16)

//
// Define the synthetic interrupt message page layout.
//
typedef struct _HV_MESSAGE_PAGE {
    volatile HV_MESSAGE SintMessage[HV_SYNIC_SINT_COUNT];
} HV_MESSAGE_PAGE, *PHV_MESSAGE_PAGE;

//
// Define the synthetic interrupt controller event flags format.
//
typedef union _HV_SYNIC_EVENT_FLAGS {
    uint8_t Flags8[HV_EVENT_FLAGS_BYTE_COUNT];
    uint32_t Flags32[HV_EVENT_FLAGS_DWORD_COUNT];
} HV_SYNIC_EVENT_FLAGS, *PHV_SYNIC_EVENT_FLAGS;

//
// Define the synthetic interrupt flags page layout.
//
typedef struct _HV_SYNIC_EVENT_FLAGS_PAGE {
    volatile HV_SYNIC_EVENT_FLAGS SintEventFlags[HV_SYNIC_SINT_COUNT];
} HV_SYNIC_EVENT_FLAGS_PAGE, *PHV_SYNIC_EVENT_FLAGS_PAGE;


//
// Define SynIC control register.
//
typedef union _HV_SYNIC_SCONTROL {
    uint64_t Asuint64_t;
    struct {
        uint64_t Enable:1;
        uint64_t Reserved:63;
    };
} HV_SYNIC_SCONTROL, *PHV_SYNIC_SCONTROL;

//
// Define synthetic interrupt source.
//
typedef union _HV_SYNIC_SINT {
    uint64_t Asuint64_t;
    struct {
        uint64_t Vector    :8;
        uint64_t Reserved1 :8;
        uint64_t Masked    :1;
        uint64_t AutoEoi   :1;
        uint64_t Reserved2 :46;
    };
} HV_SYNIC_SINT, *PHV_SYNIC_SINT;

//
// Define the format of the SIMP register
//
typedef union _HV_SYNIC_SIMP {
    uint64_t Asuint64_t;
    struct {
        uint64_t SimpEnabled : 1;
        uint64_t Preserved   : 11;
        uint64_t BaseSimpGpa : 52;
    };
} HV_SYNIC_SIMP, *PHV_SYNIC_SIMP;

//
// Define the format of the SIEFP register
//
typedef union _HV_SYNIC_SIEFP {
    uint64_t Asuint64_t;
    struct {
        uint64_t SiefpEnabled : 1;
        uint64_t Preserved   : 11;
        uint64_t BaseSiefpGpa : 52;
    };
} HV_SYNIC_SIEFP, *PHV_SYNIC_SIEFP;

//
// Definitions for the monitored notification facility
//
typedef union _HV_MONITOR_TRIGGER_GROUP {
    uint64_t Asuint64_t;

    struct {
        uint32_t Pending;
        uint32_t Armed;
    };
} HV_MONITOR_TRIGGER_GROUP, *PHV_MONITOR_TRIGGER_GROUP;

typedef struct _HV_MONITOR_PARAMETER {
    HV_CONNECTION_ID    ConnectionId;
    uint16_t              FlagNumber;
    uint16_t              RsvdZ;
} HV_MONITOR_PARAMETER, *PHV_MONITOR_PARAMETER;

typedef union _HV_MONITOR_TRIGGER_STATE {
    uint32_t Asuint32_t;
        
    struct {
        uint32_t GroupEnable : 4;
        uint32_t RsvdZ       : 28;
    };
} HV_MONITOR_TRIGGER_STATE, *PHV_MONITOR_TRIGGER_STATE;

//
// HV_MONITOR_PAGE Layout
// ------------------------------------------------------
// | 0   | TriggerState (4 bytes) | Rsvd1 (4 bytes)     |
// | 8   | TriggerGroup[0]                              |
// | 10  | TriggerGroup[1]                              |
// | 18  | TriggerGroup[2]                              |
// | 20  | TriggerGroup[3]                              |
// | 28  | Rsvd2[0]                                     |
// | 30  | Rsvd2[1]                                     |
// | 38  | Rsvd2[2]                                     |
// | 40  | NextCheckTime[0][0]    | NextCheckTime[0][1] |
// | ...                                                |
// | 240 | Latency[0][0..3]                             |
// | 340 | Rsvz3[0]                                     |
// | 440 | Parameter[0][0]                              |
// | 448 | Parameter[0][1]                              |
// | ...                                                |
// | 840 | Rsvd4[0]                                     |
// ------------------------------------------------------

typedef struct _HV_MONITOR_PAGE {
    HV_MONITOR_TRIGGER_STATE TriggerState;
    uint32_t                   RsvdZ1;

    HV_MONITOR_TRIGGER_GROUP TriggerGroup[4];
    uint64_t                   RsvdZ2[3];

    int32_t                    NextCheckTime[4][32];

    uint16_t                   Latency[4][32];
    uint64_t                   RsvdZ3[32];

    HV_MONITOR_PARAMETER     Parameter[4][32];

    uint8_t                    RsvdZ4[1984];
} HV_MONITOR_PAGE, *PHV_MONITOR_PAGE;

typedef volatile HV_MONITOR_PAGE* PVHV_MONITOR_PAGE;

#endif  /* __HV_SYNIC_API_H__ */

