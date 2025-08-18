# SPDX-License-Identifier: GPL-2.0-only

# Add lru_cache [https://elrepo.org/bugs/view.php?id=1075#c7455]
drbd-y := lru_cache.o
drbd-y += drbd_buildtag.o drbd_bitmap.o drbd_proc.o
drbd-y += drbd_worker.o drbd_receiver.o drbd_req.o drbd_actlog.o
drbd-y += drbd_main.o drbd_strings.o drbd_nl.o
drbd-y += drbd_interval.o drbd_state.o
drbd-y += drbd_nla.o
drbd-$(CONFIG_DEBUG_FS) += drbd_debugfs.o

obj-$(CONFIG_BLK_DEV_DRBD)     += drbd.o
