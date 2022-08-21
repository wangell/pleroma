#!/usr/bin/env python3

import sys, os
import glob
import subprocess

for test_file in glob.glob("tests/*"):
    # We expect a failure
    success = False
    output_code = subprocess.run("./pleroma test {}".format(test_file), shell = True).returncode

    if "fail" in test_file:
        success = output_code != 0
    else:
        success = output_code == 0

    if success:
        print("\033[1;32mSuccess:\033[0m {}\n".format(test_file))
    else:
        print("\033[1;31mFailed:\033[0m {}\n".format(test_file))
