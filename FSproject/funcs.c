/************* funcs.c file **************/

int chdir(char *pathname)   
{
	// READ Chapter 11.7.3 HOW TO chdir
	MINODE *mip;
	if(pathname[0]<=0){
		running->cwd = iget(root->dev, 2);
		return 0;
	}
	if(strcmp(pathname, "/") == 0){
		printf("CDing to root\n");
		running->cwd = iget(root->dev, 2);
		return 0;
	}
	int ino = getino(pathname);
	if(ino==0){
		printf("directory doesn't exist\n");
		return 0;
	}
	mip = iget(running->cwd->dev, ino);
	if((mip->INODE.i_mode & 0100000) == 0100000){
		iput(mip);
		printf("cannot cd to non dir\n");
		return -1;
	}
	iput(running->cwd);
	running->cwd = mip;
	printf("cwd:[%d %d]\n", running->cwd->dev,running->cwd->ino);
	return 0;
}

int ls_file(MINODE *mip, char *name)
{
	INODE *ip = &mip->INODE;
	//printf("ls_file: to be done: READ textbook for HOW TO!!!!\n");
	if(S_ISDIR(mip->INODE.i_mode))
		printf("d");
	if(S_ISREG(mip->INODE.i_mode))
		printf("r");
	if(S_ISLNK(mip->INODE.i_mode))
		printf("l");

	printf((mip->INODE.i_mode & 1 << 8) ? "r" : "-");
	printf((mip->INODE.i_mode & 1 << 7) ? "w" : "-");
	printf((mip->INODE.i_mode & 1 << 6) ? "x" : "-");

	printf((mip->INODE.i_mode & 1 << 5) ? "r" : "-");
	printf((mip->INODE.i_mode & 1 << 4) ? "w" : "-");
	printf((mip->INODE.i_mode & 1 << 3) ? "x" : "-");

	printf((mip->INODE.i_mode & 1 << 2) ? "r" : "-");
	printf((mip->INODE.i_mode & 1 << 1) ? "w" : "-");
	printf((mip->INODE.i_mode & 1 << 0) ? "x" : "-");

	char time[64];
	ctime_r((time_t *)&mip->INODE.i_mtime, time);
	time[strlen(time)-1]=0;
	printf(" %3d\t%3d\t%3d\t%20s\t%6d ", ip->i_links_count,  mip->INODE.i_uid, mip->INODE.i_gid, time, mip->INODE.i_size);

	printf("%s\n", name);
}

int ls_dir(MINODE *mip)
{
	char buf[BLKSIZE], temp[256];
	DIR *dp;
	char *cp;
	MINODE *file;

	// Assume DIR has only one data block i_block[0]
	get_block(dev, mip->INODE.i_block[0], buf); 
	dp = (DIR *)buf;
	cp = buf;

	while (cp < buf + BLKSIZE){
		strncpy(temp, dp->name, dp->name_len);
		temp[dp->name_len] = 0;

		file = iget(dev, getino(temp));
		ls_file(file, temp);
		iput(file); 
		
		cp += dp->rec_len;
		dp = (DIR *)cp;
	}

	printf("\n");
}

int ls(char *pathname)  
{
	printf("ls %s\n", pathname);
	printf("ls CWD only! YOU do it for ANY pathname\n");
	ls_dir(running->cwd);
}

void pwd(MINODE *wd, int child)
{
	char buf[512];
	char *cpy;
	char name[64];
	
	if (wd->ino == root->ino){
		printf("/");
		return;
	}

	MINODE *pip;

	pip = iget(wd->dev, findino(wd, 0));
	findmyname(pip, wd->ino, &name);

	if(wd->ino != root->ino){
		pwd(pip, wd->ino);
		iput(pip);
		printf("%s/", name);
	}
	
  return;
}

int makedir(char *name){
	char *ppath;
	char *pino;
	
	char buf[BLKSIZE], temp[256];
	DIR *dp;
	char *cp;
	printf("name: %s", name);
	printf("dev: %d", dev);

	MINODE *pip = iget(running->cwd->dev, findino(running->cwd, 0));
	
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
	
	
	
	if(mymkdir(pip, name)){
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

	int ino = ialloc(dev);
	int bno = balloc(dev);

	printf("ino and bno are: %d, %d", ino, bno);

	mip = iget(dev, ino);
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


	int pino = getino(name);
	get_block(dev, bno, buf);

	bzero(buf, BLKSIZE);            // optional: clear buf[ ] to 0
	DIR *dp = (DIR *)buf;           // make . entry
	dp->inode = ino;
	dp->rec_len = 12;
	dp->name_len = 1;
	dp->name[0] = '.';              // make .. entry: pino=parent DIR ino, blk=allocated block
	dp = (char *)dp + 12;
	dp->inode = pino;
	dp->rec_len = BLKSIZE-12;       // rec_len spans block
	dp->name_len = 2;
	dp->name[0] = dp->name[1] = '.';
	put_block(pip->dev, bno, buf);       // write to blk on diks

	enter_name(pip, ino, name);
	return 1;
}


int create_file(char *name){
	char *ppath;
	char *pino;
	
	char buf[BLKSIZE], temp[256];
	DIR *dp;
	char *cp;

	MINODE *pip = iget(running->cwd->dev, findino(running->cwd, 0));
	
	//check its a directory
	if(!S_ISREG(pip->INODE.i_mode)){
		printf("ERROR:pip is not file\n");
		return -1;
	}
	
	// check child doen't exist
	if(search(pip, name)){
		printf("File already exists\n");
		return -1;
	}
	
	
	
	if(my_create_file(pip, name)){
		pip->INODE.i_mtime = time(NULL); //might be broken
		pip->dirty = 1;
	}
	
	iput(pip);
	return 0;

}

int my_create_file(MINODE *pip, char *name){
	MINODE *mip;

	char buf[BLKSIZE];

	int ino = ialloc(dev);
	int bno = balloc(dev);

	printf("ino and bno are: %d, %d", ino, bno);

	mip = iget(dev, ino);
	INODE *ip = &mip->INODE;

	ip->i_mode = 0x81A4;								// OR 0100644: DIR type and permissions
	ip->i_uid  = running->uid;							// Owner uid 
	ip->i_gid  = running->gid;							// Group Id
	ip->i_links_count = 1;
	ip->i_size = BLKSIZE;								// Size in bytes 
	ip->i_atime = ip->i_ctime = ip->i_mtime = time(0L);	// set to current time
	ip->i_blocks = 2;									// LINUX: Blocks count in 512-byte chunks 
	ip->i_block[0] = bno;								// new DIR has one data block

	for(int i = 1; i < 15; i++)
	{
		ip->i_block[i] = 0;
	}
	
	mip->dirty = 1;										// mark minode dirty
	iput(mip);											// write INODE to disk

	enter_name(pip, ino, name);
	return 1;

}
