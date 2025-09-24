#include <stdint.h>
#include <stddef.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <err.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
//#include <sys/vfs.h>
#include <sys/param.h>
#include <sys/mount.h>
//#include <linux/limits.h>
#include <sys/param.h>
#include <fstab.h>  
#include <sys/ioctl.h>
//#include <sys/disk.h>
//#include <linux/fs.h>
#include <sys/errno.h>
#include <time.h>
//#include <linux/ioctl.h>

#ifdef __ILP32__
#define MAXpHYS (128*1024)
#else
#define MAXPHYS (1024*1024)
#endif
#define DEV_BSHIFT 9
#define	P_OSREL_CK_CYLGRP		1200046
#define	P_OSREL_CK_SUPERBLOCK		1300000
#define	P_OSREL_CK_INODE		1300005

#define MAXBSIZE 65536
#define BBSIZE			8192
//#define MAXPATHLEN		4096
#define AVFILESIZ		16384
#define AFPDIR			64
#define	MAXBLKSPERCG	0x7fffffff
/*
 * MINFREE gives the minimum acceptable percentage of filesystem
 * blocks which may be free. If the freelist drops below this level
 * only the superuser may continue to allocate blocks. This may
 * be set to 0 if no reserve of free blocks is deemed necessary,
 * however throughput drops by fifty percent if the filesystem
 * is run at between 95% and 100% full; thus the minimum default
 * value of fs_minfree is 5%. However, to get good clustering
 * performance, 10% is a better choice. hence we use 10% as our
 * default value. With 10% free space, fragmentation is not a
 * problem, so we choose to optimize for time.
 */
#define	MINFREE		8
#define	DEFAULTOPT	FS_OPTTIME
/*
 * Preference for optimization.
 */
#define	FS_OPTTIME 	0	/* minimize allocation time */
#define	FS_OPTSPACE 1	/* minimize disk fragmentation */
/*
 * The volume name for this filesystem is maintained in fs_volname.
 * MAXVOLLEN defines the length of the buffer allocated.
 */
#define	MAXVOLLEN 32
/*
 * MINBSIZE is the smallest allowable block size.
 * In order to insure that it is possible to create files of size
 * 2^32 with only two levels of indirection, MINBSIZE is set to 4096.
 * MINBSIZE must be big enough to hold a cylinder group block,
 * thus changes to (struct cg) must keep its size within MINBSIZE.
 * Note that super blocks are always of size SBLOCKSIZE,
 * and that both SBLOCKSIZE and MAXBSIZE must be >= MINBSIZE.
 */
#define	MINBSIZE 	4096
//#define MAXBSIZE 	65536
#define _PATH_DEV 	"/dev/"
#define	FSMAXSNAP 	20
#define	NOCSPTRS	((128 / sizeof(void *)) - 1)
#define	MAXMNTLEN	468
#define	SBLOCKSIZE	8192
#define	MAXFRAG 	8
#define	MINE_WRITE	0x02
#define LIBUFS_BUFALIGN	128
#define	UFS_STDSB	-1	/* Search standard places for superblock */
#define	MINE_NAME	0x01

#define	IOCPARM_SHIFT	13		/* number of bits for ioctl size */
//#define	IOCPARM_MASK	((1 << IOCPARM_SHIFT) - 1) /* parameter length mask */
#define MAX(a,b) (((a)>(b))?(a):(b))
#define MIN(a,b) (((a)<(b))?(a):(b))
#define	DIOCGSECTORSIZE	_IOR('d', 128, u_int)
#define	DIOCGMEDIASIZE	_IOR('d', 129, off_t)	/* Get media size in bytes */
//#define	_IOC(inout,group,num,len)	((unsigned long) \
//	((inout) | (((len) & IOCPARM_MASK) << 16) | ((group) << 8) | (num)))
#define MAXPARTITIONS	8 /* XXX should be 20, but see PR276517 */

/*
 * The size of physical and logical block numbers and time fields in UFS.
 */
