/*
 * This file is part of UBIFS.
 *
 * Copyright (C) 2006-2008 Nokia Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 51
 * Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * Authors: Artem Bityutskiy (Битюцкий Артём)
 *          Adrian Hunter
 */


#include "ubifs.h"

#ifdef CONFIG_UBIFS_FS_DEBUG
static int dbg_check_bud_bytes(struct ubifs_info *c);
#else
#define dbg_check_bud_bytes(c) 0
#endif

struct ubifs_bud *ubifs_search_bud(struct ubifs_info *c, int lnum)
{
	struct rb_node *p;
	struct ubifs_bud *bud;

	spin_lock(&c->buds_lock);
	p = c->buds.rb_node;
	while (p) {
		bud = rb_entry(p, struct ubifs_bud, rb);
		if (lnum < bud->lnum)
			p = p->rb_left;
		else if (lnum > bud->lnum)
			p = p->rb_right;
		else {
			spin_unlock(&c->buds_lock);
			return bud;
		}
	}
	spin_unlock(&c->buds_lock);
	return NULL;
}

struct ubifs_wbuf *ubifs_get_wbuf(struct ubifs_info *c, int lnum)
{
	struct rb_node *p;
	struct ubifs_bud *bud;
	int jhead;

	if (!c->jheads)
		return NULL;

	spin_lock(&c->buds_lock);
	p = c->buds.rb_node;
	while (p) {
		bud = rb_entry(p, struct ubifs_bud, rb);
		if (lnum < bud->lnum)
			p = p->rb_left;
		else if (lnum > bud->lnum)
			p = p->rb_right;
		else {
			jhead = bud->jhead;
			spin_unlock(&c->buds_lock);
			return &c->jheads[jhead].wbuf;
		}
	}
	spin_unlock(&c->buds_lock);
	return NULL;
}

static inline long long empty_log_bytes(const struct ubifs_info *c)
{
	long long h, t;

	h = (long long)c->lhead_lnum * c->leb_size + c->lhead_offs;
	t = (long long)c->ltail_lnum * c->leb_size;

	if (h >= t)
		return c->log_bytes - h + t;
	else
		return t - h;
}

void ubifs_add_bud(struct ubifs_info *c, struct ubifs_bud *bud)
{
	struct rb_node **p, *parent = NULL;
	struct ubifs_bud *b;
	struct ubifs_jhead *jhead;

	spin_lock(&c->buds_lock);
	p = &c->buds.rb_node;
	while (*p) {
		parent = *p;
		b = rb_entry(parent, struct ubifs_bud, rb);
		ubifs_assert(bud->lnum != b->lnum);
		if (bud->lnum < b->lnum)
			p = &(*p)->rb_left;
		else
			p = &(*p)->rb_right;
	}

	rb_link_node(&bud->rb, parent, p);
	rb_insert_color(&bud->rb, &c->buds);
	if (c->jheads) {
		jhead = &c->jheads[bud->jhead];
		list_add_tail(&bud->list, &jhead->buds_list);
	} else
		ubifs_assert(c->replaying && c->ro_mount);

	/*
	 * Note, although this is a new bud, we anyway account this space now,
	 * before any data has been written to it, because this is about to
	 * guarantee fixed mount time, and this bud will anyway be read and
	 * scanned.
	 */
	c->bud_bytes += c->leb_size - bud->start;

	dbg_log("LEB %d:%d, jhead %s, bud_bytes %lld", bud->lnum,
		bud->start, dbg_jhead(bud->jhead), c->bud_bytes);
	spin_unlock(&c->buds_lock);
}

