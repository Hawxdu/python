#!/usr/bin/env python
# -*- coding: utf-8 -*-

"""
Copyright (c) 2014-2015 pocsuite developers (http://sebug.net)
See the file 'docs/COPYING' for copying permission
"""

import argparse
import os
from lib.core.data import defaults
from lib.core.settings import INDENT, USAGE, VERSION


def parseCmdOptions():
    """
    @function parses the command line parameters and arguments
    """

    parser = argparse.ArgumentParser(usage=USAGE, formatter_class=argparse.RawTextHelpFormatter, add_help=False)

    parser.add_argument("-h", "--help", action="help",
                        help="Show help message and exit")

    parser.add_argument("--version", action="version",
                        version=VERSION, help="Show program's version number and exit")

    target = parser.add_argument_group('target')

    target.add_argument("-u", "--url", dest="url",
                        help="Target URL (e.g. \"http://www.targetsite.com/\")")

    target.add_argument("-f", "--file", action="store", dest="urlFile",
                        help="Scan multiple targets given in a textual file")

    target.add_argument("-r", dest="pocFile", required=True,
                        help="Load POC from a file (e.g. \"_0001_cms_sql_inj.py\") or directory (e.g. \"modules/\")")

    mode = parser.add_argument_group('mode')

    mode.add_argument("--verify", dest="Mode", default='verify', action="store_const", const='verify',
                      help="Run poc with verify mode")

    mode.add_argument("--attack", dest="Mode", action="store_const", const='attack',
                      help="Run poc with attack mode")

    request = parser.add_argument_group('request')

    request.add_argument("--cookie", dest="cookie",
                         help="HTTP Cookie header value")

    request.add_argument("--referer", dest="referer",
                         help="HTTP Referer header value")

    request.add_argument("--user-agent", dest="agent",
                         help="HTTP User-Agent header value")

    request.add_argument("--random-agent", dest="randomAgent", action="store_true", default=False,
                         help="Use randomly selected HTTP User-Agent header value")

    request.add_argument("--proxy", dest="proxy",
                         help="Use a proxy to connect to the target URL")

    request.add_argument("--proxy-cred", dest="proxyCred",
                         help="Proxy authentication credentials (name:password)")

    request.add_argument("--timeout", dest="timeout",
                         help="Seconds to wait before timeout connection (default 30)")

    request.add_argument("--headers", dest="headers",
                         help="Extra headers (e.g. \"Accept-Language: zh-CN,zh;q=0.8\")")

    optimization = parser.add_argument_group("optimization")

    optimization.add_argument("--threads", dest="threads", type=int, default=1,
                              help="Max number of concurrent HTTP(s) requests (default %d)" % defaults.threads)

    optimization.add_argument("--report", dest="report",
                              help="Save a html report to file (e.g. \"./report.html\")")


    args = parser.parse_args()
    return args.__dict__


# def _format_help(help_info, choices=None):
    # if isinstance(help_info, list):
    #help_str_list = help_info[:]
    # else:
    #help_str_list = [help_info]

    # if choices:
    #help_str_list.extend(['%s%s - %s' % (INDENT, k, v) for k, v in choices.items()])

    #help_str_list.append(INDENT + '(DEFAULT: %(default)s)')

    # return os.linesep.join(help_str_list)
