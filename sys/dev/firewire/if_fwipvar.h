/*
 * Copyright (c) 2004
 *	Doug Rabson
 * Copyright (c) 2002-2003
 * 	Hidetoshi Shimokawa. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *
 *	This product includes software developed by Hidetoshi Shimokawa.
 *
 * 4. Neither the name of the author nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * 
 * $FreeBSD$
 */

#ifndef _NET_IF_FWIPVAR_H_
#define _NET_IF_FWIPVAR_H_

struct fwip_softc {
	/* XXX this must be first for fd.post_explore() */
	struct firewire_dev_comm fd;
	short dma_ch;
	struct fw_bind fwb;
	struct fw_eui64 last_dest;
	struct fw_pkt last_hdr;
	struct task start_send;
	STAILQ_HEAD(, fw_xfer) xferlist;
	struct fwip_eth_softc {
		/* XXX this must be the first for if_fwsubr.c */
		struct fw_com	fwcom;	/* firewire common data      */
		#define fwip_if		fw_softc.fwcom.fc_if
		struct fwip_softc *fwip;
	} fw_softc;
};
#endif /* !_NET_IF_FWIPVAR_H_ */
