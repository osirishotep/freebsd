/***********************************************
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
 * HyperV FreeBSD vmbus driver implementation
 *
 *****************************************************************************/

/*
 * Name:	vmbus_drv.c
 *
 * Desc:	vmbus driver implementation
 *
 */

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <sys/systm.h>
#include <sys/rtprio.h>
#include <sys/interrupt.h>
#include <sys/sx.h>
#include <sys/taskqueue.h>
#include <sys/mutex.h>
#include <sys/smp.h>

#include <machine/resource.h>
#include <sys/rman.h>

#include <machine/stdarg.h>
#include <machine/intr_machdep.h>
#include <sys/pcpu.h>

#include "hyperv.h"
#include "vmbus_priv.h"


#define VMBUS_IRQ				0x5

static struct intr_event *hv_message_intr_event;
static struct intr_event *hv_event_intr_event;
static void *msg_swintr;
static void *event_swintr;
static device_t vmbus_devp;
static void *vmbus_cookiep;
static int vmbus_rid;
struct resource *intr_res;
static int vmbus_irq = VMBUS_IRQ;
static int vmbus_inited;


/*
 * vmbus_msg_swintr()
 *
 * Description:
 * Software interrupt thread routine to handle channel messages from
 * the hypervisior
 */
static void
vmbus_msg_swintr(void *dummy)
{
	int cpu;
	void *page_addr;
	hv_vmbus_message *msg;
	hv_vmbus_message *copied;

	cpu = PCPU_GET(cpuid);
	page_addr = hv_vmbus_g_context.syn_ic_message_page[cpu];
	msg = (hv_vmbus_message*) page_addr + HV_VMBUS_MESSAGE_SINT;
	while (1) {
		if (msg->header.message_type == HV_MESSAGE_TYPE_NONE) {
			break; /* no message */
		} else {
			copied = malloc(sizeof(hv_vmbus_message), M_DEVBUF, M_NOWAIT);
			if (copied == NULL) {
				continue;
			}

			memcpy(copied, msg, sizeof(hv_vmbus_message));
			hv_queue_work_item(hv_vmbus_g_connection.work_queue,
				hv_vmbus_on_channel_message, copied);
		}

		msg->header.message_type = HV_MESSAGE_TYPE_NONE;

		// Make sure the write to message_type (ie set to HV_MESSAGE_TYPE_NONE) happens
		// before we read the message_pending and EOMing. Otherwise, the EOMing will not deliver
		// any more messages since there is no empty slot
		wmb();

		if (msg->header.message_flags.message_pending) {
			// This will cause message queue rescan to possibly deliver another msg from the hypervisor
			hv_vmbus_write_msr(HV_X64_MSR_EOM, 0);
		}
	}
}

/*
 * hv_vmbus_isr()
 *
 * Interrupt filter routine for VMBUS.
 * The purpose of this routine is to determine the type of VMBUS protocol
 * message to process - an event or a channel message.
 * As this is an interrupt filter routine, the function runs in a very
 * restricted envinronment.  From the manpage for bus_setup_intr(9)
 *
 *   In this restricted environment, care must be taken to account for all
 *   races.  A careful analysis of races should be done as well.  It is gener-
 *   ally cheaper to take an extra interrupt, for example, than to protect
 *   variables with spinlocks.	Read, modify, write cycles of hardware regis-
 *   ters need to be carefully analyzed if other threads are accessing the
 *   same registers.
 */
static int
hv_vmbus_isr(void *unused) 
{
	int cpu;
	void *page_addr;
	hv_vmbus_message* msg;
	HV_SYNIC_EVENT_FLAGS* event;

	cpu = PCPU_GET(cpuid);

	KASSERT(cpu == 0, ("hv_vmbus_isr: Interrupt on CPU other than zero"));

	/*
	 * Check for events before checking for messages. This is the order
	 * in which events and messages are checked in Windows guests on Hyper-V
	 * and the Windows team suggested we do the same here.
	 */

	page_addr = hv_vmbus_g_context.syn_ic_event_page[cpu];
	event = (HV_SYNIC_EVENT_FLAGS*) page_addr + HV_VMBUS_MESSAGE_SINT;

	// Since we are a child, we only need to check bit 0
	if (synch_test_and_clear_bit(0, &event->flags32[0])) {
		swi_sched(event_swintr, 0);
	}

	// Check if there are actual msgs to be process
	page_addr = hv_vmbus_g_context.syn_ic_message_page[cpu];
	msg = (hv_vmbus_message*) page_addr + HV_VMBUS_MESSAGE_SINT;

	if (msg->header.message_type != HV_MESSAGE_TYPE_NONE) {
		swi_sched(msg_swintr, 0);
	}

	return FILTER_HANDLED;
}

