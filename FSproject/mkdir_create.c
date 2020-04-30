int enter_name(MINODE *pip, int myino, char *myname)
{
	char buf[BLKSIZE], *cp;
	DIR *dp;
	int need_l = 4 * ((8 + strlen(myname) + 3) / 4);
	int ideal_l = 0;
	int remain, i;
	
	INODE *ip = &pip->INODE;
	for(i = 0; i < 12; i++)
	{
		if (ip->i_block[i] == 0)
		{
			break;
		}

		get_block(dev, ip->i_block[i], buf);
		cp = buf;
		dp = (DIR*)buf;

		while(cp + dp->rec_len < &buf[BLKSIZE])
		{
			cp += dp->rec_len;
			dp = (DIR*)cp;
		}

		ideal_l = 4 * ((8 + dp->name_len + 3) / 4);
		remain = dp->rec_len - ideal_l;

		if(remain >= need_l)
		{
			dp->rec_len = ideal_l;
			cp += dp->rec_len;
			dp = (DIR*)cp;
			dp->inode = myino;
			dp->rec_len = remain;
			dp->name_len = strlen(myname);
			memcpy(dp->name, myname, strlen(myname));
			put_block(pip->dev, ip->i_block[i], buf);
			return 1;
		}
	}
	if (i >= 12) {
		printf("Only 12 block allowed\n");
		return -1;
	}

	ip->i_block[i] = balloc(pip->dev);
	ip->i_blocks += 2;
	get_block(pip->dev, ip->i_block[i], buf);
	cp = buf;
	dp = (DIR *)cp;
	dp->inode = myino;
	dp->rec_len = BLKSIZE;
	dp->name_len = strlen(myname);
	memcpy(dp->name, myname, strlen(myname));
	put_block(pip->dev, ip->i_block[i], buf);
	return -1;
}

int makedir(char *name)
{
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
	
	// check its a directory
	if(!S_ISDIR(pip->INODE.i_mode)){
		printf("ERROR:pip is not dir\n");
		iput(pip);
		return -1;
	}
	
	// check child doen't exist
	if(search(pip, name)){
		printf("Directory already exists\n");
		iput(pip);
		return -1;
	}
	
	//permissions check
	if (!maccess(pip, 'w')){
      printf("makedir: Access Denied\n");
      iput(pip); 
	  return -1;
	}

	mymkdir(pip, child);
	pip->INODE.i_links_count++;
	pip->INODE.i_mtime = time(0L);
	pip->dirty = 1;
	
	
	iput(pip);
	return 0;
}

int mymkdir(MINODE *pip, char *name)
{
	MINODE *mip;

	char *cp, buf[BLKSIZE];

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

	DIR *dp = (DIR *)buf;           // make . entry
	cp = buf;
	dp->inode = ino;
	dp->rec_len = 12;
	dp->name_len = 1;
	memcpy(dp->name, ".", 1);              // make .. entry: pino=parent DIR ino, blk=allocated block
	cp += dp->rec_len;
	dp = (DIR *)cp;
	dp->inode = pip->ino;
	dp->rec_len = BLKSIZE-12;       // rec_len spans block
	dp->name_len = 2;
	memcpy(dp->name, "..", 2);
	put_block(pip->dev, bno, buf);       // write to blk on diks

	enter_name(pip, ino, name);
	return 1;
}


int create_file(char *name)
{
	MINODE *mip, *pip;
	char *parent, *child;
	int pino;
	if (name[0] == '/') {
		mip = root;
		dev = root->dev;
	} else {
		mip = running->cwd;
		dev = mip->dev;
	}

	child = basename(name);
	parent = dirname(name);
	pino = getino(parent);
	pip = iget(dev, pino);
	
	//check its a directory
	if(!S_ISDIR(pip->INODE.i_mode))
	{
		printf("ERROR:pip is not a directory\n");
		iput(pip);
		return -1;
	}
	
	// check child doen't exist
	if(search(pip, child))
	{
		printf("File already exists\n");
		iput(pip);
		return -1;
	}

	my_create_file(pip, child);
	pip->INODE.i_mtime = time(0L);
	pip->dirty = 1;
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
	ip->i_size = 0;										// Size in bytes 
	ip->i_atime = ip->i_ctime = ip->i_mtime = time(0L);	// set to current time
	ip->i_blocks = 0;									// LINUX: Blocks count in 512-byte chunks
	for(int i = 0; i < 15; i++)
	{
		ip->i_block[i] = 0;
	}

	mip->dirty = 1;										// mark minode dirty
	iput(mip);											// write INODE to disk

	enter_name(pip, ino, name);
	return 1;

}

int mychmod(char *pathname, int mode)
{
	int ino;
    MINODE *mip;
    char linkname[64];

    if (pathname[0] == '/')
            dev = root->dev;
    else
            dev = running->cwd->dev;

    ino = getino( pathname);

    if (ino == 0) 
		return -1;

    mip = iget(dev, ino);

	if(running->uid != mip->INODE.i_uid){
		printf("Access denied: must be owner.\n");
		return -1;
	}

    if (S_ISLNK(mip->INODE.i_mode)) {
            readlink(pathname, linkname);
            iput(mip);
            if (linkname[0] == '/')
                    dev = root->dev;
            else
                    dev = running->cwd->dev;
            ino = getino(linkname);
            mip = iget(dev, ino);
    }
	printf("mode: %o\n", mode);
	mip->INODE.i_mode = mode | (mip->INODE.i_mode & 0777000);
	mip->dirty = 1;
	iput(mip);
}