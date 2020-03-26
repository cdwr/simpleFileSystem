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
    if (mip->dev == dev && mip->ino == ino){
       mip->refCount++;
       //printf("found [%d %d] as minode[%d] in core\n", dev, ino, i);
       return mip;
    }
  }
    
  for (i=0; i<NMINODE; i++){
    mip = &minode[i];
    if (mip->refCount == 0){
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

   while (cp < sbuf + BLKSIZE){
     strncpy(temp, dp->name, dp->name_len);
     temp[dp->name_len] = 0;
     //printf("%4d  %4d  %4d    %s\n", dp->inode, dp->rec_len, dp->name_len, temp);
     if (strcmp(temp, name)==0){
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
  int i, ino, blk, disp;
  char buf[BLKSIZE];
  INODE *ip;
  MINODE *mip;

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

  for (i=0; i<n; i++){
      //printf("===========================================\n");
      //printf("getino: i=%d name[%d]=%s\n", i, i, name[i]);
 
      ino = search(mip, name[i]);

      if (ino==0){
         iput(mip);
         //printf("name %s does not exist\n", name[i]);
         return 0;
      }
      iput(mip);                // release current mip
      mip = iget(dev, ino);     // get next mip
   }

  iput(mip);                   // release mip  
   return ino;
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
        
    if(dp->inode == myino){
    	break;
    }

    cp += dp->rec_len;
    dp = (DIR*) cp;
  }

  return;
}

int enter_name(MINODE *pip, int myino, char *myname){
	char buf[BLKSIZE], *cp;
  DIR *dp;
  int need_l = 4 *((8+strlen(myname)+3)/4);
	int ideal_l = 0;
	int remain;
	int bno;
	
  for(int i = 0; i<12; i++){
  	if(pip->INODE.i_block[i]==0){
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

    while(cp+dp->rec_len<buf+BLKSIZE){
    	cp+=dp->rec_len;
     	dp=(DIR*)cp;
    }
     	
    ideal_l = 4*((8+dp->name_len+3)/4);
    remain=dp->rec_len - ideal_l;

    if(remain >= need_l){
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

int tst_bit(char *buf, int bit){
	return buf[bit/8] & (1 << (bit % 8));
}

int set_bit(char *buf, int bit){
	buf[bit/8] |= (1 << (bit % 8));
}

int decFreeInodes(int dev){
	char buf[BLKSIZE]; 
	get_block(dev, SUPERBLOCK, buf);
	sp = (SUPER *)buf;
	sp->s_free_inodes_count--;
	put_block(dev, SUPERBLOCK, buf);
	get_block(dev, GDBLOCK, buf);
	gp = (GD *)buf;
	gp->bg_free_inodes_count--;
	put_block(dev, GDBLOCK, buf);
}


int decFreeBlocks(int dev){
	char buf[BLKSIZE];
	get_block(dev, SUPERBLOCK, buf);
	sp=(SUPER*)buf;
	sp->s_free_blocks_count--;
	put_block(dev, SUPERBLOCK, buf);

	get_block(dev, GDBLOCK, buf);
	gp=(GD*)buf;
	gp->bg_free_blocks_count--;
	put_block(dev, SUPERBLOCK, buf);
}

int ialloc(int dev){
	int i;
	char buf[BLKSIZE];
	
	get_block(dev, imap, buf);
	for(i=0; i<imap; i++){
		if(tst_bit(buf, i)==0){
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
  int  b;
  char buf[BLKSIZE];

  // read inode_bitmap block
  get_block(dev, bmap, buf);

  for (b=0; b < ninodes; b++){
    if (tst_bit(buf, b)==0){
       set_bit(buf,b);
       decFreeBlocks(dev);

       put_block(dev, bmap, buf);

       return b+1;
    }
  }
  printf("balloc(): no more free blocks\n");
  return 0;
}