static int vmbus_read_ivar(device_t dev, device_t child, int index,
	uintptr_t *result) {
	struct hv_device *child_dev_ctx = device_get_ivars(child);

	switch (index) {

	case HV_VMBUS_IVAR_TYPE:
		*result = (uintptr_t) &child_dev_ctx->class_id;
		return (0);
	case HV_VMBUS_IVAR_INSTANCE:
		*result = (uintptr_t) &child_dev_ctx->device_id;
		return (0);
	case HV_VMBUS_IVAR_DEVCTX:
		*result = (uintptr_t) child_dev_ctx;
		return (0);
	case HV_VMBUS_IVAR_NODE:
		*result = (uintptr_t) child_dev_ctx->device;
		return (0);
	}
	return (ENOENT);
}

static int vmbus_write_ivar(device_t dev, device_t child, int index,
	uintptr_t value) {
	switch (index) {

	case HV_VMBUS_IVAR_TYPE:
	case HV_VMBUS_IVAR_INSTANCE:
	case HV_VMBUS_IVAR_DEVCTX:
	case HV_VMBUS_IVAR_NODE:
		/* read-only */
		return (EINVAL);
	}
	return (ENOENT);
}

struct hv_device *vmbus_child_device_create(hv_guid type,
						hv_guid instance,
						hv_vmbus_channel *channel ) 
{
	struct hv_device *child_dev;

	// Allocate the new child device
	child_dev = malloc(sizeof(struct hv_device), M_DEVBUF,
			M_NOWAIT |  M_ZERO);
	if (!child_dev) 
		return NULL;

	child_dev->channel = channel;
	memcpy(&child_dev->class_id, &type, sizeof(hv_guid));
	memcpy(&child_dev->device_id, &instance, sizeof(hv_guid));

	return child_dev;
}

static void print_dev_guid(struct hv_device *dev)
{
        int i;
	unsigned char guid_name[100];
        for (i = 0; i < 32; i += 2)
                sprintf(&guid_name[i], "%02x", dev->class_id.data[i/2]);
	printf("Class ID: %s\n", guid_name);
}


int vmbus_child_device_register(struct hv_device *child_dev)
{
	device_t child;
	int ret = 0;

	print_dev_guid(child_dev);


	child = device_add_child(vmbus_devp, NULL, -1);
	child_dev->device = child;
	device_set_ivars(child, child_dev);

	mtx_lock(&Giant);
	ret = device_probe_and_attach(child);
	mtx_unlock(&Giant);

	return 0;
}

int vmbus_child_device_unregister(struct hv_device *child_dev)
{
	/*
	 * XXXKYS: Ensure that this is the opposite of
	 * device_add_child()
	 */
	return(device_delete_child(vmbus_devp, child_dev->device));
}

static int vmbus_print_child(device_t dev, device_t child) {
	int retval = 0;

	retval += bus_print_child_header(dev, child);
	retval += bus_print_child_footer(dev, child);

	return (retval);
}

static void vmbus_identify(driver_t *driver, device_t parent) {
	BUS_ADD_CHILD(parent, 0, "vmbus", 0);
	if (device_find_child(parent, "vmbus", 0) == NULL) {
		BUS_ADD_CHILD(parent, 0, "vmbus", 0);
	}
}

static int vmbus_probe(device_t dev) {
	printf("vmbus_probe\n");

	if (!hv_vmbus_query_hypervisor_presence())
		return (ENXIO);

	device_set_desc(dev, "Vmbus Devices");

	return (0);
}


/*++

 Name:   vmbus_bus_init()

 Desc:   Main vmbus driver initialization routine. Here, we
 - initialize the vmbus driver context
 - setup various driver entry points
 - invoke the vmbus hv main init routine
 - get the irq resource
 - invoke the vmbus to add the vmbus root device
 - setup the vmbus root device
 - retrieve the channel offers
 --*/

