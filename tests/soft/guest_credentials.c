#define _GNU_SOURCE
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <grp.h>

int main(void) {
 uid_t r,e,s; gid_t gr,ge,gs;
 if(getresuid(&r,&e,&s) || getresgid(&gr,&ge,&gs)) return 11;
 printf("initial: %u:%u:%u %u:%u:%u\n",r,e,s,gr,ge,gs);
 if(r||e||s||gr||ge||gs)return 12;
 gid_t g[2] = {65534, 42};
 if(setgroups(2,g)){perror("setgroups(2)");return 13;}
 gid_t out[5];
 int n=getgroups(5,out);
 printf("groups: count=%d %u %u\n",n,out[0],out[1]);
 if(n!=2||out[0]!=65534||out[1]!=42)return 14;
 if(setresgid(-1,65534,-1)){perror("setresgid");return 15;}
 if(setresuid(-1,42,-1)){perror("setresuid");return 16;}
 if(getresuid(&r,&e,&s) || getresgid(&gr,&ge,&gs))return 17;
 printf("dropped: %u:%u:%u %u:%u:%u\n",r,e,s,gr,ge,gs);
 if(r!=0||e!=42||s!=0||gr!=0||ge!=65534||gs!=0)return 18;
 if(setgroups(0,NULL)==0||errno!=EPERM){puts("expected EPERM after drop");return 19;}
 if(setresuid(-1,0,-1)){perror("restore-root");return 20;}
 if(setresgid(-1,0,-1)){perror("restore-root-group");return 21;}
 if(setgroups(0,NULL)){perror("setgroups(0)");return 22;}
 if(getgroups(0,NULL)!=0)return 23;
 printf("restored: uid=%u gid=%u groups=%d\n",geteuid(),getegid(),getgroups(0,NULL));
 if(setuid(42)){perror("setuid 42");return 24;}
 if(getresuid(&r,&e,&s))return 25;
 printf("permanent: %u:%u:%u\n",r,e,s);
 if(r!=42||e!=42||s!=42)return 26;
 if(setresuid(-1,0,-1)==0||errno!=EPERM) return 27;
 puts("CREDENTIALS_OK");
 return 0;
}
