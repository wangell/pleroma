#!/bin/bash

for f in tests/*.po
do
	./pleroma test $f
done
