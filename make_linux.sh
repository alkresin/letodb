#!/bin/bash
if ! [ -e lib ]; then
   mkdir lib
   chmod a+w+r+x lib
fi
if ! [ -e obj ]; then
   mkdir obj
   chmod a+w+r+x obj
fi
if ! [ -e obj/linux ]; then
   mkdir obj/linux
   chmod a+w+r+x obj/linux
fi
make -fMakefile.linux >a1.log 2>a2.log $1