typedef	int32_t	ufs1_daddr_t;
typedef	int64_t	ufs2_daddr_t;
typedef int64_t ufs_lbn_t;
typedef int64_t ufs_time_t;


/*
 * The following two constants set the default block and fragment sizes.
 * Both constants must be a power of 2 and meet the following constraints:
 *	MINBSIZE <= DESBLKSIZE <= MAXBSIZE
 *	sectorsize <= DESFRAGSIZE <= DESBLKSIZE
 *	DESBLKSIZE / DESFRAGSIZE <= 8
 */
#define	DFL_FRAGSIZE	4096
#define	DFL_BLKSIZE	32768

/*
 * Cylinder groups may have up to MAXBLKSPERCG blocks. The actual
 * number used depends upon how much information can be stored
 * in a cylinder group map which must fit in a single file system
 * block. The default is to use as many as possible blocks per group.
 */
#define	MAXBLKSPERCG	0x7fffffff	/* desired fs_fpg ("infinity") */

/*
 * MAXBLKPG determines the maximum number of data blocks which are
 * placed in a single cylinder group. The default is one indirect
 * block worth of data blocks.
 */
#define MAXBLKPG(bsize)	((bsize) / sizeof(ufs2_daddr_t))

/*
 * Each file system has a number of inodes statically allocated.
 * We allocate one inode slot per NFPI fragments, expecting this
 * to be far more than we will ever need.
 */
#define	NFPI		2


int	Eflag;			/* Erase previous disk contents */
int	Lflag;			/* add a volume label */
int	Nflag;			/* run without writing file system */
int Oflag = 2;		/* file system format (1 => UFS1, 2 => UFS2) */
int	Rflag;			/* regression test */
int	Uflag;			/* enable soft updates for file system */
int	jflag;			/* enable soft updates journaling for filesys */
int	Xflag = 0;		/* exit in middle of newfs for testing */
int	Jflag;			/* enable gjournal for file system */
int	lflag;			/* enable multilabel for file system */
int	nflag;			/* do not create .snap directory */
int	tflag;			/* enable TRIM */
intmax_t fssize;		/* file system size */
off_t	mediasize;		/* device size */
int	sectorsize;		/* bytes/sector */
int	realsectorsize;		/* bytes/sector in hardware */
int	fsize = 0;		/* fragment size */
int	bsize = 0;		/* block size */
int	maxbsize = 0;		/* maximum clustering */
int	maxblkspercg = MAXBLKSPERCG; /* maximum blocks per cylinder group */
int	minfree = MINFREE;	/* free space threshold */
int	metaspace;		/* space held for metadata blocks */
int	opt = DEFAULTOPT;	/* optimization preference (space or time) */
int	density;		/* number of bytes per inode */
int	maxcontig = 0;		/* max contiguous blocks to allocate */
int	maxbpg;			/* maximum blocks per file in a cyl group */
int	avgfilesize = AVFILESIZ;/* expected average file size */
int	avgfilesperdir = AFPDIR;/* expected number of files per directory */
char	*volumelabel = NULL;	/* volume label for filesystem */
//struct uufsd disk;		/* libufs disk structure */

static char	device[MAXPATHLEN];
ufs2_daddr_t part_ofs;

int32_t d_fd;


char *d_name;
static struct	csum *fscs;

struct unionacg {
	struct cg d_cg;
	char d_buf[MAXBSIZE];
};
struct unionacg d_acg;
#define acg d_acg.d_cg

char *iobuf;
static long iobufsize;
static const char *failmsg;

/*
 * Ensure that the buffer is aligned to the I/O subsystem requirements.
 */
#define BUF_MALLOC(newbufpp, data, size) {				     \
	if (data != NULL && (((intptr_t)data) & (LIBUFS_BUFALIGN - 1)) == 0) \
		*newbufpp = (void *)data;				     \
	else								     \
		*newbufpp = aligned_alloc(LIBUFS_BUFALIGN, size);	     \
}


static u_int32_t
newfs_random(void)
{
	static u_int32_t nextnum = 1;

	if (Rflag)
		return (nextnum++);
	return (arc4random());
}
