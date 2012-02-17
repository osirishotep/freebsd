/*-
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
 * HyperV FreeBSD netvsc driver implementation
 *
 */

/*-

Copyright (c) 2008, Microsoft. All rights reserved.
Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
	Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer. 
	Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution. 
	Neither the name of the Microsoft nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission. 
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 */


#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/queue.h>
#include <sys/lock.h>
#include <sys/sx.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_dl.h>
#include <net/if_media.h>

#include <net/bpf.h>

#include <net/if_types.h>
#include <net/if.h>

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_kern.h>
#include <vm/pmap.h>

//#include <machine/clock.h>      /* for DELAY */
#include <machine/bus.h>
#include <machine/resource.h>
#include <machine/frame.h>
#include <machine/vmparam.h>

#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/mutex.h>
#include <sys/errno.h>

#include <machine/intr_machdep.h>

#include <dev/hyperv/include/hv_osd.h>
#include <dev/hyperv/include/hv_logging.h>
#include <dev/hyperv/vmbus/hv_vmbus_var.h>
#include <dev/hyperv/vmbus/hv_vmbus_api.h>
#include <dev/hyperv/vmbus/hv_vmbus.h>
#include <dev/hyperv/netvsc/hv_net_vsc_api.h>

#define NETVSC_DEVNAME "hn"
#define ETH_ALEN       6


/*
 * Data types
 */
struct net_device_context {
	/* points back to our device context */
	struct device_context  *device_ctx;
//	struct net_device_stats stats;
};

struct netvsc_driver_context {
	/* !! These must be the first 2 fields !! */
	struct driver_context   drv_ctx;
	netvsc_driver_object    drv_obj;
	uint32_t		drv_inited;
};

#define SN_LOCK_INIT(_sc, _name) \
	    mtx_init(&(_sc)->hn_lock, _name, MTX_NETWORK_LOCK, MTX_DEF)
#define SN_LOCK(_sc)		mtx_lock(&(_sc)->hn_lock)
#define SN_LOCK_ASSERT(_sc)	mtx_assert(&(_sc)->hn_lock, MA_OWNED)
#define SN_UNLOCK(_sc)		mtx_unlock(&(_sc)->hn_lock)
#define SN_LOCK_DESTROY(_sc)	mtx_destroy(&(_sc)->hn_lock)

static int netvsc_ringbuffer_size = NETVSC_DEVICE_RING_BUFFER_SIZE;


/*
 * Globals
 */

int hv_promisc_mode = 0;    /* normal mode by default */

/* Fixme:  Should this be hv_promisc_mode, defined above? */
int promisc_mode;

#ifdef REMOVED
/*
 * Fixme:  Do we need promiscuous mode?
 * Fixme:  Are we able to get promiscuous mode if we need it?
 */
SYSCTL_INT(_netscaler, OID_AUTO, hv_promisc_mode, CTLFLAG_RD,
                   &hv_promisc_mode, 0, "HYPER_V driver promisc mode ");
#endif

/* The one and only one */
static struct netvsc_driver_context g_netvsc_drv;

/*
 * Forward declarations
 */
static void hn_stop(hn_softc_t *sc);
static void hn_ifinit_locked(hn_softc_t *sc);
static void hn_ifinit(void *xsc);
static int  hn_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data);
static int  hn_start_locked(struct ifnet *ifp);
static void hn_start(struct ifnet *ifp);

static void netvsc_xmit_completion(void *context);
/* Fixme:  Function pointer removal */
//static int  netvsc_recv_callback(DEVICE_OBJECT *device_obj,
//				 netvsc_packet *packet);
//static void netvsc_linkstatus_callback(DEVICE_OBJECT *, uint32_t);
/* Fixme:  Function pointer removal */
//static int  netvsc_drv_init(PFN_DRIVERINITIALIZE pfn_drv_init);
static int  netvsc_drv_init(void);


/*
 * NetVsc driver initialization
 */