int ubifs_add_bud_to_log(struct ubifs_info *c, int jhead, int lnum, int offs)
{
	int err;
	struct ubifs_bud *bud;
	struct ubifs_ref_node *ref;

	bud = kmalloc(sizeof(struct ubifs_bud), GFP_NOFS);
	if (!bud)
		return -ENOMEM;
	ref = kzalloc(c->ref_node_alsz, GFP_NOFS);
	if (!ref) {
		kfree(bud);
		return -ENOMEM;
	}

	mutex_lock(&c->log_mutex);
	ubifs_assert(!c->ro_media && !c->ro_mount);
	if (c->ro_error) {
		err = -EROFS;
		goto out_unlock;
	}

	
	if (empty_log_bytes(c) - c->ref_node_alsz < c->min_log_bytes) {
		dbg_log("not enough log space - %lld, required %d",
			empty_log_bytes(c), c->min_log_bytes);
		ubifs_commit_required(c);
		err = -EAGAIN;
		goto out_unlock;
	}

	if (c->bud_bytes + c->leb_size - offs > c->max_bud_bytes) {
		dbg_log("bud bytes %lld (%lld max), require commit",
			c->bud_bytes, c->max_bud_bytes);
		ubifs_commit_required(c);
		err = -EAGAIN;
		goto out_unlock;
	}

	if (c->bud_bytes >= c->bg_bud_bytes &&
	    c->cmt_state == COMMIT_RESTING) {
		dbg_log("bud bytes %lld (%lld max), initiate BG commit",
			c->bud_bytes, c->max_bud_bytes);
		ubifs_request_bg_commit(c);
	}

	bud->lnum = lnum;
	bud->start = offs;
	bud->jhead = jhead;

	ref->ch.node_type = UBIFS_REF_NODE;
	ref->lnum = cpu_to_le32(bud->lnum);
	ref->offs = cpu_to_le32(bud->start);
	ref->jhead = cpu_to_le32(jhead);

	if (c->lhead_offs > c->leb_size - c->ref_node_alsz) {
		c->lhead_lnum = ubifs_next_log_lnum(c, c->lhead_lnum);
		c->lhead_offs = 0;
	}

	if (c->lhead_offs == 0) {
		
		err = ubifs_leb_unmap(c, c->lhead_lnum);
		if (err)
			goto out_unlock;
	}

	if (bud->start == 0) {
		err = ubifs_leb_map(c, bud->lnum, UBI_SHORTTERM);
		if (err)
			goto out_unlock;
	}

	dbg_log("write ref LEB %d:%d",
		c->lhead_lnum, c->lhead_offs);
	err = ubifs_write_node(c, ref, UBIFS_REF_NODE_SZ, c->lhead_lnum,
			       c->lhead_offs, UBI_SHORTTERM);
	if (err)
		goto out_unlock;

	c->lhead_offs += c->ref_node_alsz;

	ubifs_add_bud(c, bud);

	mutex_unlock(&c->log_mutex);
	kfree(ref);
	return 0;

out_unlock:
	mutex_unlock(&c->log_mutex);
	kfree(ref);
	kfree(bud);
	return err;
}

static void remove_buds(struct ubifs_info *c)
{
	struct rb_node *p;

	ubifs_assert(list_empty(&c->old_buds));
	c->cmt_bud_bytes = 0;
	spin_lock(&c->buds_lock);
	p = rb_first(&c->buds);
	while (p) {
		struct rb_node *p1 = p;
		struct ubifs_bud *bud;
		struct ubifs_wbuf *wbuf;

		p = rb_next(p);
		bud = rb_entry(p1, struct ubifs_bud, rb);
		wbuf = &c->jheads[bud->jhead].wbuf;

		if (wbuf->lnum == bud->lnum) {
			c->cmt_bud_bytes += wbuf->offs - bud->start;
			dbg_log("preserve %d:%d, jhead %s, bud bytes %d, "
				"cmt_bud_bytes %lld", bud->lnum, bud->start,
				dbg_jhead(bud->jhead), wbuf->offs - bud->start,
				c->cmt_bud_bytes);
			bud->start = wbuf->offs;
		} else {
			c->cmt_bud_bytes += c->leb_size - bud->start;
			dbg_log("remove %d:%d, jhead %s, bud bytes %d, "
				"cmt_bud_bytes %lld", bud->lnum, bud->start,
				dbg_jhead(bud->jhead), c->leb_size - bud->start,
				c->cmt_bud_bytes);
			rb_erase(p1, &c->buds);
			list_move(&bud->list, &c->old_buds);
		}
	}
	spin_unlock(&c->buds_lock);
}

/**
 * ubifs_log_start_commit - start commit.
 * @c: UBIFS file-system description object
 * @ltail_lnum: return new log tail LEB number
 *
 * The commit operation starts with writing "commit start" node to the log and
 * reference nodes for all journal heads which will define new journal after
 * the commit has been finished. The commit start and reference nodes are
 * written in one go to the nearest empty log LEB (hence, when commit is
 * finished UBIFS may safely unmap all the previous log LEBs). This function
 * returns zero in case of success and a negative error code in case of
 * failure.
 */
