int mount(char *devname, char *mountname)
{
	if (strcmp(devname, "\0") == 0 || (strcmp(mountname, "\003") == 0 || strcmp(mountname, "/") == 0))
	{
		for (int i = 0; i < NMTABLE; i++)
		{
			MTABLE *table = &mtable[i];
			if (table->dev == 0)
			{
				continue;
			}

			printf("%s is mounted as %s\n", table->devname, table->mntName);
		}
		return 0;
	}

		//check if permissions are met
	if(running->uid != 0)
	{
		printf("Must be super user (uid=0)\n");
		return -1;
	}

	int fd, ino, DEV;
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

	// Find right TABLE entry
	for(int x = 0; x < NMTABLE; x++)
	{
		MTABLE *mt = &mtable[x];
		if (mt->dev != 0)
		{
			continue;
		}

		if ((fd = open(devname, O_RDWR)) < 0)
		{
			printf("open %s failed\n", devname);
			return -1;
		}
		DEV = fd;    // fd is the global dev

		/********** read super block  ****************/
		get_block(DEV, 1, buf);
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

		get_block(DEV, 2, buf);
		gp = (GD *)buf;

		mt->bmap = bmap = gp->bg_block_bitmap;
		mt->imap = imap = gp->bg_inode_bitmap;
		mt->inode_start = gp->bg_inode_table;
		printf("bmp=%d imap=%d inode_start = %d\n", mt->bmap, mt->imap, mt->inode_start);

		ino = getino(mountname);  //to get ino:
		mip = iget(dev, ino);    //to get its minode in memory;

		//printf("ino=%d mode=%d\n", mip->ino, S_ISDIR(mip->INODE.i_mode));

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

		mt->dev = DEV;
		strcpy(mt->devname, regname);
		strcpy(mt->mntName, mountname);

		// Mark mount_point's minode as being mounted on and let it point at the
		// MOUNT table entry, which points back to the mount_point minode.
		mip->mptr = mt;
		mip->mounted = 1;
		mt->mntDirPtr = mip;
		printf("mounted\n");
		//printf("cwd:[%d %d]\n", dev, mip->ino);
		return 0;
	}
	printf("Was unable to mount. All slots are full.");
	return -1;
}

int umount(char *filesys)
{
	char buf[BLKSIZE];
	int found = 0;
	MINODE *mip;
	MTABLE *table;

	// Check if filesys is mounted
	for(int x = 0; x < NMTABLE; x++)
	{
		table = &mtable[x];
		if (table->dev == 0)
		{
			continue;
		}
		if (strcmp(filesys, table->devname) == 0)
		{
			
			found = 1;
			break;
		}

	}
	if (!found){
		printf("Filesys not mounted\n");
		return -1;
	}

	//check if filesys is busy
	for (int i = 0; i < NMINODE; i++)	
	{ 
		MINODE *node = &minode[i];
		//I'm pretty sure this is how to search for it, the minode ray contains all open references, right?
		if (node->dev == table->dev && node->refCount > 0)
		{
			if (node == table->mntDirPtr && node->refCount > 1)
			{
				printf("Filesys is busy\n");
				return -1;
			}
		}
	}

	if(running->uid != 0)
	{
		printf("Must be super user (uid=0)\n");
		return -1;
	}

	//should we be marking mounted in iget?
	table->mntDirPtr->mounted = 0;

	iput(table->mntDirPtr);
	table->dev = 0;
	dev = root->dev;

	printf("%s was unmounted\n", filesys);
	return 0;
}