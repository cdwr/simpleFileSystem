/*********** util.c file ****************/

int get_block(int dev, int blk, char *buf)
{
	lseek(dev, (long)blk*BLKSIZE, 0);
	read(dev, buf, BLKSIZE);
}
int put_block(int dev, int blk, char *buf)
{
	lseek(dev, (long)blk*BLKSIZE, 0);
	write(dev, buf, BLKSIZE);
}

int tokenize(char *pathname)
{
	int i;
	char *s;
	//printf("tokenize %s\n", pathname);

	strcpy(gpath, pathname);   // tokens are in global gpath[ ]
	n = 0;

	s = strtok(gpath, "/");
	while(s){
		name[n] = s;
		n++;
		s = strtok(0, "/");
	}

	//for (i= 0; i<n; i++)
		//printf("%s  ", name[i]);
	//printf("\n");
}

// return minode pointer to loaded INODE
MINODE *iget(int dev, int ino)
{
	int i;
	MINODE *mip;
	char buf[BLKSIZE];
	int blk, offset;
	INODE *ip;

	for (i=0; i<NMINODE; i++){
		mip = &minode[i];
		if (mip->dev == dev && mip->ino == ino)
		{
			mip->refCount++;
			//printf("found [%d %d] as minode[%d] in core\n", dev, ino, i);
			return mip;
		}
	}
		
	for (i=0; i<NMINODE; i++){
		mip = &minode[i];
		if (mip->refCount == 0)
		{
			//printf("allocating NEW minode[%d] for [%d %d]\n", i, dev, ino);
			mip->refCount = 1;
			mip->dev = dev;
			mip->ino = ino;

			// get INODE of ino into buf[ ]    
			blk    = (ino-1)/8 + inode_start;
			offset = (ino-1) % 8;

			//printf("iget: ino=%d blk=%d offset=%d\n", ino, blk, offset);

			get_block(dev, blk, buf);
			ip = (INODE *)buf + offset;
			// copy INODE to mp->INODE
			mip->INODE = *ip;
			return mip;
		}
	}
	printf("PANIC: no more free minodes\n");
	return 0;
}

void iput(MINODE *mip)
{
	int blk, disp;
	INODE *ip;
	char buf[BLKSIZE];
	mip->refCount--;

	if (mip->refCount > 0) 
		return 0;
	if (!mip->dirty) 
		return 0;

	// write INODE back to disk
	blk = (mip->ino - 1) / 8 + inode_start;
	disp = (mip->ino - 1) % 8;
	get_block(mip->dev, blk, buf);
	ip = (INODE *)buf + disp;
	*ip = mip->INODE;
	put_block(mip->dev, blk, buf);
	mip->dirty = 0;
	return 0;
}

int search(MINODE *mip, char *name)
{
	char *cp, c, sbuf[BLKSIZE], temp[256];
	DIR *dp;
	INODE *ip;

	//printf("search for %s in MINODE = [%d, %d]\n", name, mip->dev, mip->ino);
	ip = &(mip->INODE);

	/*** search for name in mip's data blocks: ASSUME i_block[0] ONLY ***/

	get_block(dev, ip->i_block[0], sbuf);
	dp = (DIR *)sbuf;
	cp = sbuf;
	//printf("  ino   rlen  nlen  name\n");

	while (cp < sbuf + BLKSIZE)
	{
		strncpy(temp, dp->name, dp->name_len);
		temp[dp->name_len] = 0;
		//printf("%4d  %4d  %4d    %s\n", dp->inode, dp->rec_len, dp->name_len, temp);
		if (strcmp(temp, name)==0)
		{
			//printf("found %s : ino = %d\n", temp, dp->inode);
			return dp->inode;
		}
		cp += dp->rec_len;
		dp = (DIR *)cp;
	}

	return 0;
}

