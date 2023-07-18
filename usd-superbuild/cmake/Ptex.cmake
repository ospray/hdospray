##
## Ptex
##

set(Ptex_ARGS
-DPTEX_VER=v2.3.1
)

set(Ptex_DEPENDENCIES "")

build_component(
  NAME Ptex
  VERSION "v2.3.1"
  URL https://github.com/wdas/ptex
  BUILD_ARGS ${Ptex_ARGS}
  DEPENDS_ON ${Ptex_DEPENDENCIES}
)