int ubifs_log_start_commit(struct ubifs_info *c, int *ltail_lnum)
{
	void *buf;
	struct ubifs_cs_node *cs;
	struct ubifs_ref_node *ref;
	int err, i, max_len, len;

	err = dbg_check_bud_bytes(c);
	if (err)
		return err;

	max_len = UBIFS_CS_NODE_SZ + c->jhead_cnt * UBIFS_REF_NODE_SZ;
	max_len = ALIGN(max_len, c->min_io_size);
	buf = cs = kmalloc(max_len, GFP_NOFS);
	if (!buf)
		return -ENOMEM;

	cs->ch.node_type = UBIFS_CS_NODE;
	cs->cmt_no = cpu_to_le64(c->cmt_no);
	ubifs_prepare_node(c, cs, UBIFS_CS_NODE_SZ, 0);


	len = UBIFS_CS_NODE_SZ;
	for (i = 0; i < c->jhead_cnt; i++) {
		int lnum = c->jheads[i].wbuf.lnum;
		int offs = c->jheads[i].wbuf.offs;

		if (lnum == -1 || offs == c->leb_size)
			continue;

		dbg_log("add ref to LEB %d:%d for jhead %s",
			lnum, offs, dbg_jhead(i));
		ref = buf + len;
		ref->ch.node_type = UBIFS_REF_NODE;
		ref->lnum = cpu_to_le32(lnum);
		ref->offs = cpu_to_le32(offs);
		ref->jhead = cpu_to_le32(i);

		ubifs_prepare_node(c, ref, UBIFS_REF_NODE_SZ, 0);
		len += UBIFS_REF_NODE_SZ;
	}

	ubifs_pad(c, buf + len, ALIGN(len, c->min_io_size) - len);

	
	if (c->lhead_offs) {
		c->lhead_lnum = ubifs_next_log_lnum(c, c->lhead_lnum);
		c->lhead_offs = 0;
	}

	if (c->lhead_offs == 0) {
		
		err = ubifs_leb_unmap(c, c->lhead_lnum);
		if (err)
			goto out;
	}

	len = ALIGN(len, c->min_io_size);
	dbg_log("writing commit start at LEB %d:0, len %d", c->lhead_lnum, len);
	err = ubifs_leb_write(c, c->lhead_lnum, cs, 0, len, UBI_SHORTTERM);
	if (err)
		goto out;

	*ltail_lnum = c->lhead_lnum;

	c->lhead_offs += len;
	if (c->lhead_offs == c->leb_size) {
		c->lhead_lnum = ubifs_next_log_lnum(c, c->lhead_lnum);
		c->lhead_offs = 0;
	}

	remove_buds(c);

	c->min_log_bytes = 0;

out:
	kfree(buf);
	return err;
}

int ubifs_log_end_commit(struct ubifs_info *c, int ltail_lnum)
{
	int err;

	mutex_lock(&c->log_mutex);

	dbg_log("old tail was LEB %d:0, new tail is LEB %d:0",
		c->ltail_lnum, ltail_lnum);

	c->ltail_lnum = ltail_lnum;
	c->min_log_bytes = c->leb_size;

	spin_lock(&c->buds_lock);
	c->bud_bytes -= c->cmt_bud_bytes;
	spin_unlock(&c->buds_lock);

	err = dbg_check_bud_bytes(c);

	mutex_unlock(&c->log_mutex);
	return err;
}

int ubifs_log_post_commit(struct ubifs_info *c, int old_ltail_lnum)
{
	int lnum, err = 0;

	while (!list_empty(&c->old_buds)) {
		struct ubifs_bud *bud;

		bud = list_entry(c->old_buds.next, struct ubifs_bud, list);
		err = ubifs_return_leb(c, bud->lnum);
		if (err)
			return err;
		list_del(&bud->list);
		kfree(bud);
	}
	mutex_lock(&c->log_mutex);
	for (lnum = old_ltail_lnum; lnum != c->ltail_lnum;
	     lnum = ubifs_next_log_lnum(c, lnum)) {
		dbg_log("unmap log LEB %d", lnum);
		err = ubifs_leb_unmap(c, lnum);
		if (err)
			goto out;
	}
out:
	mutex_unlock(&c->log_mutex);
	return err;
}

struct done_ref {
	struct rb_node rb;
	int lnum;
};

static int done_already(struct rb_root *done_tree, int lnum)
{
	struct rb_node **p = &done_tree->rb_node, *parent = NULL;
	struct done_ref *dr;

	while (*p) {
		parent = *p;
		dr = rb_entry(parent, struct done_ref, rb);
		if (lnum < dr->lnum)
			p = &(*p)->rb_left;
		else if (lnum > dr->lnum)
			p = &(*p)->rb_right;
		else
			return 1;
	}

	dr = kzalloc(sizeof(struct done_ref), GFP_NOFS);
	if (!dr)
		return -ENOMEM;

	dr->lnum = lnum;

	rb_link_node(&dr->rb, parent, p);
	rb_insert_color(&dr->rb, done_tree);

	return 0;
}