static int
/* Fixme:  Function pointer removal */
//netvsc_drv_init(PFN_DRIVERINITIALIZE pfn_drv_init)
netvsc_drv_init(void)
{
	int ret = 0;
	netvsc_driver_object *net_drv_obj = &g_netvsc_drv.drv_obj;
	struct driver_context *drv_ctx = &g_netvsc_drv.drv_ctx;

	DPRINT_ENTER(NETVSC_DRV);

	vmbus_get_interface(&net_drv_obj->base.VmbusChannelInterface);

	net_drv_obj->ring_buf_size = netvsc_ringbuffer_size;
	net_drv_obj->on_rx_callback = netvsc_recv_callback;
	net_drv_obj->on_link_stat_changed = netvsc_linkstatus_callback;

	/* Callback to client driver to complete the initialization */
	/* Fixme:  Function pointer removal */
	//pfn_drv_init(&net_drv_obj->base);
	hv_net_vsc_initialize(&net_drv_obj->base);

	memcpy(&drv_ctx->class_id, &net_drv_obj->base.deviceType, sizeof(GUID));

	/* The driver belongs to vmbus */
	vmbus_child_driver_register(drv_ctx);

	DPRINT_EXIT(NETVSC_DRV);

	return (ret);
}

/*
 * NetVsc global initialization entry point
 */
static void
netvsc_init(void)
{
	DPRINT_ENTER(NETVSC_DRV);
	printf("Netvsc initializing....");

	if (!g_netvsc_drv.drv_inited) {
		/* Fixme:  Function pointer removal */
		//netvsc_drv_init(hv_net_vsc_initialize);
		netvsc_drv_init();
		atomic_set_int(&g_netvsc_drv.drv_inited, 1);
	}

	DPRINT_EXIT(NETVSC_DRV);
}

/*
 *
 */
static int
netvsc_probe(device_t dev)
{
	const char *p;

	/* 
	 * If the system has already booted and thread
	 * scheduling is possible indicated by the global
	 * cold set to zero, we just call the driver
	 * initialization directly.
	 */
	if (!cold && !g_netvsc_drv.drv_inited) {
		netvsc_init();
	}

	p = vmbus_get_type(dev);
	if (!memcmp(p, &g_netvsc_drv.drv_obj.base.deviceType, sizeof(GUID))) {
		device_set_desc(dev, "Synthetic Network Interface");
		printf("Netvsc probe... DONE \n");
		return (0);
	}

	return (ENXIO);
}

/*
 *
 */
static int
netvsc_attach(device_t dev)
{
	netvsc_driver_object *net_drv_obj = &g_netvsc_drv.drv_obj;
	struct device_context *device_ctx = vmbus_get_devctx(dev);
	netvsc_device_info device_info;
	hn_softc_t *sc;
	int unit = device_get_unit(dev);
	struct ifnet *ifp;
	int ret;

	sc = device_get_softc(dev);
	if (sc == NULL) {
		DPRINT_ERR(NETVSC_DRV, "%s%d not configured", NETVSC_DEVNAME,
		    unit);

		return (ENOMEM);
	}

	if (!net_drv_obj->base.OnDeviceAdd) {
		DPRINT_ERR(NETVSC_DRV, "OnDeviceAdd is not initialized");

		return (-1);
	}

	bzero(sc, sizeof(hn_softc_t));
	sc->hn_unit = unit;
	sc->hn_dev = dev;

	SN_LOCK_INIT(sc, "NetVSCLock");

	sc->hn_dev_obj = &device_ctx->device_obj;

	device_ctx->device_obj.Driver = &g_netvsc_drv.drv_obj.base;
	ret = net_drv_obj->base.OnDeviceAdd(&device_ctx->device_obj,
	    (void*)&device_info);

	if (ret != 0) {
		DPRINT_ERR(NETVSC_DRV, "unable to add netvsc device (ret %d)",
		    ret);
		
		return (ret);
	}

	if (device_info.link_state == 0) {
		sc->hn_carrier = 1;
	}

	DPRINT_DBG(NETVSC_DRV, 
	    "netvsc_attach: mac address: %02x %02x %02x %02x %02x %02x\n",
	    device_info.mac_addr[0], device_info.mac_addr[1],
	    device_info.mac_addr[2], device_info.mac_addr[3],
	    device_info.mac_addr[4], device_info.mac_addr[5]);

	ifp = sc->hn_ifp = sc->arpcom.ac_ifp = if_alloc(IFT_ETHER);
	ifp->if_softc = sc;

	if_initname(ifp, device_get_name(dev), device_get_unit(dev));
	ifp->if_dunit = unit;
	ifp->if_dname = NETVSC_DEVNAME;

	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = hn_ioctl;
	ifp->if_output = ether_output;
	ifp->if_start = hn_start;
	/* Fixme:  Should have this */
//	ifp->if_watchdog = hn_watchdog;
	ifp->if_init = (void*)hn_ifinit;
	ifp->if_mtu = ETHERMTU;
	IFQ_SET_MAXLEN(&ifp->if_snd, 512);
	ifp->if_snd.ifq_drv_maxlen = 511;
	IFQ_SET_READY(&ifp->if_snd);

	/* Fixme -- should we have a copy of MAC addr in softc? */
	ether_ifattach(ifp, device_info.mac_addr);

	return (0);
}

