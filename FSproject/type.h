/*************** type.h file ************************/
typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int   u32;

typedef struct ext2_super_block SUPER;
typedef struct ext2_group_desc  GD;
typedef struct ext2_inode       INODE;
typedef struct ext2_dir_entry_2 DIR;

SUPER *sp;
GD    *gp;
INODE *ip;
DIR   *dp;   

#define SUPERBLOCK 1
#define GDBLOCK 2
#define ROOT_INODE 2

#define FREE        0
#define READY       1

#define BLKSIZE  1024
#define NMINODE   128
#define NMTABLE		 10 //added, not used
#define NFD        16
#define NPROC       2

typedef struct minode{
  INODE INODE;
  int dev, ino;
  int refCount;
  int dirty;
  int mounted;
  struct mntable *mptr;
}MINODE;

typedef struct oft{
  int  mode;
  int  refCount;
  MINODE *mptr;
  int  offset;
}OFT;

typedef struct mtable{
	int dev;
	int ninodes;
	int nblocks;
	int free_blocks;
	int free_inodes;
	int bmap;
	int imap;
	int iblock;
	MINODE *mntDirPtr;
	char devname[64];
	char mntName[64];
}MTABLE;

typedef struct proc{
  struct proc *next;
  int          pid;
  int          status;
  int          uid, gid;
  MINODE      *cwd;
  OFT         *fd[NFD];
}PROC;
