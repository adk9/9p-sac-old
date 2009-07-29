/*
 * include/net/9p/transport.h
 *
 * Transport Definition
 *
 *  Copyright (C) 2005 by Latchesar Ionkov <lucho@ionkov.net>
 *  Copyright (C) 2004-2008 by Eric Van Hensbergen <ericvh@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2
 *  as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to:
 *  Free Software Foundation
 *  51 Franklin Street, Fifth Floor
 *  Boston, MA  02111-1301  USA
 *
 */

#ifndef NET_9P_TRANSPORT_H
#define NET_9P_TRANSPORT_H

/**
 * struct p9_trans_opts - transport-specific mount options
 *
 * @addr: endpoint address the transport should connect to
 * @port: port to connect to (tcp/rdma)
 * @rfd: file descriptor for reading (fd)
 * @wfd: file descriptor for writing (fd)
 * @sq_depth: The requested depth of the SQ. This really doesn't need
 * to be any deeper than the number of threads used in the client (rdma)
 * @rq_depth: The depth of the RQ. Should be greater than or equal to SQ depth
 * @timeout: Time to wait in msecs for CM events (rdma)
 *
 */

struct p9_trans_opts {
	/* common transport options */
	char *addr;
	u16 port;
	/* fd transport options */
	int rfd;
	int wfd;
	/* rdma transport options */
	int sq_depth;
	int rq_depth;
	int timeout;
};

/**
 * struct p9_trans_module - transport module interface
 * @list: used to maintain a list of currently available transports
 * @name: the human-readable name of the transport
 * @maxsize: transport provided maximum packet size
 * @def: set if this transport should be considered the default
 * @create: member function to create a new connection on this transport
 * @request: member function to issue a request to the transport
 * @cancel: member function to cancel a request (if it hasn't been sent)
 *
 * This is the basic API for a transport module which is registered by the
 * transport module with the 9P core network module and used by the client
 * to instantiate a new connection on a transport.
 *
 * BUGS: the transport module list isn't protected.
 */

struct p9_trans_module {
	struct list_head list;
	char *name;		/* name of transport */
	int maxsize;		/* max message size of transport */
	int def;		/* this transport should be default */
	struct module *owner;
	int (*create)(struct p9_client *, struct p9_trans_opts *);
	void (*close) (struct p9_client *);
	int (*request) (struct p9_client *, struct p9_req_t *req);
	int (*cancel) (struct p9_client *, struct p9_req_t *req);
};

void v9fs_register_trans(struct p9_trans_module *m);
void v9fs_unregister_trans(struct p9_trans_module *m);
struct p9_trans_module *v9fs_get_trans_by_name(const substring_t *name);
struct p9_trans_module *v9fs_get_default_trans(void);
void v9fs_put_trans(struct p9_trans_module *m);
#endif /* NET_9P_TRANSPORT_H */