/*
 *
 */
static int
netvsc_detach(device_t dev)
{
	printf("netvsc_detach\n");

	return (0);
}

/*
 *
 */
static int
netvsc_shutdown(device_t dev)
{
//	printf("netvsc_shutdown\n");

	return (0);
}

/*
 * Send completion processing
 */
static void
netvsc_xmit_completion(void *context)
{
	netvsc_packet *packet = (netvsc_packet *)context;
	struct mbuf *m;
	unsigned char *buf;
	//void *sc;

	DPRINT_ENTER(NETVSC_DRV);

	m = (struct mbuf *)packet->compl.send.send_completion_tid;
	/* Fixme:  magic number */
	buf = ((unsigned char *)packet) - 16;
	/* Fixme:  not used */
	//sc = (void *)(*(vm_offset_t *)buf);

	free(buf, M_DEVBUF);

	if (m) {
		m_freem(m);
	}

	DPRINT_EXIT(NETVSC_DRV);
}

/*
 * Start a transmit
 */
static int
hn_start_locked(struct ifnet *ifp)
{
	int ret = 0;
	hn_softc_t *sc = ifp->if_softc;
	netvsc_driver_object *net_drv_obj = &g_netvsc_drv.drv_obj;
	struct device_context *device_ctx = vmbus_get_devctx(sc->hn_dev);

	int i;
	unsigned char *buf;

	netvsc_packet *packet;
	int num_frags = 0;
	int retries = 0;
	struct mbuf *m_head, *m;
	int len = 0;
	int xlen = 0;

	DPRINT_ENTER(NETVSC_DRV);

	while (!IFQ_DRV_IS_EMPTY(&sc->hn_ifp->if_snd)) {

		IFQ_DRV_DEQUEUE(&sc->hn_ifp->if_snd, m_head);
		if (m_head == NULL) {
			break;
		}

		len = 0;
		num_frags = 0;
		xlen = 0;

		for (m = m_head; m != NULL; m = m->m_next) {
			if (m->m_len != 0) {
				num_frags++;
				len += m->m_len;
			}
		}

		DPRINT_DBG(NETVSC_DRV, "xmit packet - len %d", len);

		/* Add 1 for skb->data and any additional ones requested */
		num_frags += net_drv_obj->additional_request_page_buf_cnt;

		/* Allocate a netvsc packet based on # of frags. */
		/* Fixme:  magic number */
		buf = malloc(16 + sizeof(netvsc_packet) + 
		    (num_frags * sizeof(PAGE_BUFFER)) + 
		    net_drv_obj->request_ext_size, 
		    M_DEVBUF, M_ZERO | M_WAITOK);

		if (buf == NULL) {
			DPRINT_ERR(NETVSC_DRV, "Cannot allocate netvsc_packet");
			return (-1);
		}

		/* Fixme:  magic number */
		packet = (netvsc_packet *)(buf + 16);
		*(vm_offset_t *)buf = 0;

		packet->extension = (void *)((unsigned long)packet + 
		    sizeof(netvsc_packet) + (num_frags * sizeof(PAGE_BUFFER))) ;

		/* Set up the rndis header */
		packet->page_buf_count = num_frags;

		/* TODO: Flush all write buffers / memory fence ??? */
		//wmb();
	
		/* Initialize it from the mbuf */
		packet->tot_data_buf_len = len;

		/*
		 * Start filling in the page buffers starting at
		 * AdditionalRequestPageBufferCount offset
		 */
		i = net_drv_obj->additional_request_page_buf_cnt;
		for (m = m_head; m != NULL; m = m->m_next) {
			if (m->m_len) {
				vm_offset_t paddr =
				    vtophys(mtod(m, vm_offset_t));
				packet->page_buffers[i].Pfn =
				    paddr >> PAGE_SHIFT;
				packet->page_buffers[i].Offset =
				    paddr & (PAGE_SIZE - 1);
				packet->page_buffers[i].Length = m->m_len;
				DPRINT_DBG(NETVSC_DRV, 
				    "vaddr: %lx, pfn: %lx, Off: %x, len: %x\n", 
				    paddr, packet->page_buffers[i].Pfn, 
				    packet->page_buffers[i].Offset, 
				    packet->page_buffers[i].Length);

				i++;
			}
		}

		/* Set the completion routine */
		packet->compl.send.on_send_completion =
		    netvsc_xmit_completion;
		packet->compl.send.send_completion_context = packet;
		packet->compl.send.send_completion_tid = (ULONG_PTR)m_head;
retry_send:
		critical_enter();
		ret = net_drv_obj->on_send(&device_ctx->device_obj, packet);
		critical_exit();

		if (ret == 0) {
			ifp->if_opackets++;
			if (ifp->if_bpf)
				bpf_mtap(ifp->if_bpf, m_head);
//			if (ifp->if_timer == 0)
//				ifp->if_timer = 5;
		} else {
			retries++;
			if (retries < 4) {
				DPRINT_ERR(NETVSC_DRV,
				    "unable to send...retrying %d...", retries);
				goto retry_send;
			}

			DPRINT_INFO(NETVSC_DRV, "net device (%p) stopping", sc);
			IF_PREPEND(&ifp->if_snd, m_head);
			ifp->if_drv_flags |= IFF_DRV_OACTIVE;

			ret = -1;
			/* Fixme: ??? */
//			net_device_ctx->stats.tx_dropped++;

			/*
			 * Null it since the caller will free it instead of
			 * the completion routine
			 */
			packet->compl.send.send_completion_tid = 0;

			/*
			 * Release the resources since we will not get any
			 * send completion
			 */
			netvsc_xmit_completion(packet);
		}
	}

//	DPRINT_DBG(NETVSC_DRV, "# of xmits %lu total size %lu",
//	    net_device_ctx->stats.tx_packets, net_device_ctx->stats.tx_bytes);

	DPRINT_EXIT(NETVSC_DRV);

	return (ret);
}