static int vmbus_bus_init(void) 
{
	int ret;
	unsigned int vector = 0;
	struct intsrc *isrc;

	if (vmbus_inited)
		return 0;

	vmbus_inited = 1;

	ret = hv_vmbus_init();

	if (ret) {
		printf("Hypervisor Initialization Failed\n");
		return ret;
	}

	ret = swi_add(&hv_message_intr_event, "hv_msg", vmbus_msg_swintr,
		NULL, SWI_CLOCK, 0, &msg_swintr);

	if (ret)
		goto cleanup;

	/*
	 * Message SW interrupt handler checks a per-CPU page and
	 * thus the thread needs to be bound to CPU-0 - which is where
	 * all interrupts are processed.
	 */
	ret = intr_event_bind(hv_message_intr_event, 0);

	if (ret)
		goto cleanup1;

	ret = swi_add(&hv_event_intr_event, "hv_event", vmbus_on_events,
		NULL, SWI_CLOCK, 0, &event_swintr);

	if (ret)
		goto cleanup1;

	intr_res = bus_alloc_resource(vmbus_devp,
		SYS_RES_IRQ, &vmbus_rid, vmbus_irq, vmbus_irq, 1, RF_ACTIVE);

	if (intr_res == NULL) {
		ret = ENOMEM; /* XXXKYS: Need a better errno */
		goto cleanup2;
	}

	/* Setup interrupt filter handler */
	ret = bus_setup_intr(vmbus_devp, intr_res,
		INTR_TYPE_NET | INTR_FAST | INTR_MPSAFE, hv_vmbus_isr, NULL,
		NULL, &vmbus_cookiep);

	if (ret != 0)
		goto cleanup3;

	ret = bus_bind_intr(vmbus_devp, intr_res, 0);
	if (ret != 0) 
		goto cleanup4;

	isrc = intr_lookup_source(vmbus_irq);
	if ((isrc == NULL) || (isrc->is_event == NULL)) {
		ret = EINVAL;
		goto cleanup4;
	}

	vector = isrc->is_event->ie_vector;
	printf("VMBUS: irq 0x%x vector 0x%x\n", vmbus_irq, vector);

	/*
	 * Notify the hypervisor of our irq.
	 */

	smp_rendezvous(NULL, hv_vmbus_synic_init, NULL, &vector);

	// Connect to VMBus in the root partition
	ret = hv_vmbus_connect();

	if (ret)
		goto cleanup4;

	hv_vmbus_request_channel_offers();
	return ret;

cleanup4:

	/* remove swi, bus and intr resource */
	bus_teardown_intr(vmbus_devp, intr_res, vmbus_cookiep);

cleanup3:

	bus_release_resource(vmbus_devp, SYS_RES_IRQ, vmbus_rid, intr_res);

cleanup2: 
	swi_remove(event_swintr);

cleanup1:
	swi_remove(msg_swintr);

cleanup:
	hv_vmbus_cleanup();

	return ret;
}

static int vmbus_attach(device_t dev) {
	printf("vmbus_attach: dev: %p\n", dev);
	vmbus_devp = dev;

	/* 
	 * If the system has already booted and thread
	 * scheduling is possible indicated by the global
	 * cold set to zero, we just call the driver
	 * initialization directly.
	 */
	if (!cold) {
		vmbus_bus_init();
	}

	return 0;
}

static void vmbus_init(void) 
{
	/* 
	 * If the system has already booted and thread
	 * scheduling is possible indicated by the global
	 * cold set to zero, we just call the driver
	 * initialization directly.
	 */
	if (!cold) {
		vmbus_bus_init();
	}
}

static void vmbus_bus_exit(void) 
{

	hv_vmbus_release_unattached_channels();
	hv_vmbus_disconnect();

	smp_rendezvous(NULL, hv_vmbus_synic_cleanup, NULL, NULL);

	hv_vmbus_cleanup();

	/* remove swi, bus and intr resource */
	bus_teardown_intr(vmbus_devp, intr_res, vmbus_cookiep);

	bus_release_resource(vmbus_devp, SYS_RES_IRQ, vmbus_rid, intr_res);

	swi_remove(msg_swintr);
	swi_remove(event_swintr);

	return;
}

static void vmbus_exit(void) 
{
	vmbus_bus_exit();

}

static int vmbus_detach(device_t dev) 
{
	vmbus_exit();
	return 0;
}

static void vmbus_mod_load(void) 
{
	printf("Vmbus load\n");
}

static void vmbus_mod_unload(void) 
{
	printf("Vmbus unload\n");
}

static int vmbus_modevent(module_t mod, int what, void *arg) 
{
	switch (what) {

	case MOD_LOAD:
		vmbus_mod_load();
		break;
	case MOD_UNLOAD:
		vmbus_mod_unload();
		break;
	}

	return (0);
}

static device_method_t vmbus_methods[] = {
	/* Device interface */DEVMETHOD(device_identify, vmbus_identify),
	DEVMETHOD(device_probe, vmbus_probe),
	DEVMETHOD(device_attach, vmbus_attach),
	DEVMETHOD(device_detach, vmbus_detach),
	DEVMETHOD(device_shutdown, bus_generic_shutdown),
	DEVMETHOD(device_suspend, bus_generic_suspend),
	DEVMETHOD(device_resume, bus_generic_resume),

	/* Bus interface */DEVMETHOD(bus_add_child, bus_generic_add_child),
	DEVMETHOD(bus_print_child, vmbus_print_child),
	DEVMETHOD(bus_read_ivar, vmbus_read_ivar),
	DEVMETHOD(bus_write_ivar, vmbus_write_ivar),

	{ 0, 0 } };

static char driver_name[] = "vmbus";
static driver_t vmbus_driver = { driver_name, vmbus_methods,
	0, };


devclass_t vmbus_devclass;

DRIVER_MODULE(vmbus, nexus, vmbus_driver, vmbus_devclass, vmbus_modevent, 0);
MODULE_VERSION(vmbus,1);

// TODO: We want to be earlier than SI_SUB_VFS
SYSINIT(vmb_init, SI_SUB_VFS, SI_ORDER_MIDDLE, vmbus_init, NULL);