int getino(char *pathname)
{
	int i, ino, blk, disp, pino;
	char buf[BLKSIZE];
	INODE *ip;
	MINODE *mip;
	MINODE *newmip;
	MTABLE *mt;
	char pathnamecpy[64];
	strcpy(pathnamecpy, pathname);
	char *parent = dirname(pathnamecpy);

	//printf("getino: pathname=%s\n", pathname);
	if (strcmp(pathname, "/")==0)
			return 2;
	


	// starting mip = root OR CWD
	if (pathname[0]=='/')
		mip = root;
	else
		mip = running->cwd;

	mip->refCount++;         // because we iput(mip) later
	
	tokenize(pathname);

	for (i = 0; i < n; i++)
	{

		if(!maccess(mip, 'x')){ //why does this need to be in the for loop?
			printf("getino: Access Denied (%d)\n", maccess(mip, 'x'));
			iput(mip);
			return -1;
		}

		if ((strcmp(name[i], "..") == 0) && (mip->dev != root->dev) && (mip->ino == 2))
		{
			//printf("UP cross mounting point\n");

			// Get mountpoint MINODE
			for (int i = 0; i < NMTABLE; i++)
			{ 
				MTABLE *table = &mtable[i];
				if (table->dev == mip->dev)
				{
					newmip = table->mntDirPtr;
					break;
				}
			}

			iput(mip);
			pino = findino(newmip, newmip->ino);
			mip = iget(newmip->dev, pino);
			//printf("dev=%d, ino=%d\n", mip->dev, pino);

			dev = newmip->dev;
			continue;
			
		}
		//printf("===========================================\n");
		//printf("getino: i=%d name[%d]=%s\n", i, i, name[i]);

		ino = search(mip, name[i]);
		//printf("dev=%d, ino=%d, name=%s\n", dev, mip->ino, name[i]);
		newmip = iget(dev, ino);
		iput(mip);
		mip = newmip;

		if (ino==0)
		{
			iput(mip);
			//printf("name %s does not exist\n", name[i]);
			return 0;
		}

		if (mip->mounted)
		{
			//printf("DOWN cross mounting point\n");

			mt = mip->mptr;

			iput(mip); // discard current mip

			dev = mt->dev;
			mip = iget(dev, 2);
		}
	}

	iput(mip);                   // release mip  
	return mip->ino;
}

int findmyname(MINODE *parent, u32 myino, char *myname) 
{
	// WRITE YOUR code here:
	// search parent's data block for myino;
	// copy its name STRING to myname[ ];
	
	char buf[BLKSIZE], *cp;
	DIR *dp;
	get_block(parent->dev, parent->INODE.i_block[0], buf);
	cp = buf;
	dp = (DIR *)buf;
	
	while(cp < buf + BLKSIZE){
		strncpy(myname, dp->name, 64);
		myname[dp->name_len] = '\0';
				
		if(dp->inode == myino)
		{
			break;
		}

		cp += dp->rec_len;
		dp = (DIR*) cp;
	}

	return;
}

int findino(MINODE *mip, u32 *myino) // myino = ino of . return ino of ..
{
	char buf[BLKSIZE], *cp;   
	DIR *dp;
	get_block(mip->dev, mip->INODE.i_block[0], buf);
	cp = buf; 
	dp = (DIR *)buf;
	cp += dp->rec_len;
	dp = (DIR *)cp;
	//printf("findIno: dpName:%s myino:%s", dp->name, *myino);
	return dp->inode;
}

int string_to_int(char *str)
{
	int num;
	sscanf(str, "%d", &num);
	return num;
}

int tst_bit(char *buf, int bit){
	return buf[bit/8] & (1 << (bit % 8));
}

int set_bit(char *buf, int bit){
	buf[bit/8] |= (1 << (bit % 8));
}

int clr_bit(char *buf, int bit)
{
	buf[bit/8] &= (1 << (bit % 8));
}

int incFreeInodes(int dev)
{
	char buf[BLKSIZE];
	// inc free INODEs count in SUPER
	get_block(dev, SUPERBLOCK, buf);
	sp = (SUPER *)buf;
	sp->s_free_inodes_count++;
	put_block(dev, SUPERBLOCK, buf);

	// inc free INODEs count in GD
	get_block(dev, GDBLOCK, buf);
	gp = (GD *)buf;
	gp->bg_free_inodes_count++;
	put_block(dev, GDBLOCK, buf);
	return 0;
}

int incFreeBlocks(int dev)
{
	char buf[BLKSIZE];
	// inc free blocks count in SUPER
	get_block(dev, SUPERBLOCK, buf);
	sp = (SUPER *)buf;
	sp->s_free_blocks_count++;
	put_block(dev, SUPERBLOCK, buf);

	// inc free blocks count in GD
	get_block(dev, GDBLOCK, buf);
	gp = (GD *)buf;
	gp->bg_free_blocks_count++;
	put_block(dev, GDBLOCK, buf);
	return 0;
}

int decFreeInodes(int dev){
	// dec free INODes count in SUPEBLOCK
	char buf[BLKSIZE]; 
	get_block(dev, SUPERBLOCK, buf);
	sp = (SUPER *)buf;
	sp->s_free_inodes_count--;
	put_block(dev, SUPERBLOCK, buf);

	// dec free INODes count in GD
	get_block(dev, GDBLOCK, buf);
	gp = (GD *)buf;
	gp->bg_free_inodes_count--;
	put_block(dev, GDBLOCK, buf);
}


int decFreeBlocks(int dev){
	// dec free Blocks count in SUPEBLOCK
	char buf[BLKSIZE];
	get_block(dev, SUPERBLOCK, buf);
	sp=(SUPER*)buf;
	sp->s_free_blocks_count--;
	put_block(dev, SUPERBLOCK, buf);

	// dec free Blocks count in GD
	get_block(dev, GDBLOCK, buf);
	gp=(GD*)buf;
	gp->bg_free_blocks_count--;
	put_block(dev, GDBLOCK, buf);
}