/*
 * Link up/down notification
 */
void
netvsc_linkstatus_callback(DEVICE_OBJECT *device_obj, uint32_t status)
{
	struct device_context *device_ctx = to_device_context(device_obj);
	hn_softc_t *sc = device_get_softc(device_ctx->device);

	DPRINT_ENTER(NETVSC_DRV);

	if (!sc) {
		DPRINT_ERR(NETVSC_DRV,
		    "got link status but net device not initialized yet");

		return;
	}

	if (status == 1) {
		sc->hn_carrier = 1;
	} else {
		sc->hn_carrier = 0;
	}
	DPRINT_EXIT(NETVSC_DRV);
}

/*
 * RX Callback.  Called when we receive a packet from the "wire" on the
 * specified device
 */
int
netvsc_recv_callback(DEVICE_OBJECT *device_obj, netvsc_packet *packet)
{
	struct device_context *device_ctx = to_device_context(device_obj);
	hn_softc_t *sc = (hn_softc_t *)device_get_softc(device_ctx->device);

	struct mbuf *m_new;
	struct ifnet *ifp = sc->hn_ifp;
	int i;

	DPRINT_ENTER(NETVSC_DRV);

	if (!sc) {
		DPRINT_ERR(NETVSC_DRV, "got receive callback but net device "
		    "not yet initialized");

		return (0);
	}
	
	ifp = sc->arpcom.ac_ifp;

	if (!(ifp->if_drv_flags & IFF_DRV_RUNNING)) {
		return (0);
	}

	if (packet->tot_data_buf_len > MCLBYTES) {
		DPRINT_ERR(NETVSC_DRV, "rx error: packet length: %x", 
		    packet->tot_data_buf_len);

		return (0);
	}

	MGETHDR(m_new, M_DONTWAIT, MT_DATA);
	if (m_new == NULL) {
		DPRINT_ERR(NETVSC_DRV, "Failed to allocate mbuf header");

		return (0);
	}
	MCLGET(m_new, M_DONTWAIT);
	if ((m_new->m_flags & M_EXT) == 0) {
		DPRINT_ERR(NETVSC_DRV, "Failed to allocate mbuf cluster");
		m_freem(m_new);

		return (0);
	}

	/*
	 * Copy the received packet to one or more mbufs. 
	 * The copy is required since the memory pointed to by netvsc_packet
	 * cannot be deallocated
	 */
	for (i=0; i < packet->page_buf_count; i++) {
		/* Shift virtual page number to form virtual page address */
		unsigned char *vaddr = (unsigned char *)
		    (packet->page_buffers[i].Pfn << PAGE_SHIFT);

		m_append(m_new, packet->page_buffers[i].Length,
		    vaddr + packet->page_buffers[i].Offset);
	}

	m_new->m_pkthdr.len = m_new->m_len = packet->tot_data_buf_len -
	    ETHER_CRC_LEN;
	m_new->m_pkthdr.rcvif = ifp;

	hv_nv_on_receive_completion(
	    (void *)packet->compl.rx.rx_completion_context);
	ifp->if_ipackets++;
	/* Fixme:  Is the lock held? */
//	SN_UNLOCK(sc);
	(*ifp->if_input)(ifp, m_new);
//	SN_LOCK(sc);

	DPRINT_EXIT(NETVSC_DRV);

	return (0);
}

