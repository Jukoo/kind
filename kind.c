/** 
 * @file   kind.c   
 * @brief  Une alternative moderne a la command  unix 'type' 
 * @keyboard-layout: QWERTY  
 * -----------------------------------------------------------------------------------------------------
 * Copyright (C) 2025 Umar Ba <jUmarB@protonmail.com> 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * @warning :  Ce programme est compatible avec bash car ce dernier a ete develope sous cet environement.
 * le comportement peut varier legerement  selon le shell utilise  (csh,zsh, fish , nushell, elvish ...).
 * NOTE: Ce programme  peut servir de reference si vous voulez faire votre propre implementation       
 */

#define  _GNU_SOURCE 
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
#include <sys/mman.h> 
#include <sys/wait.h> 
#include <sys/stat.h> 
#include <sys/cdefs.h>
#include "version.h"  /** Auto Generer par Meson */ 



#if __has_attribute(warn_unused_result) 
# define __must_use  __attribute__((warn_unused_result)) 
#else  
# define __must_use /*  Nothing */ 
#endif 

#if __has_attribute(unused) 
# define  __notused __attribute__((unused)) 
#else 
# define  __notused 
#endif 

#if __has_attribute(noreturn) 
# define  __nortrn __attribute__((noreturn)) 
#else 
# define  __nortrn 
#endif 

#define _Nullable  

#if !defined(NDEBUG)
# define pr_dbg(...) \
  do{ printf("%s :",__func__); fprintf(stdout ,__VA_ARGS__);} while(0) 
#else  
# define  pr_dbg(...) /* ne fait rien  ... */ 
#endif 
#define _KIND_WARNING_MESG  "\011A utility to reveal real type of command. An alternative to unix 'type' command\012"


#define  BINARY_TYPE   (1 << 0)  
#define  BUILTIN_TYPE  (1 << 1)
#define  ALIAS_TYPE    (1 << 2)
#define  SHELL_KW_TYPE (1 << 3)  
/*Il se peut que parfois certains commandes se trouvant dans /usr/bin soient de scripts */
#define  SCRIPT_TYPE   (1 << 4)  

#define  BINARY_STR    "binary" 
#define  BUILTIN_STR   "builtin" 
#define  SCRIPT_STR    "script"  
#define  ALIAS_STR     "alias" 
#define  SHELL_KW_STR  "Shell keyword" 


#define  TYPE_STR(__type) \
   __type##_STR

#define  SHEBANG   0x212300           /* Pour la detection des scripts potentiel  */ 
#define  BASH_SIG  0x73616268         /* Signature pour Bash                      */
#define  ELF_SIG   0x4c457f46         /* Signature pour le executables            */
#define  MA_ALIAS  0x61696c6173       /* Pour les aliase                          */

/* Macro pour simplifier le matching entre signature */
#define  SIGMATCH(signature, strmatch ,lsize) ({\
    int s =  (lsize >>8 );\
    while(s <=(lsize &0xff))\
      signature&=~*(strmatch+ (s-1)) << (8*s),s=-~s;\
    })