int ialloc(int dev)
{
	char buf[BLKSIZE];
	
	get_block(dev, imap, buf);
	for(int i = 0; i < ninodes; i++)
	{
		if(tst_bit(buf, i) == 0)
		{
			set_bit(buf, i);
			decFreeInodes(dev); //not in sample, but is in book??
			put_block(dev, imap, buf);

			return (i+1);
		}
	}
	printf("no more free inodes\n");
	return 0;
}

int balloc(int dev)
{
	char buf[BLKSIZE];

	// read inode_bitmap block
	get_block(dev, bmap, buf);

	for (int b = 0; b < nblocks; b++)
	{
		if (tst_bit(buf, b) == 0)
		{
			set_bit(buf, b);
			decFreeBlocks(dev);

			put_block(dev, bmap, buf);

			return b+1;
		}
	}

	printf("balloc(): no more free blocks\n");
	return 0;
}

int idealloc(int dev, int ino)
{
	char buf[BLKSIZE];
	if (ino > ninodes)
	{
		printf("inumber %d out of range\n", ino);
		return 0;
	}

	// get inode bitmap block
	get_block(dev, imap, buf);
	clr_bit(buf, ino-1);
	incFreeInodes(dev);

	// write buf back
	put_block(dev, imap, buf);

	return 0;
}

int bdealloc(int dev, int bno)
{
	char buf[BLKSIZE];
	get_block(dev, bmap, buf);
	clr_bit(buf, bno);
	incFreeBlocks(dev);
	put_block(dev, bmap, buf);
	return 0;
}

int read_link(char *pathname, char *link)
{
	int ino;
	MINODE *mip;
	INODE *ip;
	// get pathname's inumber:
	if (pathname[0] == '/')
	{
		dev = root->dev;
	}
	else
	{
		dev = running->cwd->dev;
	}

	// get parent's MINODE
	mip = iget(dev, ino);
	if (!S_ISLNK(mip->INODE.i_mode)) {
		printf("Error, file %s is not a symbolic link\n", pathname);
		iput(mip);
		return -1;
	}
	strcpy(link, (char *)mip->INODE.i_block);
	return 0;
}


int can_open(char *pathname, int mode)
{
	int ino, bitmask;
	MINODE *mip;
	char name_of_link[64];

	// get pathname's inumber:
	if (pathname[0] == '/')
	{
		dev = root->dev;
	}
	else
	{
		dev = running->cwd->dev;
	}

	// Get ino.
	ino = getino(pathname);
	if (ino == 0)
	{
		printf("Error, invalid path inode for path=%s\n", pathname);
		return -1;
	}

	// get parent MINODE
	mip = iget(dev, ino);

	// Check if files is a symlink, then this is a special case.
	// Then we need to get ino of linked file.
	if (S_ISLNK(mip->INODE.i_mode))
	{
		read_link(pathname, name_of_link);
		iput(mip);
		// get pathname's inumber:
		if (pathname[0] == '/')
		{
			dev = root->dev;
		}
		else
		{
			dev = running->cwd->dev;
		}
		// Get ino of symlinked file
		ino = getino(name_of_link);
		// get parent MINODE of linked file
		mip = iget(dev, ino);
	}

	iput(mip);

	// Depending on the open mode 0|1|2|3, check for OFT's offset.
	switch(mode)
	{
		case 0:
			bitmask = 0b100;
			break;
		case 1:
			bitmask = 0b010;
			break;
		case 2:
			bitmask = 0b110;
			break;
		case 3:
			bitmask = 0b010;
			break;
		default:
			printf("Error, mode %d is not supported\n", mode);
			return 0;
	}


	if ((running->uid != 0) && (bitmask != (bitmask & mip->INODE.i_mode)))
	{
		return 0;
	}

	// shift to the right by 3
	bitmask <<= 3;

	if((running->gid != mip->INODE.i_gid) && (bitmask != (bitmask & mip->INODE.i_mode)))
	{
		return 0;
	}
	
	// shift to the right by 3
	bitmask <<= 3;

	if((running->uid != mip->INODE.i_uid) && (bitmask != (bitmask & mip->INODE.i_mode)))
	{
		return 0;
	}


	// Check whether the file is ALREADY opened with INCOMPATIBLE mode:
	// If it's already opened for W, RW, APPEND : reject.
	// (that is, only multiple R are OK)

	for (int i = 0; i < NFD; i++)
	{
		if (running->fd[i] == NULL)
		{
			continue;
		}
		if (running->fd[i]->mptr == mip) {
			if (running->fd[i]->mode || mode) {
				printf("%s cannot be opened, as due to incompatible mode\n", pathname);
				return 0;
			}
		}
	}
	return 1;
}