/*
 *
 */
static int
hn_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	hn_softc_t *sc = ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *) data;
//	struct ifaddr *ifa = (struct ifaddr *)data;

	int mask, error = 0;

	switch(cmd) {
	case SIOCSIFADDR:
	case SIOCGIFADDR:
		error = ether_ioctl(ifp, cmd, data);
		break;
	case SIOCSIFMTU:
		ifp->if_mtu = ifr->ifr_mtu;
//		ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
		hn_ifinit(sc);
		break;
	case SIOCSIFFLAGS:
		SN_LOCK(sc);
		if (ifp->if_flags & IFF_UP) {
			/*
			 * If only the state of the PROMISC flag changed,
			 * then just use the 'set promisc mode' command
			 * instead of reinitializing the entire NIC. Doing
			 * a full re-init means reloading the firmware and
			 * waiting for it to start up, which may take a
			 * second or two.
			 */
#ifdef notyet
			/* Fixme:  Promiscuous mode? */
			/* No promiscuous mode with Xen */
			if (ifp->if_drv_flags & IFF_DRV_RUNNING &&
			    ifp->if_flags & IFF_PROMISC &&
			    !(sc->hn_if_flags & IFF_PROMISC)) {
				/* do something here for Hyper-V */
				;
//				XN_SETBIT(sc, XN_RX_MODE,
//					  XN_RXMODE_RX_PROMISC);
			} else if (ifp->if_drv_flags & IFF_DRV_RUNNING &&
				   !(ifp->if_flags & IFF_PROMISC) &&
				   sc->hn_if_flags & IFF_PROMISC) {
				/* do something here for Hyper-V */
				;
//				XN_CLRBIT(sc, XN_RX_MODE,
//					  XN_RXMODE_RX_PROMISC);
			} else
#endif
				hn_ifinit_locked(sc);
		} else {
			if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
				hn_stop(sc);
			}
		}
		sc->hn_if_flags = ifp->if_flags;
		SN_UNLOCK(sc);
		error = 0;
		break;
	case SIOCSIFCAP:
		mask = ifr->ifr_reqcap ^ ifp->if_capenable;
		if (mask & IFCAP_HWCSUM) {
			if (IFCAP_HWCSUM & ifp->if_capenable)
				ifp->if_capenable &= ~IFCAP_HWCSUM;
			else
				ifp->if_capenable |= IFCAP_HWCSUM;
		}
		error = 0;
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
#ifdef notyet
		/* Fixme:  Multicast mode? */
		if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
			SN_LOCK(sc);
			netvsc_setmulti(sc);
			SN_UNLOCK(sc);
			error = 0;
		}
