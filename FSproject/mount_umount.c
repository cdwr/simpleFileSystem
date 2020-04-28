int mount(char *devname, char *mountname)
{
	

	int fd, ino;
	SUPER *sp;
	GD *gp;
	MINODE *mip;
	char buf[BLKSIZE], *regname;

	regname = basename(devname);
	printf("attempting to mount %s as %s\n", devname, mountname);

	// Check if we already mounted this
	for(int x = 0; x < NMTABLE; x++)
	{
		MTABLE *table = &mtable[x];
		if (table->dev == 0)
		{
			continue;
		}

		if (strcmp(devname, table->devname) == 0)
		{
			printf("ERROR: %s device is already mounted as mount point %s\n", devname, table->mntName);
			return -1;
		}
		else if (strcmp(devname, table->mntName) == 0)
		{
			printf("ERROR: %s mount point is already mounted as device %s\n", mountname, table->devname);
			return -1;
		}
	}

	// Check if we already mounted this
	for(int x = 0; x < NMTABLE; x++)
	{
		MTABLE *mt = &mtable[x];
		if (mt->dev == 0)
		{
			continue;
		}

		if ((fd = open(devname, O_RDWR)) < 0)
		{
			printf("open %s failed\n", devname);
			exit(1);
		}
		dev = fd;    // fd is the global dev

		/********** read super block  ****************/
		get_block(dev, 1, buf);
		sp = (SUPER *)buf;

		/* verify it's an ext2 file system ***********/
		if (sp->s_magic != 0xEF53)
		{
			printf("magic = %x is not an ext2 filesystem\n", sp->s_magic);
			exit(1);
		}

		printf("EXT2 FS OK\n");

		mt->ninodes = sp->s_inodes_count;
		mt->nblocks = sp->s_blocks_count;

		get_block(dev, 2, buf);
		gp = (GD *)buf;

		mt->bmap = bmap = gp->bg_block_bitmap;
		mt->imap = imap = gp->bg_inode_bitmap;
		mt->inode_start = gp->bg_inode_table;
		printf("bmp=%d imap=%d inode_start = %d\n", mt->bmap, mt->imap, mt->inode_start);

		ino = getino(mountname);  //to get ino:
		mip = iget(dev, ino);    //to get its minode in memory;

		printf("ino=%d mode=%d\n", mip->ino, S_ISDIR(mip->INODE.i_mode));

		// check its a directory
		if (!S_ISDIR(mip->INODE.i_mode))
		{
			printf("ERROR: mip is not dir\n");
			iput(mip);
			return -1;
		}

		// Check mount_point is NOT busy (e.g. can't be someone's CWD)
		if (mip->refCount > 1)
		{
			printf("ERROR: %s is busy by something else\n", mountname);
			iput(mip);
			return -1;
		}

		mt->dev = dev;
		strcpy(mt->devname, mountname);
		strcpy(mt->mntName, regname);

		// Mark mount_point's minode as being mounted on and let it point at the
		// MOUNT table entry, which points back to the mount_point minode.
		mip->mptr = mt;
		mt->mntDirPtr = mip;
		mip->mounted = 1;
	}
}