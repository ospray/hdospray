#!/bin/bash -x
## Copyright 2019 Intel Corporation
## SPDX-License-Identifier: Apache-2.0

a=$(compare -metric AE -fuzz 50% $1 $2 /tmp/compare.png 2>&1)
echo "result: $a"
if [ "$((a))" -lt 12 ]; then
    exit 0
else
    exit 1
fi
