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
#define _KIND_WARNING_MESG  "A utility to reveal real type of command. An alternative to unix 'type' command\012"

#define  BINARY_TYPE  (1 << 0)  
#define  BINARY_STR   "binary" 
#define  BUILTIN_TYPE (1 << 1)
#define  BUILTIN_STR  "builtin" 
#define  ALIAS_TYPE   (1 << 2)
#define  ALIAS_STR    "alias" 
#define  SHELL_KW_TYPE (1<< 3) 
#define  SHELL_KW_STR  "Shell keyword" 
/*Il se peut que parfois certains commandes se trouvant dans /usr/bin soient de scripts */
#define  SCRIPT_TYPE (1<<4) 
#define  SCRIPT_STR "script"  

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

/*
 * Ces variables  pointeront respectivement: 
 * vers les commandes builtins & les mot-cles du shell 
 **/ 
char __notused *shell_builtin=(char*)00,  
     __notused *shell_keyword=(char*)00; 


#define  ALIAS_MAX_ROW 0xa
#define  ALIAS_STRLEN  0x64  
/**Par defaut  l'option de recherche est activer pour tous les types*/
static unsigned int option_search= BINARY_TYPE|ALIAS_TYPE|BUILTIN_TYPE| SHELL_KW_TYPE  ;  
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
static int __load_shell_builtin_keywords(const char*  cmd) ;  
/*detection des alias depuis  les fichers bashrc  disponible */
static int load_alias_from(char (*)[ALIAS_STRLEN] , const off_t) ;

static char *looking_for_aliases(struct kind_info_t *   cmd_info  , int  mtdata) __must_use; 
static void  looking_for_builtin_cmd(struct kind_info_t*  cmd_info   , const char *shkw_builtin) ;  
static void  looking_for_shell_keyword(struct kind_info_t * cmd_info , const char *shkw_builtin) ; 

static int spawn(const char * dot_file) ; 
static int memfd_exec(int fd) ; 
static size_t  inject_shell_statement(int fd); 
static char * map_dump(const char * dot_file) __must_use; 

static void check_compatibility_environment(void) ;   
void release(int rc , void *args) 
{
   struct kind_info_t *  info =  (struct kind_info_t *) args ; 
   if (!info) 
     return ; 
  
   if(info->_path) 
     free(info->_path) ; 
  
   free(info) ; 
   info = 0  ; 
  
   if(bkw_source)  
    munmap(bkw_source , dot_file_size), bkw_source= 0; 
}
static struct passwd * check_scope_action_for(struct passwd * user_id) ;  
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
  char  dotfile_path[0x32]= {0} ;  
  sprintf(dotfile_path , "%s/%s",  uid->pw_dir, BSH_DOT_FILE) ;  
  __load_shell_builtin_keywords(dotfile_path); 
  
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
    perr_r(check_scope_action_for , "%i  due to  :%s\012", *__errno_location())  ;
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

  local_info->_cmd = (char *) cmd_target ; 
  unsigned   data = (search_option  &  0xff ) ; 
  search_option  >>=8 ;  
  
  if (search_option & ALIAS_TYPE)  
    has_alias =  looking_for_aliases(local_info, data ) ; 
  
  if (search_option & BINARY_TYPE )
    (void *)search_in_sysbin(local_info);  
  
  if(search_option  &  BUILTIN_TYPE) 
     looking_for_builtin_cmd(local_info  , bkw_source) ;  
  
  if(search_option & SHELL_KW_TYPE)  
     /** TODO : FOR SHELL KEY WORDS  */  
    looking_for_shell_keyword(local_info , bkw_source) ;  
  
  return local_info ; 
}

static char  * search_in_sysbin(struct kind_info_t * local_info) 
{ 
  char *path_bins  = secure_getenv("PATH") ; 
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
    
    flags  = !(elf_check^ELF_SIG) ? BINARY_TYPE : 0 ; 
  } 
  
  if(options & SCRIPT_TYPE && !flags) 
  { 
    elf_check&=~elf_check, size=1;   
    /*TODO : Detection si  la command est un potantiel script */ 
    while(size <=2  ) 
      elf_check|= *(elf_header_sig+ (size-1))  <<(8*size), size=-~size ;  
    
    flags = !(elf_check ^ SHEBANG)  ? SCRIPT_TYPE : 0 ; 

  }

  return flags ;  
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


static int  __load_shell_builtin_keywords(const char* sh_dot_file)  
{ 
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
        return EPERM; 
     } 
     io_request|=EACCES;    
  }

  bkw_source=map_dump(sh_dot_file);   
 
  return 0 ; 
}

static char * map_dump(const char * dot_file) 
{ 

   int fd =  ~0 ; 
   fd^=open(dot_file , O_RDONLY) ; 
   if(!fd) 
     return (void *)0 ;
   
   fd=~fd  ; 
   struct stat  sb ; 
   fstat(fd , &sb);  
   dot_file_size  =  sb.st_size  ; 
   bkw_source = (char *)mmap((void *)0 ,sb.st_size , PROT_READ , MAP_PRIVATE , fd , 0) ; 
   close(fd) ; 
   if (MAP_FAILED ==   bkw_source) 
     return (void *)0 ;  

   return  bkw_source ;  
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


static void looking_for_builtin_cmd(struct kind_info_t * cmd , const char  * shbkw)  
{
  
  char builtin_cmd[0x14]={0} , i = 0 , 
       *builtins  = (char *) shbkw ;   
 
  while ( ((*builtins & 0xff) !=  0x23))
  {
     char *linefeed  = strchr(builtins, 0xa);  
     if (linefeed) 
        i =  linefeed -  builtins;
     
     memcpy(builtin_cmd ,   builtins ,  i ) ; 

     if(!strcmp(cmd->_cmd , builtin_cmd))
     {
       cmd->_type|=BUILTIN_TYPE ; 
       return ;  
     }
      
     builtins = (builtins+(i+1)) ;   
     bzero(builtin_cmd  , i ) ; 
  }
 

}

static void  looking_for_shell_keyword(struct kind_info_t * cmd , const char * shbkw) 
{

  char *shell_kw = strchr(shbkw ,  0x23) , 
       kword[0x14] = {0} ; 
  if(!shell_kw) 
  {
     option_search&=~SHELL_KW_TYPE ;  
     return ; 
  }
  shell_kw=(shell_kw+2) ; 
  int index_jmp  = 0 ;   
 
  while( (0x30^ (*(shell_kw) & 0xff ))) 
  {
    char *lf = strchr(shell_kw  ,012)  ; 
    if(!lf) 
      break ; 
    
    index_jmp = (lf - shell_kw) ;
    memcpy(kword ,  shell_kw , index_jmp) ; 
    
    if(!strcmp(cmd->_cmd, kword)) 
    {
       cmd->_type|=SHELL_KW_TYPE ; 
       return ; 
    }
    bzero(kword , 0x14);
    shell_kw =(shell_kw+(index_jmp+1)) ; 
  }
}
