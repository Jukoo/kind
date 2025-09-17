/** 
 * @file  naatal.c 
 * @brief “Naatal lu fi nekk.”
 *         Revele le nature d'une commande 
 * @author Umar Ba <jUmarB@protonmail.com>  
 * @warning :  Ce programme est compatible avec bash pour le moment 
 *             - je ne connais pas le comportement exacte  avec les autres shell (csh , fish , nu, elvish ...)
 * NOTE: Ce programme  peut servir de reference si vous voulez faire votre propre implementation
 */

#include <stdio.h> 
#include <stdlib.h> 
#include <stdint.h> 
#include <string.h>
#include <unistd.h> 
#include <assert.h> 
#include <errno.h> 
#include <pwd.h> 
#include <fcntl.h> 
#include <sys/types.h> 
#include <sys/wait.h> 
#include <sys/stat.h> 
#include <sys/cdefs.h> 

#if __has_attribute(warn_unused_result) 
# define __must_use  __attribute__((warn_unused_result)) 
#else  
# define __must_use /*  Nothing */ 
#endif 

#define _Nullable  
#define _Nonnulable  [static 01]  

#define _NATAAL_WARNING_MESG  "A utility to reveal real type of command \012"

#define  BINARY_TYPE  (1 << 0)  
#define  BINARY_STR   "binary" 
#define  BUILTIN_TYPE (1 << 1)
#define  BUILTIN_STR  "builtin" 
#define  ALIAS_TYPE   (1 << 2)
#define  ALIAS_STR    "alias" 
#define  SHELL_KW_TYPE (1<< 3) 
#define  SHELL_KW_STR  "Shell keyword" 

#define  TYPE_STR(__type) \
   __type##_STR

#define  BASH_SIG  0x73616268 
#define  ELF_SIG   0x4c457f46  
#define  MA_ALIAS  0x61696c6173 

#define  SIGMATCH(signature, strmatch ,lsize) ({\
    int s =  (lsize >>8 );\
    while(s <=(lsize &0xff))\
      signature&=~*(strmatch+ (s-1)) << (8*s),s=-~s;\
    })

