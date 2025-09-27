/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2002 Juli Mallett.  All rights reserved.
 *
 * This software was written by Juli Mallett <jmallett@FreeBSD.org> for the
 * FreeBSD project.  Redistribution and use in source and binary forms, with
 * or without modification, are permitted provided that the following
 * conditions are met:
 *
 * 1. Redistribution of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistribution in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <inttypes.h>


/*
 * A write function for use by user-level programs using sbput in libufs.
 */
static int
use_pwrite(void *devfd, uint64_t loc, void *buf, int size)
{
	int fd;

	fd = *(int *)devfd;
	if (pwrite(fd, buf, size, loc) != size)
		return (EIO);
	return (0);
}


/*
 * Unwinding superblock updates for old filesystems.
 * See ffs_oldfscompat_read above for details.
 *
 * XXX - Parts get retired eventually.
 * Unfortunately new bits get added.
 */
void
ffs_oldfscompat_write(struct fs *fs)
{

	switch (fs->fs_magic) {
	case FS_UFS1_MAGIC:
		if (fs->fs_sblockloc != SBLOCK_UFS1 &&
		    (fs->fs_old_flags & FS_FLAGS_UPDATED) == 0) {
			printf(
			"WARNING: %s: correcting fs_sblockloc from %li to %d\n",
			    fs->fs_fsmnt, fs->fs_sblockloc, SBLOCK_UFS1);
			fs->fs_sblockloc = SBLOCK_UFS1;
		}
		/*
		 * Copy back UFS2 updated fields that UFS1 inspects.
		 */
		fs->fs_old_time = fs->fs_time;
		fs->fs_old_cstotal.cs_ndir = fs->fs_cstotal.cs_ndir;
		fs->fs_old_cstotal.cs_nbfree = fs->fs_cstotal.cs_nbfree;
		fs->fs_old_cstotal.cs_nifree = fs->fs_cstotal.cs_nifree;
		fs->fs_old_cstotal.cs_nffree = fs->fs_cstotal.cs_nffree;
		if (fs->fs_save_maxfilesize != 0)
			fs->fs_maxfilesize = fs->fs_save_maxfilesize;
		break;
	case FS_UFS2_MAGIC:
		if (fs->fs_sblockloc != SBLOCK_UFS2 &&
		    (fs->fs_old_flags & FS_FLAGS_UPDATED) == 0) {
			printf(
			"WARNING: %s: correcting fs_sblockloc from %li to %d\n",
			    fs->fs_fsmnt, fs->fs_sblockloc, SBLOCK_UFS2);
			fs->fs_sblockloc = SBLOCK_UFS2;
		}
		break;
	}
}


/*
 * Calculate the check-hash for a superblock.
 */
uint32_t
ffs_calc_sbhash(struct fs *fs)
{
	uint32_t ckhash, save_ckhash;

	/*
	 * A filesystem that was using a superblock ckhash may be moved
	 * to an older kernel that does not support ckhashes. The
	 * older kernel will clear the FS_METACKHASH flag indicating
	 * that it does not update hashes. When the disk is moved back
	 * to a kernel capable of ckhashes it disables them on mount:
	 *
	 *	if ((fs->fs_flags & FS_METACKHASH) == 0)
	 *		fs->fs_metackhash = 0;
	 *
	 * This leaves (fs->fs_metackhash & CK_SUPERBLOCK) == 0) with an
	 * old stale value in the fs->fs_ckhash field. Thus the need to
	 * just accept what is there.
	 */
	if ((fs->fs_metackhash & CK_SUPERBLOCK) == 0)
		return (fs->fs_ckhash);

	save_ckhash = fs->fs_ckhash;
	fs->fs_ckhash = 0;
	/*
	 * If newly read from disk, the caller is responsible for
	 * verifying that fs->fs_sbsize <= SBLOCKSIZE.
	 */
	ckhash = calculate_crc32c(~0L, (unsigned char *)(void *)fs, fs->fs_sbsize);
	fs->fs_ckhash = save_ckhash;
	return (ckhash);
}

/*
 * Write a superblock to the devfd device from the memory pointed to by fs.
 * Write out the superblock summary information if it is present.
 *
 * If the write is successful, zero is returned. Otherwise one of the
 * following error values is returned:
 *     EIO: failed to write superblock.
 *     EIO: failed to write superblock summary information.
 */
