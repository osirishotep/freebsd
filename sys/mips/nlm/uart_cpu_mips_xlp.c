/*-
 * Copyright 2003-2011 Netlogic Microsystems (Netlogic). All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY Netlogic Microsystems ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL NETLOGIC OR CONTRIBUTORS BE 
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * NETLOGIC_BSD */

/*
 * Skeleton of this file was based on respective code for ARM
 * code written by Olivier Houchard.
 */
/*
 * XLRMIPS: This file is hacked from arm/...
 */
#include "opt_uart.h"

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: head/sys/mips/rmi/uart_cpu_mips_xlr.c 202175 2010-01-12 21:36:08Z imp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/cons.h>
#include <sys/kdb.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>

#include <machine/bus.h>

#include <dev/uart/uart.h>
#include <dev/uart/uart_cpu.h>

#include <mips/nlm/hal/mmio.h>
#include <mips/nlm/hal/iomap.h>
#include <mips/nlm/hal/uart.h>

bus_space_tag_t uart_bus_space_io;
bus_space_tag_t uart_bus_space_mem;

int
uart_cpu_eqres(struct uart_bas *b1, struct uart_bas *b2)
{
	return ((b1->bsh == b2->bsh && b1->bst == b2->bst) ? 1 : 0);
}


int
uart_cpu_getdev(int devtype, struct uart_devinfo *di)
{
	di->ops = uart_getops(&uart_ns8250_class);
	di->bas.chan = 0;
	di->bas.bst = rmi_bus_space;
	di->bas.bsh = nlm_regbase_uart(0, 0) + XLP_IO_PCI_HDRSZ;
	
	di->bas.regshft = 2;
	/* divisor = rclk / (baudrate * 16); */
	di->bas.rclk = 133000000;
	di->baudrate = 115200;
	di->databits = 8;
	di->stopbits = 1;
	di->parity = UART_PARITY_NONE;

	uart_bus_space_io = NULL;
	uart_bus_space_mem = rmi_bus_space;
	return (0);
}
