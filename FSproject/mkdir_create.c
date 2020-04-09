int enter_name(MINODE *pip, int myino, char *myname){
	char buf[BLKSIZE], *cp;
	DIR *dp;
	int need_l = 4 *((8+strlen(myname)+3)/4);
	int ideal_l = 0;
	int remain;
	int bno;
	
	for(int i = 0; i<12; i++)
	{
		if(pip->INODE.i_block[i]==0)
		{
			bno=balloc(dev);
			pip->INODE.i_block[i]=bno;
			get_block(dev, bno, buf);
			pip->INODE.i_size+=BLKSIZE;
			cp=buf;
			dp=(DIR*)buf;
			dp->inode=myino;
			dp->rec_len=BLKSIZE;
			dp->name_len=strlen(myname);
			strcpy(dp->name, myname);
			put_block(dev, bno, buf);
			return 1;
		}

		bno=pip->INODE.i_block[i];
		get_block(dev, bno, buf);
		cp=buf;
		dp=(DIR*)buf;

		while(cp+dp->rec_len<buf+BLKSIZE)
		{
			cp+=dp->rec_len;
			dp=(DIR*)cp;
		}

		ideal_l = 4*((8+dp->name_len+3)/4);
		remain=dp->rec_len - ideal_l;

		if(remain >= need_l)
		{
			dp->rec_len=ideal_l;
			cp+=dp->rec_len;
			dp=(DIR*)cp;
			dp->inode=myino;
			dp->rec_len=BLKSIZE-((u32)cp-(u32)buf);
			dp->name_len=strlen(myname);
			strcpy(dp->name, myname);
			put_block(dev, bno, buf);
			return 1;
		}
	}

	printf("Only 12 block allowed\n");
	return -1;
}


int makedir(char *name){
	MINODE *mip, *pip;
	char pathcpy[64];
	char *parent, *child;
	int pino;
	if (name[0] == '/') {
		mip = root;
		dev = root->dev;
	} else {
		mip = running->cwd;
		dev = mip->dev;
	}
	strcpy(pathcpy, name);
	child = basename(pathcpy);
	parent = dirname(name);
	pino = getino(parent);
	pip = iget(dev, pino);
	
	//check its a directory
	if(!S_ISDIR(pip->INODE.i_mode)){
		printf("ERROR:pip is not dir\n");
		return -1;
	}
	
	// check child doen't exist
	if(search(pip, name)){
		printf("Directory already exists\n");
		return -1;
	}
	

	if(mymkdir(pip, child)){
		pip->INODE.i_links_count++;
		pip->INODE.i_mtime = time(0L); //might be broken
		pip->dirty = 1;
	}
	
	iput(pip);
	return 0;
}

int mymkdir(MINODE *pip, char *name)
{
	MINODE *mip;

	char buf[BLKSIZE];

	int ino = ialloc(pip->dev);
	int bno = balloc(pip->dev);

	printf("ino and bno are: %d, %d\n", ino, bno);

	mip = iget(pip->dev, ino);
	INODE *ip = &mip->INODE;

	ip->i_mode = 0x41ED;								// OR 040755: DIR type and permissions
	ip->i_uid  = running->uid;							// Owner uid 
	ip->i_gid  = running->gid;							// Group Id
	ip->i_size = BLKSIZE;								// Size in bytes 
	ip->i_links_count = 2;								// Links count=2 because of . and ..
	ip->i_atime = ip->i_ctime = ip->i_mtime = time(0L);	// set to current time
	ip->i_blocks = 2;									// LINUX: Blocks count in 512-byte chunks 
	ip->i_block[0] = bno;								// new DIR has one data block

	for(int i = 1; i < 15; i++)
	{
		ip->i_block[i] = 0;
	}
	
	mip->dirty = 1;										// mark minode dirty
	iput(mip);											// write INODE to disk

	get_block(pip->dev, bno, buf);

	bzero(buf, BLKSIZE);            // optional: clear buf[ ] to 0
	DIR *dp = (DIR *)buf;           // make . entry
	dp->inode = ino;
	dp->rec_len = 12;
	dp->name_len = 1;
	dp->name[0] = '.';              // make .. entry: pino=parent DIR ino, blk=allocated block
	dp = (char *)dp + 12;
	dp->inode = pip->ino;
	dp->rec_len = BLKSIZE-12;       // rec_len spans block
	dp->name_len = 2;
	dp->name[0] = dp->name[1] = '.';
	put_block(pip->dev, bno, buf);       // write to blk on diks

	enter_name(pip, ino, name);
	return 1;
}


int create_file(char *name){
	MINODE *mip, *pip;
	char pathcpy[64];
	char *parent, *child;
	int pino;
	if (name[0] == '/') {
		mip = root;
		dev = root->dev;
	} else {
		mip = running->cwd;
		dev = mip->dev;
	}
	strcpy(pathcpy, name);
	child = basename(pathcpy);
	parent = dirname(name);
	pino = getino(parent);
	pip = iget(dev, pino);
	
	//check its a directory
	if(!S_ISDIR(pip->INODE.i_mode))
	{
		printf("ERROR:pip is not a directory\n");
		return -1;
	}
	
	// check child doen't exist
	if(search(pip, child))
	{
		printf("File already exists\n");
		return -1;
	}
	
	if(my_create_file(pip, child))
	{
		pip->INODE.i_mtime = time(0L); //might be broken
		pip->dirty = 1;
	}
	
	iput(pip);
	return 0;

}

int my_create_file(MINODE *pip, char *name){
	MINODE *mip;

	int ino = ialloc(pip->dev);

	printf("ino and bno are: %d\n", ino);

	mip = iget(pip->dev, ino);
	INODE *ip = &mip->INODE;

	ip->i_mode = 0x81A4;								// OR 0100644: DIR type and permissions
	ip->i_uid  = running->uid;							// Owner uid 
	ip->i_gid  = running->gid;							// Group Id
	ip->i_links_count = 1;
	ip->i_size = BLKSIZE;								// Size in bytes 
	ip->i_atime = ip->i_ctime = ip->i_mtime = time(0L);	// set to current time
	ip->i_blocks = 0;									// LINUX: Blocks count in 512-byte chunks 
	ip->i_block[0] = 0;

	mip->dirty = 1;										// mark minode dirty
	iput(mip);											// write INODE to disk

	enter_name(pip, ino, name);
	return 1;

}