int
ffs_sbput(void *devfd, struct fs *fs, uint64_t loc)
{

	struct fs_summary_info *fs_si;
	int i, error, blks, size;
	uint8_t *space;

	/*
	 * If there is summary information, write it first, so if there
	 * is an error, the superblock will not be marked as clean.
	 */
	if (fs->fs_si != NULL && fs->fs_csp != NULL) {

		blks = howmany(fs->fs_cssize, fs->fs_fsize);
		space = (uint8_t *)fs->fs_csp;
		for (i = 0; i < blks; i += fs->fs_frag) {

			size = fs->fs_bsize;
			if (i + fs->fs_frag > blks)
				size = (blks - i) * fs->fs_fsize;

			if ((error = use_pwrite(devfd,
			     (fsbtodb(fs, fs->fs_csaddr + i))/sectorsize,
			     space, size)) != 0)
				return (error);

			space += size;
		}
	}

	fs->fs_fmod = 0;
	ffs_oldfscompat_write(fs);
#ifdef _KERNEL
	fs->fs_time = time_second;
#else /* User Code */
	fs->fs_time = time(NULL);
#endif
	/* Clear the pointers for the duration of writing. */
	fs_si = fs->fs_si;
	fs->fs_si = NULL;
	fs->fs_ckhash = ffs_calc_sbhash(fs);
	error = use_pwrite(devfd, loc, fs, fs->fs_sbsize);
	/*
	 * A negative error code is returned when a copy of the
	 * superblock has been made which is discarded when the I/O
	 * is done. So the fs_si field does not and indeed cannot be
	 * restored after the write is done. Convert the error code
	 * back to its usual positive value when returning it.
	 */
	if (error < 0)
		return (-error - 1);
	fs->fs_si = fs_si;
	return (error);
}



/*
 * Write a superblock to the devfd device from the memory pointed to by fs.
 * Also write out the superblock summary information but do not free the
 * summary information memory.
 *
 * Additionally write out numaltwrite of the alternate superblocks. Use
 * fs->fs_ncg to write out all of the alternate superblocks.
 */
int
sbput(int devfd, struct fs *fs, int numaltwrite)
{
	struct csum *savedcsp;
	uint64_t savedactualloc;
	int i, error;


	error = ffs_sbput(&devfd, fs, fs->fs_sblockactualloc);

	fflush(NULL); /* flush any messages */
	if (error != 0 || numaltwrite == 0)
		return (error);
	savedactualloc = fs->fs_sblockactualloc;
	if (fs->fs_si != NULL) {
		savedcsp = fs->fs_csp;
		fs->fs_csp = NULL;
	}
	for (i = 0; i < numaltwrite; i++) {
		fs->fs_sblockactualloc = (fsbtodb(fs, cgsblock(fs, i)))/sectorsize;
		if ((error = ffs_sbput(&devfd, fs, fs->fs_sblockactualloc
		     )) != 0) {
			fflush(NULL); /* flush any messages */
			fs->fs_sblockactualloc = savedactualloc;
			fs->fs_csp = savedcsp;
			return (error);
		}
	}
	fs->fs_sblockactualloc = savedactualloc;
	if (fs->fs_si != NULL)
		fs->fs_csp = savedcsp;
	fflush(NULL); /* flush any messages */
	return (0);
}


int
sbwrite(int all)
{
	d_err = NULL;


	if ((errno = sbput(d_fd, &sblock, all ? sblock.fs_ncg : 0)) != 0) {
		switch (errno) {
		case EIO:
			d_err = "failed to write superblock";
			break;
		default:
			d_err = "unknown superblock write error";
			errno = EIO;
			break;
		}
		return (-1);
	}
	return (0);
}


ssize_t
bwrite(ufs2_daddr_t blockno, const void *data, size_t size)
{
	ssize_t cnt;
	void *p2;

	d_err = NULL;


	BUF_MALLOC(&p2, data, size);
	if (p2 == NULL) {
		d_err = "allocate bounce buffer";
		return (-1);
	}
	if (p2 != data)
		memcpy(p2, data, size);
	cnt = pwrite(d_fd, p2, size, (off_t)(blockno * sectorsize));
	if (p2 != data)
		free(p2);
	if (cnt == -1) {
		d_err = "write error to block device";
		return (-1);
	}
	if ((size_t)cnt != size) {
		d_err = "short write to block device";
		return (-1);
	}
	return (cnt);
}


/*
 * possibly write to disk
 */
static void
wtfs(ufs2_daddr_t bno, int size, char *bf)
{
	if (Nflag)
		return;
	if (bwrite(part_ofs + bno, bf, size) < 0)
		err(36, "wtfs: %d bytes at sector %jd", size, (intmax_t)bno);
}


ssize_t
bread(uint64_t blockno, void *data, size_t size)
{
	void *p2;
	ssize_t cnt;



	BUF_MALLOC(&p2, data, size);
	if (p2 == NULL) {
		d_err = "allocate bounce buffer";
		goto fail;
	}
	cnt = pread(d_fd, p2, size, (off_t)(blockno * sectorsize));
	if (cnt == -1) {
		d_err = "read error from block device";
		goto fail;
	}
	if (cnt == 0) {
		d_err = "end of file from block device";
		goto fail;
	}
	if ((size_t)cnt != size) {
		d_err = "short read or read error from block device";
		goto fail;
	}
	if (p2 != data) {
		memcpy(data, p2, size);
		free(p2);
	}
	return (cnt);
fail:	memset(data, 0, size);
	if (p2 != data) {
		free(p2);
	}
	return (-1);
}
