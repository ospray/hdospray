#/bin/bash
clang-format -style=file -i -fallback-style=none $(git ls-files *.c *.cpp *.h *.hpp)
