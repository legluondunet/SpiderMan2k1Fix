#ifndef SPIDERMAN2K1FIX_VERSION_INFO_H
#define SPIDERMAN2K1FIX_VERSION_INFO_H

/* Semantic version; build.sh appends the exact source commit for traceability. */
#define SPIDERMAN2K1FIX_VERSION "1.0.0"
#ifndef SPIDERMAN2K1FIX_COMMIT
#define SPIDERMAN2K1FIX_COMMIT "local"
#endif
#define SPIDERMAN2K1FIX_VERSION_LABEL \
  "SpiderMan2k1Fix v" SPIDERMAN2K1FIX_VERSION "-" SPIDERMAN2K1FIX_COMMIT

#endif
