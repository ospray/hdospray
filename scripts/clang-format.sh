#/bin/bash
## Copyright 2019 Intel Corporation
## SPDX-License-Identifier: Apache-2.0
clang-format -style=file -i -fallback-style=none $(git ls-files *.c *.cpp *.h *.hpp)
