/*
 *  linux/fs/9p/vfs_addr.c
 *
 * This file contians vfs address (mmap) ops for 9P2000.
 *
 *  Copyright (C) 2005 by Eric Van Hensbergen <ericvh@gmail.com>
 *  Copyright (C) 2002 by Ron Minnich <rminnich@lanl.gov>
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

#include <linux/module.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/stat.h>
#include <linux/string.h>
#include <linux/inet.h>
#include <linux/pagemap.h>
#include <linux/idr.h>
#include <linux/sched.h>
#include "9p.h"
#include "client.h"

#include "v9fs.h"
#include "v9fs_vfs.h"

/**
 * v9fs_vfs_readpage - read an entire page in from 9P
 *
 * @filp: file being read
 * @page: structure to page
 *
 */

static int v9fs_vfs_readpage(struct file *filp, struct page *page)
{
	int retval;
	loff_t offset;
	char *buffer;

	P9_DPRINTK(P9_DEBUG_VFS, "\n");
	buffer = kmap(page);
	offset = page_offset(page);

	retval = v9fs_file_readn(filp, buffer, NULL, PAGE_CACHE_SIZE, offset);
	if (retval < 0)
		goto done;

	memset(buffer + retval, 0, PAGE_CACHE_SIZE - retval);
	flush_dcache_page(page);
	SetPageUptodate(page);
	retval = 0;

done:
	kunmap(page);
	unlock_page(page);
	return retval;
}

/**
 * v9fs_vfs_readpages - read a set of pages from 9P
 *
 * @filp: file being read
 * @mapping: the address space
 * @pages: list of pages to read
 * @nr_pages: count of pages to read
 *
 */

static int v9fs_vfs_readpages(struct file *filp, struct address_space *mapping,
			     struct list_head *pages, unsigned nr_pages)
{
	int ret = 0;

	P9_DPRINTK(P9_DEBUG_VFS, "inode: %p file: %p\n", mapping->host, filp);
	ret = read_cache_pages(mapping, pages, (void *)v9fs_vfs_readpage, filp);
	P9_DPRINTK(P9_DEBUG_VFS, "  = %d\n", ret);
	return ret;
}

const struct address_space_operations v9fs_addr_operations = {
      .readpage = v9fs_vfs_readpage,
      .readpages = v9fs_vfs_readpages,
};
