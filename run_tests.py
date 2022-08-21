#!/usr/bin/env python3

import sys, os
import glob
import subprocess

for test_file in sorted(glob.glob("tests/*")):
    # We expect a failure
    success = False
    output = subprocess.run("./pleroma test {}".format(test_file), shell = True, capture_output = True)
    output_code = output.returncode

    if "fail" in test_file:
        success = output_code != 0
    else:
        success = output_code == 0

    if success:
        print("\033[1;32mSuccess:\033[0m {}".format(test_file))
    else:
        print("\033[1;31mFailed:\033[0m {}".format(test_file))
        print("\t" + str(output.stdout, 'utf-8'))
        print("\t" + str(output.stderr, 'utf-8'))
