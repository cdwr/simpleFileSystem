int mount(char *name, char *pathname)
{
	printf("attempting to mount %s as %s\n", name, pathname);

	int fd;
	SUPER *sp;
	GD *gp;
	char buf[BLKSIZE];

	// Check if we already mounted this
	for(int x = 0; x < NMTABLE; x++)
	{
		MTABLE *table = &mtable[x];
		if (table->dev == 0)
		{
			continue;
		}

		if (strcmp(name, table->devname) == 0)
		{
			printf("Error, %s device is already mounted as mount point %s\n", name, table->mntName);
			return -1;
		}
		else if (strcmp(name, table->mntName) == 0)
		{
			printf("Error, %s mount point is already mounted as device %s\n", pathname, table->devname);
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

		if ((fd = open(name, O_RDWR)) < 0)
		{
			printf("open %s failed\n", name);
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
	}
}