int rmdir(char *pathname){
	int ino;
	MINODE *mip, *pip;

	if (pathname[0] == '/')
		dev = root->dev;
	else
		dev = running->cwd->dev;

	ino = getino(pathname);
	mip = iget(dev, ino);
	
	//check if permissions are met
	if(running->uid != 0)
	{
		if(running->uid != mip->INODE.i_uid)
		{
			printf("Invalid file permissions; user:%d, inode->uid:%d\n",running->uid, mip->INODE.i_uid);
			iput(mip);
			return -1;
		}
	}
		
	//check if its a dir type
	if(!S_ISDIR(mip->INODE.i_mode))
	{
		printf("INODE is not dir\n");
		iput(mip);
		return -1;
	}
	
	//check if inode is busy
	if(mip->refCount != 1)
	{
		printf("Dir in use. (mip->refCount = %d)\n", mip->refCount);
		iput(mip);
		return -1;
	}
	
	//check if file is empty
	if(!isEmpty(mip)){
		printf("Dir isn't empty \n");
		iput(mip);
		return -1;
	}
	
	for (int i=0; i<12; i++){
		if (mip->INODE.i_block[i]==0)
			continue;
		
		bdealloc(mip->dev, mip->INODE.i_block[i]);
	}


	idealloc(mip->dev, mip->ino);
	iput(mip);

	pip = iget(mip->dev, findino(mip, NULL));
	rm_child(pip, basename(pathname));
	
	pip->INODE.i_links_count--;
	pip->INODE.i_mtime = time(0L); //might be broken
	pip->INODE.i_atime = time(0L);
	pip->dirty = 1;
	
	iput(pip);
	return 0;
}
	
int isEmpty(MINODE *mip){ //return 1 if empty, 0 if not
	char buf[BLKSIZE];
	char name[256];
	DIR *dp;
	char *cp;
		
	if(mip->INODE.i_links_count > 2){
		return 0;
	}
			
	if(mip->INODE.i_links_count == 2){
		get_block(dev, mip->INODE.i_block[0], buf);
		dp = (DIR*)buf;
		cp = buf;
		
		while(cp < buf + BLKSIZE){
			strncpy(name, dp->name, dp->name_len);
			name[dp->name_len] = 0;
			
			if(strcmp(name,".") != 0 && strcmp(name, "..") != 0){
					return 0;
			}
				
			cp += dp->rec_len;
			dp = (DIR *)cp;
		}
	}
	
	return 1;
}


int rm_child(MINODE *pip, char *child)
{
	int i;
	char *cp, *cp2, buf[BLKSIZE], entName[64];
	DIR *dp, *dp2;
	INODE *ip;
	int br = 1;
	ip = &pip->INODE;
	// search parent INODE data blocks for entry of name
	for (i=0; i<12; i++)
	{
		if (ip->i_block[i] == 0)
			continue;
		get_block(pip->dev, ip->i_block[i], buf);
		cp = buf;
		dp = (DIR *)cp;
		while (cp < &buf[BLKSIZE])
		{
			memcpy(entName, dp->name, dp->name_len);
			entName[dp->name_len] = 0;
			if (strcmp(entName, child) == 0)
			{
				br = 0;
				break;
			}
			cp += dp->rec_len;
			dp = (DIR *)cp;
		}
		if (br == 0)
		{
			break;
		}
	}

	if (br == 1)
	{
		printf("error: could not find %s in parent directory\n", child);
		return -1;
	}

	// if last entry in block
	if (cp + dp->rec_len == &buf[BLKSIZE]) {
		// if only entry in data block
		if (cp == buf) {
			// deallocate block
			bdealloc(pip->dev, ip->i_block[i]);
			// shift upper blocks down
			memmove(&ip->i_block[i], &ip->i_block[i+1], 11-i);
			ip->i_block[11] = 0;
			return 0;
		}
		// else, search for previous dirent and increase its rec_len
		cp2 = buf;
		dp2 = (DIR *)cp2;
		while (cp2 + dp2->rec_len != cp) {
			cp2 += dp2->rec_len;
			dp2 = (DIR *)cp2;
		}
		dp2->rec_len += dp->rec_len;
	} else {
		// else, shift later blocks down
		cp2 = cp;
		dp2 = (DIR *)cp;
		// copy contents of next directory into current directory
		while (cp2 + dp2->rec_len < &buf[BLKSIZE]) {
			cp2 += dp2->rec_len;
			dp2 = (DIR *)cp2;
			dp->inode = dp2->inode;
			dp->name_len = dp2->name_len;
			memcpy(dp->name, dp2->name, dp2->name_len);
			// if next directory is last directory
			// then add its rec_len to curr rec_len
			dp->rec_len += (cp2 + dp2->rec_len < &buf[BLKSIZE] ? 0 : dp2->rec_len);
			dp = dp2; // no need to increase cp
		}
	}
	put_block(pip->dev, ip->i_block[i], buf);
	return 0;
  
}

