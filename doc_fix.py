#!/usr/bin/python3
# -*- coding: utf-8 -*-

""" This script postprocesses doxygen output to change dot graphs to use rankdir="LR". """
import os

for path,dirs,files in os.walk("./doc"):
    for f in files:
        if len(f) > 5: # prevent index out of range error
            if f[-4:] == ".dot":
                source = os.path.join(path,f.replace(".dot",""))
                cmdTpl = 'dot -Grankdir="LR" -Tpng -o<source>.png -Tcmapx -o<source>.map <source>.dot'
                cmd = cmdTpl.replace("<source>",source)
                print(f"Running dot for {source}")
                os.system(cmd)