#define perr_r(__fname , ...)\
  EXIT_FAILURE;do{puts(#__fname);fprintf(stderr , __VA_ARGS__); }while(0)   

#define  BASHRCS \
  ".bashrc",\
  "/etc/bash/bashrc"  /**Vous pouvez ajouter d'autres sources si vous voulez*/ 

char  bashrcs_sources[][0x14] = {
  BASHRCS,
  '\000'
}; 

/**  Ce bash_builtinkw  sera generer par le programme si il n'existe pas dans le repertoire /home/<user> */
#define BSH_DOT_FILE  ".bash_builtinkw"

#define  ALIAS_MAX_ROW 0xa
#define  ALIAS_STRLEN  0x64  
/**Par defaut  l'option de recherche est activer pour tous les types*/
static unsigned int option_search= BINARY_TYPE|ALIAS_TYPE|BUILTIN_TYPE| SHELL_KW_TYPE  | SCRIPT_TYPE ;  
typedef uint16_t pstat_t  ;  
typedef struct passwd   userinfo_t ; 

/* TODO : (Refactoring ... later) 
 *  faire un agregas de memoire  pour les regoupers. 
 **/
/** Contiendra toutes les aliases definis*/ 
char bash_aliases[ALIAS_MAX_ROW][ALIAS_STRLEN]= {0} ; 
char *has_alias = (char *)00  ; 
size_t dot_file_size  = 0 ; 
extern char ** environ ;
char * bkw_source  = (void*)00; 
/*information sur l'utilisateur */ 
userinfo_t *uid= 00;   

struct dotfile_info_t  { 
  ssize_t   df_size ; 
  char *df_source ; 
}; 
typedef struct shell_bltkw_t shbk_t  ; 
struct shell_bltkw_t { 
  struct dotfile_info_t  *df_info ;  
  /*
   * Ces variables  pointeront respectivement: 
   * vers les commandes builtins & les mot-cles du shell 
   **/ 
  char * shell_builtins; 
  char * shell_keywords;  
}; 

/* Ici  je fais un  type dite  'incomplet' 
 * qui est dans le scope global  et sert de masquage a mes structure de donnees  
 * */
struct sh_t* sh_t; /* Incomplet */ 

/**
 * Contient les informations sur la commande 
 * _cmd : represente la command elle meme 
 * _type : le type de command   si c'est un BINAIRE , BUILTIN  ou bien un ALIAS...
 * _path : la ou la commande se trouve 
 */
struct kind_info_t 
{
    char *_cmd ; 
    char _type;
    union {
      char * _path ; 
    } ;  
} ; 

   

void brief(struct kind_info_t  *cmd_info) ; 
struct  kind_info_t*  kind_search(const char * cmd_tgt , int search_option) ;  
static char *search_in_sysbin(struct   kind_info_t  *  cmd_info) ; 
/** Verifie  la signature de commandes */
static int looking_for_signature(const char *cmd_location  , int operation) ;  
/** Pre-Chargement des alias ... */
int  __preload_all_aliases(struct passwd * uid ) __must_use;   
/** Chargement des commandes builtin et les shell keywords*/
static struct  shell_bltkw_t * __load_shell_builtin_keywords(const char*  cmd) ;  
/*detection des alias depuis  les fichers bashrc  disponible */
static int load_alias_from(char (*)[ALIAS_STRLEN] , const off_t) ;

static char *looking_for_aliases(struct kind_info_t *   cmd_info  , int  mtdata) __must_use; 
static void  looking_for_builtin_cmd(struct kind_info_t*  __restrict__  cmd_info) ; 
static void  looking_for_shell_keyword(struct kind_info_t* __restrict__ cmd_info) ; 

static int spawn(const char * dot_file) ; 
static int memfd_exec(int fd) ; 
static size_t  inject_shell_statement(int fd); 
static char * map_dump(const char * dot_file) __must_use; 

static void check_compatibility_environment(void) ;  
static struct passwd * check_scope_action_for(struct passwd * user_id) ;  

void release(int rc , void *args) 
{
   struct kind_info_t *  info =  (struct kind_info_t *) args ; 
   if (!info) 
     return ; 

   if(info->_path) 
     free(info->_path) ; 
  
   free(info) ; 
   info = 00;  

   struct  shell_bltkw_t*  shinfo  = (struct shell_bltkw_t *) sh_t ;
  
   if(shinfo->df_info->df_source)  
      munmap(shinfo->df_info->df_source, shinfo->df_info->df_size );  
 
   free(shinfo->df_info),shinfo->df_info = 00; 
  
   if(shinfo->shell_builtins)  
     free(shinfo->shell_builtins), shinfo->shell_builtins=00; 

   free(shinfo),shinfo =  00 ; 
   
}

void kind_baseopts(const char * option, char  *const *av) 
{
  if(!strcmp(option ,  "--version")  || !(strcmp(option, "-v"))) 
  {
    printf("%s\012", KIND_VERSION); 
    exit(EXIT_SUCCESS)  ;
  }
  if(!strcmp(option, "--help") || !(strcmp(option , "-h")))  
  {
    printf("Usage : %s : <command>\012For more info see manpage (1)\012",basename(*(av)))    ; 
    exit(EXIT_SUCCESS);  
  }
} 

int main (int ac , char **av,  char **env) 
{
  pstat_t pstatus =EXIT_SUCCESS;
  (void) setvbuf(stdout , (char *)0 , _IONBF , 0) ; 
  
  /* A partir de la je  fais une serie de verification  comme  : 
   * l'environement  - le scope de l'utilisateur (uid et le shell) 
   * */
  check_compatibility_environment() ;
  uid =  check_scope_action_for(uid) ; 

  if (!(ac &~(1))) 
  {
    pstatus^=perr_r(kind ,_KIND_WARNING_MESG); 
    goto __eplg; 
  }

  char *target_command = (char *) *(av+(ac+~0));
  kind_baseopts(target_command , av);  

  char  dotfile_path[0x32]= {0} ;  
  sprintf(dotfile_path , "%s/%s",  uid->pw_dir, BSH_DOT_FILE) ;  
  sh_t =(struct  sh_t  *)__load_shell_builtin_keywords(dotfile_path); 
  
  int summary_stat = __preload_all_aliases(uid); 
  
  if( 0 >= (summary_stat >>  8) )  
   option_search&=~ALIAS_TYPE ; 
    
  option_search = (option_search << 8 ) |  summary_stat  ; 
  
  struct kind_info_t*  info =  kind_search(target_command ,  option_search) ;
  if(!info) 
  {
    pstatus^=perr_r(kind_search ,  "No able to retrieve information  for this '%s' command \012",  target_command); 
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
        *shenv  = secure_getenv("SHELL"); 

  if(!shenv) 
  {
    (void) perr_r(compatiblity_errror, "on %s Your Shell is not supported Yet",  __func__) ; 
    exit(EXIT_FAILURE) ;
  }
  
  while((void *)00  != (token = strtok(shenv , (const char []){0x2f, 00} ))) shname = (token), shenv=0;

  size_t slen=(1<<8|strlen(shname)) ; 

  sig|=BASH_SIG;   
  SIGMATCH(sig, shname, slen) ; 
  if(sig) printf("Not able to check shell signature for bash\012"); 

}

static struct passwd * check_scope_action_for(struct passwd * user_id)   
{
  char *username  =  secure_getenv("USER"); 
  if (!username)
  {
     perr_r(check_scope_action_for , "Username %s is not found \012", username) ; 
     exit(EXIT_FAILURE) ;
  }

  user_id = getpwnam(username) ; 
  if(!user_id ) 
  {
    perr_r(check_scope_action_for , "cause error due to  :%s\012", *__errno_location())  ;
    exit(EXIT_FAILURE)  ; 
  }
  
  /**je verifie  si c'est un utlilisateur normal*/ 
  if(0x3e8 > (user_id->pw_uid & 0xfff))  
  {
    perr_r(check_scope_action_for,"%i is restricted  : %s\012", user_id->pw_uid,strerror(EPERM) )  ;  
    exit(EXIT_FAILURE); 
  }
  return user_id ; 
}


int  __preload_all_aliases(struct  passwd *  uid)  
{
  off_t offset_index= ~0;  
  while(  00 != *(*(bashrcs_sources+ (++offset_index))) )    
  { 
     char  * shrc = (*(bashrcs_sources+(offset_index))) ; 
     switch(*(shrc) & 0xff) 
     {
       case  0x2e  :
         char user_home_profile[0x32] ={0}  ;  
         sprintf(user_home_profile ,  "%s/%s", uid->pw_dir,  shrc) ;  
         if(!(~0 ^ access(user_home_profile , F_OK))) 
           continue ; 
        
         memcpy((bash_aliases+offset_index) , user_home_profile , strlen(user_home_profile)) ;  
         break ; 
       case  0x2f  :
        memcpy(*(bash_aliases+offset_index)  , shrc , strlen(shrc)) ; 
        break ;  
     }
  } 

  return load_alias_from(bash_aliases , offset_index) ;

}

/*! TODO: Faire en sorte de rendre les aliases persistant  
 *        car faire de i/o trop souvent  me semble assez lourd.  
 *-- Later: **/
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
void brief(struct kind_info_t *  cmd_info) 
{

   char  typestr[0x32] ={0} ; 
   char  hint =0 ; 
   fprintf(stdout ,  "command\011: %s\012", cmd_info->_cmd);
   if (!cmd_info->_path  && !cmd_info->_type )   return ; 

#define  _Append(__typestr ,__str )({\
    size_t s  = strlen(__typestr) ;\
    strcat((__typestr+s), __str) ; \
    *(__typestr+(strlen(__typestr))) = 0x3a;  \
    })

   if (cmd_info->_type & BINARY_TYPE)   _Append(typestr, TYPE_STR(BINARY)) ; 
   if (cmd_info->_type & ALIAS_TYPE)    _Append(typestr, TYPE_STR(ALIAS)) ; 
   if (cmd_info->_type & BUILTIN_TYPE)  _Append(typestr, TYPE_STR(BUILTIN)) ; 
   if (cmd_info->_type & SHELL_KW_TYPE) _Append(typestr, TYPE_STR(SHELL_KW)) ;
   if (cmd_info->_type & SCRIPT_TYPE)   _Append(typestr, TYPE_STR(SCRIPT)); 
  
   fprintf(stdout,"Location:") ; 
   if ((cmd_info->_type  &(BINARY_TYPE | ALIAS_TYPE | SCRIPT_TYPE ))) 
     fprintf(stdout , " %s", cmd_info->_path ? cmd_info->_path :"Not Found");
     
   if (cmd_info->_type & BUILTIN_TYPE)  
     fprintf(stdout , " <is a shell builtin>");
   
   if((cmd_info->_type & SHELL_KW_TYPE))
     fprintf(stdout , " <is a shell keyword>") ; 

   if(cmd_info->_type & SCRIPT_TYPE)  
   {
     fprintf(stdout , " <is potentially a script>") ; 
     hint^=1 ;
   }
  
   puts("") ; 


   fprintf(stdout , "Type\011: [:%s]\012", (1 < strlen(typestr))? typestr: "Unknow:")  ; 

   if (has_alias)  
     fprintf(stdout , "Alias\011: %s", has_alias)  ; 

   if(hint) 
     /* Je laisse ce tips la: car la commande 'file' peut deja faire une investigation */
     fprintf(stdout , "Hint: Please Use 'file' command to investigate further\012") ;
}

struct kind_info_t *  kind_search(const char *  cmd_target ,  int search_option)
{
  struct kind_info_t * local_info = (struct kind_info_t*) malloc(sizeof(*local_info))  ; 
  if (!local_info) 
    return (struct kind_info_t*) 0 ; 

  memset(local_info  ,  00 , sizeof(*local_info)) ;  

  local_info->_cmd = (char *) cmd_target ;  
  unsigned   data = (search_option  &  0xff ) ; 
  search_option  >>=8 ;  
  
  if (search_option & ALIAS_TYPE)  
    has_alias =  looking_for_aliases(local_info, data ) ; 
  
  if (search_option & BINARY_TYPE )
    (void *)search_in_sysbin(local_info);  
  
  if(search_option  &  BUILTIN_TYPE) 
     looking_for_builtin_cmd(local_info) ;  
  
  if(search_option & SHELL_KW_TYPE)  
    looking_for_shell_keyword(local_info) ; 

 return local_info ; 
}

static char  * search_in_sysbin(struct kind_info_t * local_info) 
{
  if(!((*local_info->_cmd  & 0xff)  ^ 0x2e) && strlen(local_info->_cmd) == 1 ) 
    return   (char *) local_info ; 
  
  char *path_bins  = secure_getenv("PATH") ; 
  if(!path_bins)  
    return (void *) 0  ;   
  
  char *token =  (char *) 0 , 
       location[0x64] ={0} ,
       found = 0 ;  
  
  while((char  *)0 != (token = strtok(path_bins , (const char[]){0x3a , 00})) ) 
  {  
    if(path_bins)
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
  local_info->_type|= looking_for_signature(location, BINARY_TYPE | SCRIPT_TYPE) ; 

  return   (char *) local_info ;  
}


static int looking_for_signature(const char * location ,  int options) 
{ 
  if (!location) return 0 ;   

  char elf_header_sig[0xf] ={0} ; 
  ssize_t  size = 4 ; 
  unsigned  int  elf_check=0;
  int flags = 0 ; 
  FILE * bin =  fopen(location , "rb") ; 
  if(!bin) 
     return 0;  

  size ^=fread(elf_header_sig ,1  ,4 , bin); 
  assert(!(size)) ; 
  fclose(bin) ; 

  size^= -~(size); 
  if(options  & BINARY_TYPE) 
  {
    while( size <=4  ) 
      elf_check|=  *(elf_header_sig +size-1) << (8*size), size=-~size;   
  

    if(!(elf_check^ ELF_SIG))  
      return  BINARY_TYPE ;
  } 
  
  if(options & SCRIPT_TYPE) 
  { 
    elf_check&=~elf_check, size=1;   
    /*NOTE : Detection si  la command est un potantiel script */ 
    while(size <=2  ) 
      elf_check|= *(elf_header_sig+ (size-1))  <<(8*size), size=-~size ;  
   
    if(!(elf_check^SHEBANG)) 
      return SCRIPT_TYPE ; 
     
  }

  return 0;   
}



static char * looking_for_aliases(struct kind_info_t *  cmd_info , int mtadata ) 
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


static  struct shell_bltkw_t * __load_shell_builtin_keywords(const char* sh_dot_file)  
{ 

  struct shell_bltkw_t * shell_bkw = (struct shell_bltkw_t *) malloc(sizeof(*shell_bkw))  ;  
  if(!shell_bkw) 
    return(struct  shell_bltkw_t*) 00; 
  
  int   io_request = EACCES , 
        mfd=~0 ; 

  if(!(~0 ^ (access(sh_dot_file , F_OK))))  
    io_request&=~EACCES ; 
  
  if(!io_request)  
  {  
     if(spawn(sh_dot_file)) 
     {
        perr_r(spawn, "Not able to create  %s \012", BSH_DOT_FILE) ; 
        option_search &=~SHELL_KW_TYPE ; 
        free(shell_bkw ) ; 
        return (void *) 0 ; 
     } 
     io_request|=EACCES;    
  }

  shell_bkw->df_info = (struct dotfile_info_t*)  map_dump(sh_dot_file); 
  if(!shell_bkw->df_info) 
  {
    option_search &=~SHELL_KW_TYPE ; 
    return (struct  shell_bltkw_t *) 00 ;  
  }

  char *hash_mark  = strchr(shell_bkw->df_info->df_source , 0x23) ;  
  if(!hash_mark) 
  {
    option_search &=~SHELL_KW_TYPE; 
    return (struct shell_bltkw_t*) 00 ; 
  }  

  ssize_t  offsize = (hash_mark - shell_bkw->df_info->df_source);  

  shell_bkw->shell_builtins =(char *) strndup(shell_bkw->df_info->df_source, offsize+1) ; 
  shell_bkw->shell_keywords = hash_mark ; 

  return shell_bkw ; 
}

static char * map_dump(const char * dot_file) 
{ 
   struct  dotfile_info_t *  dfinfo  = (struct dotfile_info_t*) malloc(sizeof(*dfinfo)) ; 
   if(!dfinfo) 
     return  (char *) 0  ; 
  
   struct stat  sb ; 
   int fd= (~0 ^open(dot_file , O_RDONLY) ) ; 
   if(!fd) 
     return (void *)0 ;
   
   fd=~fd  ; 
   fstat(fd , &sb);  
   dfinfo->df_size =  sb.st_size  ; 
   dfinfo->df_source = (char *)mmap((void *)0 ,dfinfo->df_size, PROT_READ , MAP_PRIVATE , fd , 0) ; 
   close(fd) ; 
   if (MAP_FAILED ==   (dfinfo->df_source)) 
     return (void *)0 ;  

  
   return  (char *) dfinfo ; 
}
static int  spawn(const char * dot_file) 
{ 
  int mfd =~0, 
      getflags=0;  
  ssize_t bytes = 0 ; 
  mfd ^=  memfd_create("__anon__",MFD_CLOEXEC) ;
  if(!mfd)
  {
    //!TODO :  trouver une alternative au cas ou ca echoue ... (Pour le moment j'ai la flemme)  
    perr_r(memfd_create, " %s", strerror(*__errno_location())); 
    return  *__errno_location() ;  
  }

  mfd=~mfd ; 
     
  if(ftruncate(mfd , (sysconf(_SC_PAGESIZE)>>1) ))   
  {
    perr_r(ftruncate , "Fail to expend memory fd offset : %s \012 ", strerror(*__errno_location()))    ; 
    return   *__errno_location() ;  
  } 
     
  bytes = inject_shell_statement(mfd); 
  /** Une verification pour ne pas polluer la memoire,
   *  car l'injection du script  se fait  en memoire ne doit pas execeder les 2048 (soit 1/2  d'un block de page)
   **/
    
  if( (bytes &~((sysconf(_SC_PAGESIZE)>>1)-1)))  
  {
    perr_r(page_block_overflow , "%s : shell inject overflow > 2048 \012", __func__);  
    return  EOVERFLOW  ; 
  }
  getflags = fcntl(mfd ,  F_GETFD) ; 
  if(O_RDONLY != getflags  ) 
  {
    getflags=(getflags^getflags)  ; 
    getflags|=O_RDONLY ; 
    /* en lecture seul pour etre sur */ 
    (void) fcntl(mfd , F_SETFD , getflags);   
  }
  
  getflags = memfd_exec(mfd);
  if(getflags) 
     option_search&=~SHELL_KW_TYPE ; 
  
  return 0 ;
}
static size_t  inject_shell_statement(int fd) 
{  
   char var[0x50] ={0}  ;
   sprintf(var  , "declare -r bsh_bltkw=\"%s/%s\"\012",uid->pw_dir,BSH_DOT_FILE);  
   
   char *inline_statements[] = {
     "#!/bin/bash\012",
     var, 
     "compgen -b >  ${bsh_bltkw}\012",
     "echo -e '\043' >> ${bsh_bltkw}\012", 
     "compgen -k >> ${bsh_bltkw}\012",
     "echo -e 0 >> ${bsh_bltkw}",
     NULL 
   }; 

   char i =0 ;
   size_t bytes =  0; 
   while((void *)0  != *(inline_statements+i))
     bytes+=write(fd , *(inline_statements+i),strlen(*(inline_statements+i) )),i=-~i ;   
  
   lseek(fd,0,0);  
   return bytes; 
}

static int  memfd_exec(int fd)
{  

   pid_t  cproc=~0; 
   cproc ^=fork() ; 
   if(!cproc) 
     return  ~0 ; 
  
   cproc=~cproc  ;  
  
   if(!(0xffff &~(0xffff ^ cproc))) 
   {
     int status = fexecve(fd,
                          (char *const[]) {uid->pw_shell , (void *)0},
                          environ
                          ) ; 
     
     exit(errno) ;   
   }else{
     int s =0 ; 
     wait(&s) ; 
     return  s; 
   }
  
   return  ~0 ;  
}

static void looking_for_builtin_cmd(struct kind_info_t  *restrict  cmd )  
{
   char  bltw[0x14] ={0};
   size_t offsize = 0 ; 
   struct  shell_bltkw_t * shbkw = (struct shell_bltkw_t*)  sh_t ;  
   char  * copy = shbkw->shell_builtins;   

   while( (*shbkw->shell_builtins & 0xff )  ^ 0x23) 
   {
     char *lf = strchr(shbkw->shell_builtins , 0xa) ; 
     offsize =  lf -  shbkw->shell_builtins ; 
     memcpy(bltw , shbkw->shell_builtins ,  offsize) ;  
     if(!strcmp(cmd->_cmd , bltw)) 
     {
       cmd->_type|=BUILTIN_TYPE ;  
       break;  
     }
     shbkw->shell_builtins=(shbkw->shell_builtins+(++offsize)) ; 
     bzero(bltw, 0x14) ; 
   } 
   
   /*  Restauration de l'address */   
   shbkw->shell_builtins = copy ;  

} 

static void  looking_for_shell_keyword(struct kind_info_t * restrict  cmd) 
{
   struct shell_bltkw_t  * shbkw  = (struct shell_bltkw_t *) sh_t  ; 
  
   ssize_t offsize = 0 ; 
   char   shkw[0x14] = {0}, 
          *copy =   shbkw->shell_keywords ; 
   while((*shbkw->shell_keywords & 0xff)!= 0) 
   {
     char *lf = strchr(shbkw->shell_keywords , 0xa ) ; 
     offsize = (lf - shbkw->shell_keywords); 
    
     memcpy(shkw , shbkw->shell_keywords ,  offsize) ; 
     if(!strcmp(shkw , cmd->_cmd)) 
     {
       cmd->_type |=SHELL_KW_TYPE ;  
       break ; 
     }
    
     shbkw->shell_keywords = (shbkw->shell_keywords + (++offsize)) ;  
     bzero(shkw,  0x14) ;  
   }

    /*Restauration de l'address */ 
   shbkw->shell_keywords = copy ; 
} 
