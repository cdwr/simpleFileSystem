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

#define SUPERBLOCK  1
#define GDBLOCK     2
#define ROOT_INODE  2

#define FREE        0
#define READY       1

#define BLKSIZE  1024
#define NMINODE   128
#define NMTABLE		 16 // added, not used
#define NFD        16
#define NPROC       2

#define READ        0
#define WRITE       1
#define READWRITE   2
#define APPEND      3

typedef struct minode{
	INODE INODE;
	int dev, ino;
	int refCount;
	int dirty;
	int mounted;
	struct MTABLE *mptr;
}MINODE;

typedef struct oft{
	int  mode;
	int  refCount;
	MINODE *mptr;
	int  offset;
}OFT;

typedef struct mtable{
	int dev; // device number
	int ninodes;
	int nblocks;
	/*
	int free_blocks;
	int free_inodes;
	*/
	int inode_start;
	int bmap;
	int imap;
	int iblock;
	MINODE *mntDirPtr; // pointer to mount point MINODE
	char devname[64]; // device name
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
