#include "types.h"
#include "defs.h"
#include "param.h"
#include "fs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "file.h"

struct {
  int current_user;
  uint cnt;
  char user[10][2][MAXUSERNAME];
} utable;

int
initUtable(void)
{
  int i;
  utable.current_user = -1;
  for(i = 0; i < 10; i++){
    memset(utable.user[i], 0, MAXUSERNAME);
  }
  cprintf("initUtable!!!\n");
  return 0;
}

int
addUser(char *username, char *password)
{
  if(!username || !password)
    panic("Null pointer. (addUser)");
  return 0;
}

int 
deleteUser(char *username) 
{
  if(!username)
    panic("Null pointer. (deleteUser)");
  return 0;
}

int
getCurrentUser(void)
{
  return utable.current_user;
}

char*
getCurrentUsername(int idx)
{
  if(idx < 0 || idx >= 10)
    return 0;
  return utable.user[idx][0];
}

int
setuser(struct inode* account)
{
  uint i;
  int r, max, off, n, n1;
  char *addr;
  
  if(readi(account, (char*)utable.user, 0, 10 * (MAXPASSWORD + MAXUSERNAME)) < 0)
    return 0;
  
  for(i = 0; i < 10; i++)
    utable.cnt += (utable.user[i][0][0]) ? 1 : 0;
  if(!utable.cnt){
    strncpy(utable.user[0][0], "root", MAXUSERNAME);
    strncpy(utable.user[0][1], "0000", MAXPASSWORD);
    utable.cnt = 1;
  }

  max = ((MAXOPBLOCKS-1-1-2) / 2) * 512;

  addr = (char*)utable.user;
  n = 320;
  i = off = 0;
  while(i < n){
    n1 = n - i;
    if(n1 > max)
      n1 = max;

    if ((r = writei(account, addr + i, off, n1)) > 0)
      off+=r;
    if(r < 0)
      break;
    if(r != n1)
      panic("short filewrite");
    i += r;
  }
  return 1;
}