#define perr_r(__fname , ...)\
  EXIT_FAILURE;do{perror(#__fname);fprintf(stderr , __VA_ARGS__); }while(0)  

#define  BASHRCS \
  "/home/%s/.bashrc",\
  "/etc/bash/bashrc"  /**Vous pouvez ajouter d'autre  sources*/ 

static unsigned int option_search= BINARY_TYPE|ALIAS_TYPE|BUILTIN_TYPE| SHELL_KW_TYPE  ;  
typedef  uint16_t pstat_t  ;  
char  bashrcs_sources[][20] = {
  BASHRCS,
  '\000'
}; 

#define  ALIAS_MAX_ROW 0xa 
#define  ALIAS_STRLEN  0x64  
/** Contiendra toutes les aliase definit*/ 
char bash_aliases[ALIAS_MAX_ROW][ALIAS_STRLEN]= {0} ; 
char *has_alias = (char *)00  ; 

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
struct  nataal_info_t*  make_effective_search(const char * cmd_tgt , int search_option) ;  
static char *search_in_sysbin(struct   nataal_info_t  *  cmd_info) ; 
static int looking_for_elf_signature(const char *cmd_location) ;  

/** Pre-Chargement des alias ... */
int  __preload_all_aliases(char * _Nullable directive) __must_use;
/*
 * @brief  detecte alias key word from bashrc sources   
 * */
static int load_alias_from(char (*)[ALIAS_STRLEN] , const off_t  /* Read only */) ;


static char * looking_for_aliases(struct nataal_info_t *   cmd_info  , int  mtdata);  

static char  is_builtin(const char*  cmd) ;  

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

static void check_compatibility_environment(void) ;   
 
int main (int ac , char **av,  char **env) 
{
  pstat_t pstatus =EXIT_SUCCESS;
  (void) setvbuf(stdout , (char *)0 , _IONBF , 0) ; 
  
  /* Pour le moment le programme est compatible  avec bash les autres  peut etre dans un future proche */ 
  check_compatibility_environment() ;  
  
  if (!(ac &~(1))) 
  {
    pstatus^=perr_r(nataal ,_NATAAL_WARNING_MESG); 
    goto __eplg; 
  }
  char *target_command = (char *) *(av+(ac+~0)); 
  int summary_stat = __preload_all_aliases(target_command); 
  if( 0 >= (summary_stat >>  8) )  
   option_search&=~ALIAS_TYPE ;  /*disable  alias searching */
  
  /*
   * Franchement j'ai la flemme  de creer d'autre variables 
   * ou de changer ma signature de fonction ... bref autant tout compacter 
   */ 
    option_search = (option_search << 8 ) |  summary_stat  ; 

  struct nataal_info_t*  info =  make_effective_search(target_command ,  option_search) ;
  if(!info) 
  {
    pstatus^=perr_r(make_effective_search ,  "No able to retrieve information  for this '%s' command 012",  target_command); 
    goto __eplg ;
  } 
  on_exit(release , (void *)info) ; 

  brief(info) ; 


__eplg: 
  return pstatus ;
}

static void check_compatibility_environment(void) 
{
  unsigned long sig=0;
  char  *token  = (char *)00,
        *shname = (token),
        *shenv  = getenv("SHELL"); 

  if(!shenv) 
  {
    (void) perr_r(compatiblity_errror, "on %s Your Shell is not supported Yet",  __func__) ; 
    exit(EXIT_FAILURE) ;
  }
  

  while((void *)00  != (token = strtok(shenv , "/"))) shname = (token), shenv=0;

  size_t slen=(1<<8|strlen(shname)) ; 


  /*!Peut etre plus tard j'aurai besoin  de tester d'autre type de shell 
   * Pour le moment un simple warning  est suffisant 
   * */ 
  sig|=BASH_SIG;   
  SIGMATCH(sig, shname, slen) ; 
  if(sig) printf("Not able to check shell  signature for bash\012"); 

}
int  __preload_all_aliases(char * directive)  
{
  /**See if it's a regular user  */ 
  int  authorized_uid = 0x3e8 ;  
  
  char *current_user_session = getenv("USER") ; 
  if(!current_user_session) 
    return ~0 ;   
 
  struct passwd *user  = getpwnam(current_user_session);
  if(!user) 
    return ~0 ; 
  
  if(authorized_uid  >   user->pw_uid )
  {
    /** Ici je n'autorise pas  les utilisateur standard et root */  
    /**TODO: specifier un code d'erreur  plus approprier */
    return    ~0;  
  }
  
  /**On charge les fichier source  des bashrcs*/ 
  char user_bashrc[0x32] = {0} ;
  sprintf(user_bashrc , *(bashrcs_sources) ,  current_user_session) ;  

  //!TODO: mettre ca dans une function 
  off_t offset_index= ~0;  
  while(  00 != *(*(bashrcs_sources+ (++offset_index))) )    
  { 
     if (0 == offset_index) 
     {
       memcpy((bash_aliases+offset_index)  , user_bashrc  , strlen(user_bashrc));  
       continue ; 
     } 
     memcpy(*(bash_aliases+offset_index)  ,  *(bashrcs_sources+offset_index),
         strlen(*(bashrcs_sources+offset_index))) ; 
  } 

  return load_alias_from(bash_aliases , offset_index) ;
 

}

/*TODO: make aliases persistant (use tmpfile or something ) 
 *      That hold  the values without re-reading the files
 *- For Later: **/
static int   load_alias_from(char  (*bashrc_list) [ALIAS_STRLEN] , const off_t starting_offset) 
{
  int offset = starting_offset , 
      naliases = 0 ; 

  while(naliases < starting_offset) 
  {
     while( 00 !=  *(*(bashrc_list + naliases ))) 
     {
       const char  *bashsrc= *(bashrc_list+naliases) ; 
       FILE *fp = fopen(bashsrc , "r") ;  
       if(!fp) 
         return  errno;   
       char inline_buffer[1024] ={0} ; 
       while((fgets(inline_buffer, 1024 ,  fp)))  
       { 
          //! Voir si la ligne commence avec le mot 'alias' 
          if(strstr(inline_buffer, ALIAS_STR ) && ( 
                !((*inline_buffer & 0xff) ^ 0x61)) && 
                !((*(inline_buffer+4) & 0xff ^0x73))
            )
          {

            /*Charge les  aliases  a partir  de l'offset */
            memcpy(*(bashrc_list+offset), inline_buffer , strlen(inline_buffer));
            offset=-~offset ;  
          }

          bzero(inline_buffer , 1024) ;  
       }
       
       fclose(fp) ; 
       break ; 
     }

     naliases=-~naliases ; 
  } 
  return (naliases  << 8 | starting_offset) ; 
}
void brief(struct nataal_info_t *  cmd_info) 
{
   char  typestr[0x32] ={0};   
   fprintf(stdout ,  "command\t: %s\n", cmd_info->_cmd);
   if (!cmd_info->_path  && !cmd_info->_type )   return ; 

#define  _Append(__typestr ,__str )({\
    size_t s  = strlen(__typestr) ;\
    strcat((__typestr+s), __str) ; \
    *(__typestr+(strlen(__typestr))) = 0x3a;  \
    })

   if (cmd_info->_type & BINARY_TYPE)  _Append(typestr, TYPE_STR(BINARY)) ; 
   if (cmd_info->_type & ALIAS_TYPE)   _Append(typestr, TYPE_STR(ALIAS)) ; 
   if (cmd_info->_type & BUILTIN_TYPE) _Append(typestr, TYPE_STR(BUILTIN)) ; 
  
   if ((cmd_info->_type ^ BUILTIN_TYPE)) //!builtin type has no location  
     fprintf(stdout , "Location: %s\n",cmd_info->_path ? cmd_info->_path :"Not Found"); 

   fprintf(stdout , "Type\t: [:%s]\n", (1 < strlen(typestr))? typestr: "Unknow:")  ; 

   if (has_alias)  
     fprintf(stdout , "Alias\t: %s", has_alias)  ; 
}