// Print all OFT's
int pfd()
{
	OFT *oftp;
	// FD, Mode, offset, INODE.
	printf("fd\tmode\toffset\tINODE\n");
	printf("--\t----\t------\t-----\n");
	for (int i = 0; i < NFD; i++)
	{
		if ((oftp = running->fd[i]) == NULL)
		{
			continue;
		}

		printf("%d\t", i);
		switch (oftp->mode)
		{
			case 0:
				printf("READ\t"); 
				break;
			case 1:
				printf("WRITE\t");
				break;
			case 2:
				printf("R/W\t"); 
				break;
			case 3: 
				printf("APPEND\t");
				break;
		}
		printf("%d\t", oftp->offset);
		printf("[%d,%d]\n", oftp->mptr->dev, oftp->mptr->ino);
	}
}

// Returns string representation of mode.
char* mode_to_string(int mode)
{
	switch (mode)
	{
		case READ:
			return "READ";
		case WRITE:
			return "WRITE";
		case READWRITE:
			return "READ/WRITE";
		case APPEND: 
			return "APPEND";
		default:
			return "ERROR, NOT VALID MODE!";
	}
}

int sw(){
	if(running->pid == 0)
		running = &proc[1];
	else
	{
		running = &proc[0];
	}
	printf("RUnning proc: pid=%d\n", running->pid);
	return 0;
}


int access(char *pathname, char mode) {
       /* if running is SUPERuser process: return OK;
  
       get INODE of pathname into memory;
       if owner     : return OK if rwx --- --- mode bit is on
       if same group: return OK if --- rwx --- mode bit is on
       if other     : return OK if --- --- rwx mode bit is on
       
       return NO; */

	MINODE *mip;
	int access = -1;

	if(running->uid == 0)
		return 1;

	mip = iget(dev, getino(pathname));

	if(running->uid == mip->INODE.i_uid){
		//printf("access: same uid\n");
		if(mode == 'r') (mip->INODE.i_mode & 1 << 8) ? (access = 1) : (access = 0);
		else if(mode == 'w')(mip->INODE.i_mode & 1 << 7) ? (access = 1) : (access = 0);
		else if(mode == 'x')(mip->INODE.i_mode & 1 << 6) ? (access = 1) : (access = 0);
	}
	else if(running->gid == mip->INODE.i_gid){
		//printf("access: same gid\n");
		if(mode == 'r') (mip->INODE.i_mode & 1 << 5) ? (access = 1) : (access = 0);
		else if(mode == 'w')(mip->INODE.i_mode & 1 << 4) ? (access = 1) : (access = 0);
		else if(mode == 'x')(mip->INODE.i_mode & 1 << 3) ? (access = 1) : (access = 0);
	}
	else { //WTF does other mean??
		//printf("access: other\n");
		if(mode == 'r') (mip->INODE.i_mode & 1 << 2) ? (access = 1) : (access = 0);
		else if(mode == 'w')(mip->INODE.i_mode & 1 << 1) ? (access = 1) : (access = 0);
		else if(mode == 'x')(mip->INODE.i_mode & 1 << 0) ? (access = 1) : (access = 0);
	}
	
	iput(mip);
	return access;
}

int maccess(MINODE *mip, char mode){  // same as access() but work on mip
	int access = -1;

	if(running->uid == 0)
		return 1;

	if(running->uid == mip->INODE.i_uid){
		//printf("access: same uid\n");
		if(mode == 'r') (mip->INODE.i_mode & 1 << 8) ? (access = 1) : (access = 0);
		else if(mode == 'w')(mip->INODE.i_mode & 1 << 7) ? (access = 1) : (access = 0);
		else if(mode == 'x')(mip->INODE.i_mode & 1 << 6) ? (access = 1) : (access = 0);
	}
	else if(running->gid == mip->INODE.i_gid){
		//printf("access: same gid\n");
		if(mode == 'r') (mip->INODE.i_mode & 1 << 5) ? (access = 1) : (access = 0);
		else if(mode == 'w')(mip->INODE.i_mode & 1 << 4) ? (access = 1) : (access = 0);
		else if(mode == 'x')(mip->INODE.i_mode & 1 << 3) ? (access = 1) : (access = 0);
	}
	else { //WTF does other mean??
		//printf("access: other\n");
		if(mode == 'r') (mip->INODE.i_mode & 1 << 2) ? (access = 1) : (access = 0);
		else if(mode == 'w')(mip->INODE.i_mode & 1 << 1) ? (access = 1) : (access = 0);
		else if(mode == 'x')(mip->INODE.i_mode & 1 << 0) ? (access = 1) : (access = 0);
	}

	return access;
}