#endif
		/* FALLTHROUGH */
	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		error = EINVAL;
		break;
	default:
		error = ether_ioctl(ifp, cmd, data);
		break;
	}
    
	return (error);
}

/*
 *
 */
static void
hn_stop(hn_softc_t *sc)
{
	struct ifnet *ifp;
	int ret;
	netvsc_driver_object *net_drv_obj = &g_netvsc_drv.drv_obj;
	struct device_context *device_ctx = vmbus_get_devctx(sc->hn_dev);

	SN_LOCK_ASSERT(sc);
	ifp = sc->hn_ifp;

	printf(" Closing Device ...\n");

	ifp->if_drv_flags &= ~(IFF_DRV_RUNNING | IFF_DRV_OACTIVE);
	sc->hn_initdone = 0;

	ret = net_drv_obj->on_close(&device_ctx->device_obj);
	if (ret != 0) {
		DPRINT_ERR(NETVSC_DRV, "unable to close device (ret %d).", ret);
	}
}

/*
 *
 */
static void
hn_start(struct ifnet *ifp)
{
	hn_softc_t *sc;

	sc = ifp->if_softc;
	SN_LOCK(sc);
	hn_start_locked(ifp);
	SN_UNLOCK(sc);
}

/*
 *
 */
static void
hn_ifinit_locked(hn_softc_t *sc)
{
	struct ifnet *ifp;
	netvsc_driver_object *net_drv_obj = &g_netvsc_drv.drv_obj;
	struct device_context *device_ctx = vmbus_get_devctx(sc->hn_dev);
	int ret;

	SN_LOCK_ASSERT(sc);

	ifp = sc->hn_ifp;

	if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
		return;
	}

	promisc_mode = 1;

	ret = net_drv_obj->on_open(&device_ctx->device_obj);
	if (ret != 0) {
		DPRINT_ERR(NETVSC_DRV, "unable to open device (ret %d).", ret);
		return;
	} else {
		sc->hn_initdone = 1;
	}
	ifp->if_drv_flags |= IFF_DRV_RUNNING;
	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
}

/*
 *
 */
static void
hn_ifinit(void *xsc)
{
	hn_softc_t *sc = xsc;

	SN_LOCK(sc);
	hn_ifinit_locked(sc);
	SN_UNLOCK(sc);
}

#ifdef LATER
/*
 *
 */
static void
hn_watchdog(struct ifnet *ifp)
{
	hn_softc_t *sc;
	sc = ifp->if_softc;

	printf("sx%d: watchdog timeout -- resetting\n", sc->hn_unit);
	hn_ifinit(sc);    /*???*/
	ifp->if_oerrors++;
}
#endif

static device_method_t netvsc_methods[] = {
        /* Device interface */
        DEVMETHOD(device_probe,         netvsc_probe),
        DEVMETHOD(device_attach,        netvsc_attach),
        DEVMETHOD(device_detach,        netvsc_detach),
        DEVMETHOD(device_shutdown,      netvsc_shutdown),

        { 0, 0 }
};

static driver_t netvsc_driver = {
        NETVSC_DEVNAME,
        netvsc_methods,
        sizeof(hn_softc_t)
};

static devclass_t netvsc_devclass;

DRIVER_MODULE(hn, vmbus, netvsc_driver, netvsc_devclass, 0, 0);
MODULE_VERSION(hn, 1);
MODULE_DEPEND(hn, vmbus, 1, 1, 1);
SYSINIT(netvsc_initx, SI_SUB_RUN_SCHEDULER, SI_ORDER_MIDDLE + 1, netvsc_init,
     NULL);

