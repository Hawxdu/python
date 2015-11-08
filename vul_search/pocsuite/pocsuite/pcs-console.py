#!/usr/bin/env python
#-*- coding:utf-8 -*-

"""
Copyright (c) 2014-2015 pocsuite developers (http://sebug.net)
See the file 'docs/COPYING' for copying permission
"""

import sys
from pocsuite import modulePath
from lib.core.consoles import baseConsole
from lib.core.data import kb
from lib.core.data import paths
from lib.core.common import setPaths
from lib.core.consoles import initializePoc
from lib.core.option import initializeKb


if __name__ == "__main__":
    folders, sys.argv = sys.argv[1:], sys.argv[:1]

    
    paths.POCSUITE_ROOT_PATH = modulePath()
    setPaths()
    kb.unloadedList = {}

    initializeKb()
    initializePoc(folders)

    pcs = baseConsole()
    pcs.cmdloop()
