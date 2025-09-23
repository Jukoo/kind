# Copyright(c) 2025  Umar Ba <jUmarB@protonmail.com> e
project('kind', 'c',
  version : '0.1',
  license:['GPL-2.O-only', 'GPL-2.0-or-later'],
  meson_version: '>=1.6.1',
  default_options : ['c_std=gnu99', 'warning_level=3'])

# TODO : Translation detection (for later) 
#il8n = import('i18n')  

cc = meson.get_compiler('c') 

add_project_arguments(
  cc.get_supported_arguments( 
    '-Wformat',
    '-Wunused-variable',
    '-Wunused-parameter',
    '-Wparentheses',
    '-Wpedantic'
    ),
  language:'c'
  )


############
# man page #
############
install_man(['manpage/kind.1'])

executable('kind',
           'kind.c',
           install : true)