static void destroy_done_tree(struct rb_root *done_tree)
{
	struct rb_node *this = done_tree->rb_node;
	struct done_ref *dr;

	while (this) {
		if (this->rb_left) {
			this = this->rb_left;
			continue;
		} else if (this->rb_right) {
			this = this->rb_right;
			continue;
		}
		dr = rb_entry(this, struct done_ref, rb);
		this = rb_parent(this);
		if (this) {
			if (this->rb_left == &dr->rb)
				this->rb_left = NULL;
			else
				this->rb_right = NULL;
		}
		kfree(dr);
	}
}

static int add_node(struct ubifs_info *c, void *buf, int *lnum, int *offs,
		    void *node)
{
	struct ubifs_ch *ch = node;
	int len = le32_to_cpu(ch->len), remains = c->leb_size - *offs;

	if (len > remains) {
		int sz = ALIGN(*offs, c->min_io_size), err;

		ubifs_pad(c, buf + *offs, sz - *offs);
		err = ubifs_leb_change(c, *lnum, buf, sz, UBI_SHORTTERM);
		if (err)
			return err;
		*lnum = ubifs_next_log_lnum(c, *lnum);
		*offs = 0;
	}
	memcpy(buf + *offs, node, len);
	*offs += ALIGN(len, 8);
	return 0;
}

int ubifs_consolidate_log(struct ubifs_info *c)
{
	struct ubifs_scan_leb *sleb;
	struct ubifs_scan_node *snod;
	struct rb_root done_tree = RB_ROOT;
	int lnum, err, first = 1, write_lnum, offs = 0;
	void *buf;

	dbg_rcvry("log tail LEB %d, log head LEB %d", c->ltail_lnum,
		  c->lhead_lnum);
	buf = vmalloc(c->leb_size);
	if (!buf)
		return -ENOMEM;
	lnum = c->ltail_lnum;
	write_lnum = lnum;
	while (1) {
		sleb = ubifs_scan(c, lnum, 0, c->sbuf, 0);
		if (IS_ERR(sleb)) {
			err = PTR_ERR(sleb);
			goto out_free;
		}
		list_for_each_entry(snod, &sleb->nodes, list) {
			switch (snod->type) {
			case UBIFS_REF_NODE: {
				struct ubifs_ref_node *ref = snod->node;
				int ref_lnum = le32_to_cpu(ref->lnum);

				err = done_already(&done_tree, ref_lnum);
				if (err < 0)
					goto out_scan;
				if (err != 1) {
					err = add_node(c, buf, &write_lnum,
						       &offs, snod->node);
					if (err)
						goto out_scan;
				}
				break;
			}
			case UBIFS_CS_NODE:
				if (!first)
					break;
				err = add_node(c, buf, &write_lnum, &offs,
					       snod->node);
				if (err)
					goto out_scan;
				first = 0;
				break;
			}
		}
		ubifs_scan_destroy(sleb);
		if (lnum == c->lhead_lnum)
			break;
		lnum = ubifs_next_log_lnum(c, lnum);
	}
	if (offs) {
		int sz = ALIGN(offs, c->min_io_size);

		ubifs_pad(c, buf + offs, sz - offs);
		err = ubifs_leb_change(c, write_lnum, buf, sz, UBI_SHORTTERM);
		if (err)
			goto out_free;
		offs = ALIGN(offs, c->min_io_size);
	}
	destroy_done_tree(&done_tree);
	vfree(buf);
	if (write_lnum == c->lhead_lnum) {
		ubifs_err("log is too full");
		return -EINVAL;
	}
	
	lnum = write_lnum;
	do {
		lnum = ubifs_next_log_lnum(c, lnum);
		err = ubifs_leb_unmap(c, lnum);
		if (err)
			return err;
	} while (lnum != c->lhead_lnum);
	c->lhead_lnum = write_lnum;
	c->lhead_offs = offs;
	dbg_rcvry("new log head at %d:%d", c->lhead_lnum, c->lhead_offs);
	return 0;

out_scan:
	ubifs_scan_destroy(sleb);
out_free:
	destroy_done_tree(&done_tree);
	vfree(buf);
	return err;
}

#ifdef CONFIG_UBIFS_FS_DEBUG

static int dbg_check_bud_bytes(struct ubifs_info *c)
{
	int i, err = 0;
	struct ubifs_bud *bud;
	long long bud_bytes = 0;

	if (!dbg_is_chk_gen(c))
		return 0;

	spin_lock(&c->buds_lock);
	for (i = 0; i < c->jhead_cnt; i++)
		list_for_each_entry(bud, &c->jheads[i].buds_list, list)
			bud_bytes += c->leb_size - bud->start;

	if (c->bud_bytes != bud_bytes) {
		ubifs_err("bad bud_bytes %lld, calculated %lld",
			  c->bud_bytes, bud_bytes);
		err = -EINVAL;
	}
	spin_unlock(&c->buds_lock);

	return err;
}

#endif 