struct nataal_info_t *  make_effective_search(const char *  cmd_target ,  int search_option)
{
  struct nataal_info_t * local_info = (struct nataal_info_t*) malloc(sizeof(*local_info))  ; 
  if (!local_info) 
    return (struct nataal_info_t*) 0 ; 

  local_info->_cmd = (char *) cmd_target ; 
  unsigned   data = (search_option  &  0xff ) ; 
  search_option  >>=8 ;  
  
  if (search_option & ALIAS_TYPE)  
  {  
    has_alias =  looking_for_aliases(local_info, data ) ; 
  }
  if (search_option & BINARY_TYPE )
    (void *)search_in_sysbin(local_info);  
  
  if(search_option  &  BUILTIN_TYPE) 
  {
    /** TODO : how to detect  if is a builtin command*/ 
  }
  
     
  
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
  unsigned  int  elf_check=0;

  FILE * bin =  fopen(location , "rb") ; 
  if(!bin) 
     return 0;  
   
  size ^=fread(elf_header_sig ,1  ,4 , bin); 
  assert(!(size)) ; 
  fclose(bin) ; 
   
  size^= -~(size); 
  while( size <=4  ) 
     elf_check|=  *(elf_header_sig +size-1) << (8*size), size=-~size; 
  

  return    !(elf_check^ELF_SIG) ? BINARY_TYPE  : 0; 
}



static char * looking_for_aliases(struct nataal_info_t *  cmd_info , int mtadata ) 
{
  int starting_offset =  (mtadata & 0xff) , 
      naliases = (mtadata >> 8 );  

  while(00 != *(*(bash_aliases+(starting_offset))))   
  {
    char  * alias  = *(bash_aliases+(starting_offset)) ;
    if(strstr(alias  , cmd_info->_cmd))   
    {
      cmd_info->_type  |= ALIAS_TYPE ;   
      return    *(bash_aliases+(starting_offset)) ; 
    }
    starting_offset=-~starting_offset ; 

  }

  return (char *) 00 ; 

}
