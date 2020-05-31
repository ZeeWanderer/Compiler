#!/usr/bin/python3
# -*- coding: utf-8 -*-

""" This script postprocesses a doxygen output to change the dot graphs having rankdir="LR"."""
import os

for path,dirs,files in os.walk("./doc"):
    for f in files:
        if len(f) > 5: # prevent index out of range error
            if f[-4:] == ".dot":
                source = os.path.join(path,f.replace(".dot",""))
                cmdTpl = 'dot -Grankdir="LR" -Tpng -o<source>.png -Tcmapx -o<source>.map <source>.dot'
                cmd = cmdTpl.replace("<source>",source)
                os.system(cmd)