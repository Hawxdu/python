#!/usr/bin/env python
# -*- coding: utf-8 -*-

"""
Copyright (c) 2014-2015 pocsuite developers (http://sebug.net)
See the file 'docs/COPYING' for copying permission
"""

import sys
from pocsuite import pcsInit
from lib.core.settings import PCS_OPTIONS
from lib.core.common import banner
from lib.core.common import dataToStdout


if __name__ == "__main__":

    try:
        pocFile, targetUrl = sys.argv[1: 3]
    except ValueError:
        excMsg = "usage: python pcs-verify.py [pocfile] [url]\n"
        excMsg += "pocsuite: error: too few arguments"
        dataToStdout(excMsg)
        sys.exit(1)

    PCS_OPTIONS.update({'url': targetUrl, 'pocFile': pocFile})
    pcsInit(PCS_OPTIONS)
    banner()
