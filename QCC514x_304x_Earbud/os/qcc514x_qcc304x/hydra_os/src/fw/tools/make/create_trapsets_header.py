#!/usr/bin/env python
"""
Copyright (c) 2018-2021 Qualcomm Technologies International, Ltd.
  %%version
"""

import sys
import argparse

from hydra_os_util import create_trapset_h

def main():
    """Script to run"""
    parser = argparse.ArgumentParser()
    parser.add_argument("build_defs", help="path to the input build_defs.h file.")
    parser.add_argument("trapset_header", help="path to the output trapset.h file.")
    args = parser.parse_args()
    create_trapset_h(args.build_defs, args.trapset_header)


if __name__ == "__main__":
    main()
