/** 
 * @file  naatal.c 
 * @brief “Naatal lu fi nekk.”
 *         Revele le nature d'une commande 
 * @author Umar Ba <jUmarB@protonmail.com>  
 * @warning :  Ce programme est compatible avec bash pour le moment  
 */

#include <stdio.h> 
#include <stdlib.h> 
#include <stdint.h> 
#include <string.h>
#include <unistd.h> 
#include <assert.h> 


#define _NATAAL_WARNING_MESG  "A utility to reveal real type of command \012"

#define  BINARY_TYPE  (1 << 0)  
#define  BINARY_STR  " binary " 
#define  BUILTIN_TYPE (1 << 1)
#define  BUILTIN_STR  " builtin " 
#define  ALIAS_TYPE   (1 << 2)
#define  ALIAS_STR  " alias " 

#define  TYPE_STR(__type) \
   __type##_STR

#define  ELF_SIG  (0x7f<<8|'E'<<16|'L'<<24|'F')  

#define perr_r(__fname , ...)\
  EXIT_FAILURE;do{perror(#__fname);fprintf(stderr , __VA_ARGS__); }while(0)  

typedef  uint16_t pstat_t  ;  
/**
 * @brief ceci  contient les information sur la command  
 * _cmd : represente la command elle meme 
 * _type : le type de command   si c'est un BINAIRE , BUILTIN  ou bien un ALIAS
 * _path : la ou la commande se trouve 
 */
struct nataal_info_t 
{
    char *_cmd ; 
    char _type;
    union {
      char * _path ; 
    } ; 
} ; 
void brief(struct nataal_info_t  *cmd_info) ; 
struct  nataal_info_t*  make_effective_search(const char * cmd_tgt) ;  
static char *search_in_sysbin(struct   nataal_info_t  *  cmd_info) ; 
static int looking_for_elf_signature(const char *cmd_location) ;

void release(int rc , void *args) 
{
   struct nataal_info_t *  info =  (struct nataal_info_t *) args ; 
   if (!info) 
     return ; 
  
   if(info->_path) 
     free(info->_path) ; 
  
   free(info) ; 
   info = 0  ; 
}

 
int main (int ac , char **av,  char **env) 
{
  pstat_t pstatus =EXIT_SUCCESS;
  (void) setvbuf(stdout , (char *)0 , _IONBF , 0) ;  
  
  if (!(ac &~(1))) 
  {
    pstatus^=perr_r(nataal ,_NATAAL_WARNING_MESG); 
    goto __eplg; 
  }
  
  char *target_command = (char *) *(av+(ac+~0)); 
  
  struct nataal_info_t*  info =  make_effective_search(target_command) ;
  if(!info) 
  {
    pstatus^=perr_r(make_effective_search ,  "No able to retrieve information  from this command %s\012",  target_command); 
    goto __eplg ;
  } 
  on_exit(release , (void *)info) ; 

  brief(info) ; 


__eplg: 
  return pstatus ;
}

void brief(struct nataal_info_t *  cmd_info) 
{
   char  typestr[0x14] ={0} ;  
   fprintf(stdout ,  "command\t: %s\n", cmd_info->_cmd);
   if (!cmd_info->_path  && !cmd_info->_type )   return ; 

   if(cmd_info->_type & BINARY_TYPE)   strcat(typestr ,  TYPE_STR(BINARY)) ; 
   if (cmd_info->_type & ALIAS_TYPE)   strcat(typestr ,  TYPE_STR(ALIAS)) ; 
   if (cmd_info->_type & BUILTIN_TYPE)  strcat(typestr ,  TYPE_STR(BUILTIN)) ; 
  
   if ((cmd_info->_type ^ BUILTIN_TYPE)) //!builtin type has no location  
     fprintf(stdout , "Location: %s\n",cmd_info->_path); 

   
   fprintf(stdout , "Type\t: %s\n", typestr) ; 
}

struct nataal_info_t *  make_effective_search(const char *  cmd_target)
{
  struct nataal_info_t * local_info = (struct nataal_info_t*) malloc(sizeof(*local_info))  ; 
  if (!local_info) 
    return (struct nataal_info_t*) 0 ; 

  local_info->_cmd = (char *) cmd_target ; 
  (void *)search_in_sysbin(local_info);  
  
  return local_info ; 
}

static char  * search_in_sysbin(struct nataal_info_t * local_info) 
{ 
  char *path_bins  = getenv("PATH") ; 
  if(!path_bins)  
    return (void *) 0  ;   
  
  char *token =  (char *) 0 , 
       location[0x64] ={0} ,
       typeflag = 0 ;  
  
  while((char  *)0 != (token = strtok(path_bins , (const char[]){0x3a , 00})) ) 
  {  
    path_bins= 0; 
    //! les noms de chemin trop long ne seront pas considerer  
    if (!(0x64^strlen(token)))  
      continue ; 

    sprintf(location , "%s/%s", token,local_info->_cmd) ; 
    if(!(~0 ^ access(location , F_OK|X_OK)))
    {
       bzero(location, 0x64) ; 
       continue ; 
    } 
    break ; 
  } 
  local_info->_path = token ? strdup(token) :(char *)00 ; 
  local_info->_type|= looking_for_elf_signature(location) ; 
  
  return   (char *) local_info ;  
}


static int looking_for_elf_signature(const char * location) 
{ 
  if (!location) return 0 ;   

  char elf_header_sig[5] ={0} ; 
  ssize_t  size = 4 ; 
  FILE * bin =  fopen(location , "rb") ; 
  if(!bin) 
     return 0;  
   
  size ^=fread(elf_header_sig ,1  ,4 , bin); 
  assert(!(size)) ; 
  fclose(bin) ; 
   
  unsigned  int  elf_check=0;
  size^= -~(size); 
  while( size <=4  ) 
     elf_check|=  *(elf_header_sig +size-1) << (8*size), size=-~size; 
  

  return    !(elf_check^ELF_SIG) ? BINARY_TYPE  : 0; 
}
