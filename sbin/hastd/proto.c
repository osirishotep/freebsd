/*-
 * Copyright (c) 2009-2010 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Pawel Jakub Dawidek under sponsorship from
 * the FreeBSD Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/socket.h>

#include <assert.h>
#include <errno.h>
#include <stdint.h>

#include "proto.h"
#include "proto_impl.h"

#define	PROTO_CONN_MAGIC	0x907041c
struct proto_conn {
	int			 pc_magic;
	struct hast_proto	*pc_proto;
	void			*pc_ctx;
	int			 pc_side;
#define	PROTO_SIDE_CLIENT		0
#define	PROTO_SIDE_SERVER_LISTEN	1
#define	PROTO_SIDE_SERVER_WORK		2
};

static TAILQ_HEAD(, hast_proto) protos = TAILQ_HEAD_INITIALIZER(protos);

void
proto_register(struct hast_proto *proto, bool isdefault)
{
	static bool seen_default = false;

	if (!isdefault)
		TAILQ_INSERT_HEAD(&protos, proto, hp_next);
	else {
		assert(!seen_default);
		seen_default = true;
		TAILQ_INSERT_TAIL(&protos, proto, hp_next);
	}
}

static int
proto_common_setup(const char *addr, struct proto_conn **connp, int side)
{
	struct hast_proto *proto;
	struct proto_conn *conn;
	void *ctx;
	int ret;

	assert(side == PROTO_SIDE_CLIENT || side == PROTO_SIDE_SERVER_LISTEN);

	conn = malloc(sizeof(*conn));
	if (conn == NULL)
		return (-1);

	TAILQ_FOREACH(proto, &protos, hp_next) {
		if (side == PROTO_SIDE_CLIENT)
			ret = proto->hp_client(addr, &ctx);
		else /* if (side == PROTO_SIDE_SERVER_LISTEN) */
			ret = proto->hp_server(addr, &ctx);
		/*
		 * ret == 0  - success
		 * ret == -1 - addr is not for this protocol
		 * ret > 0   - right protocol, but an error occured
		 */
		if (ret >= 0)
			break;
	}
	if (proto == NULL) {
		/* Unrecognized address. */
		free(conn);
		errno = EINVAL;
		return (-1);
	}
	if (ret > 0) {
		/* An error occured. */
		free(conn);
		errno = ret;
		return (-1);
	}
	conn->pc_proto = proto;
	conn->pc_ctx = ctx;
	conn->pc_side = side;
	conn->pc_magic = PROTO_CONN_MAGIC;
	*connp = conn;
	return (0);
}

int
proto_client(const char *addr, struct proto_conn **connp)
{

	return (proto_common_setup(addr, connp, PROTO_SIDE_CLIENT));
}

int
proto_connect(struct proto_conn *conn)
{
	int ret;

	assert(conn != NULL);
	assert(conn->pc_magic == PROTO_CONN_MAGIC);
	assert(conn->pc_side == PROTO_SIDE_CLIENT);
	assert(conn->pc_proto != NULL);

	ret = conn->pc_proto->hp_connect(conn->pc_ctx);
	if (ret != 0) {
		errno = ret;
		return (-1);
	}

	return (0);
}

int
proto_server(const char *addr, struct proto_conn **connp)
{

	return (proto_common_setup(addr, connp, PROTO_SIDE_SERVER_LISTEN));
}

int
proto_accept(struct proto_conn *conn, struct proto_conn **newconnp)
{
	struct proto_conn *newconn;
	int ret;

	assert(conn != NULL);
	assert(conn->pc_magic == PROTO_CONN_MAGIC);
	assert(conn->pc_side == PROTO_SIDE_SERVER_LISTEN);
	assert(conn->pc_proto != NULL);

	newconn = malloc(sizeof(*newconn));
	if (newconn == NULL)
		return (-1);

	ret = conn->pc_proto->hp_accept(conn->pc_ctx, &newconn->pc_ctx);
	if (ret != 0) {
		free(newconn);
		errno = ret;
		return (-1);
	}

	newconn->pc_proto = conn->pc_proto;
	newconn->pc_side = PROTO_SIDE_SERVER_WORK;
	newconn->pc_magic = PROTO_CONN_MAGIC;
	*newconnp = newconn;

	return (0);
}

int
proto_send(const struct proto_conn *conn, const void *data, size_t size)
{
	int ret;

	assert(conn != NULL);
	assert(conn->pc_magic == PROTO_CONN_MAGIC);
	assert(conn->pc_proto != NULL);

	ret = conn->pc_proto->hp_send(conn->pc_ctx, data, size);
	if (ret != 0) {
		errno = ret;
		return (-1);
	}
	return (0);
}

int
proto_recv(const struct proto_conn *conn, void *data, size_t size)
{
	int ret;

	assert(conn != NULL);
	assert(conn->pc_magic == PROTO_CONN_MAGIC);
	assert(conn->pc_proto != NULL);

	ret = conn->pc_proto->hp_recv(conn->pc_ctx, data, size);
	if (ret != 0) {
		errno = ret;
		return (-1);
	}
	return (0);
}

int
proto_descriptor(const struct proto_conn *conn)
{

	assert(conn != NULL);
	assert(conn->pc_magic == PROTO_CONN_MAGIC);
	assert(conn->pc_proto != NULL);

	return (conn->pc_proto->hp_descriptor(conn->pc_ctx));
}

bool
proto_address_match(const struct proto_conn *conn, const char *addr)
{

	assert(conn != NULL);
	assert(conn->pc_magic == PROTO_CONN_MAGIC);
	assert(conn->pc_proto != NULL);

	return (conn->pc_proto->hp_address_match(conn->pc_ctx, addr));
}

void
proto_local_address(const struct proto_conn *conn, char *addr, size_t size)
{

	assert(conn != NULL);
	assert(conn->pc_magic == PROTO_CONN_MAGIC);
	assert(conn->pc_proto != NULL);

	conn->pc_proto->hp_local_address(conn->pc_ctx, addr, size);
}

void
proto_remote_address(const struct proto_conn *conn, char *addr, size_t size)
{

	assert(conn != NULL);
	assert(conn->pc_magic == PROTO_CONN_MAGIC);
	assert(conn->pc_proto != NULL);

	conn->pc_proto->hp_remote_address(conn->pc_ctx, addr, size);
}

int
proto_timeout(const struct proto_conn *conn, int timeout)
{
	struct timeval tv;
	int fd;

	assert(conn != NULL);
	assert(conn->pc_magic == PROTO_CONN_MAGIC);
	assert(conn->pc_proto != NULL);

	fd = proto_descriptor(conn);
	if (fd < 0)
		return (-1);

	tv.tv_sec = timeout;
	tv.tv_usec = 0;
	if (setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0)
		return (-1);
	if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0)
		return (-1);

	return (0);
}

void
proto_close(struct proto_conn *conn)
{

	assert(conn != NULL);
	assert(conn->pc_magic == PROTO_CONN_MAGIC);
	assert(conn->pc_proto != NULL);

	conn->pc_proto->hp_close(conn->pc_ctx);
	conn->pc_magic = 0;
	free(conn);
}
