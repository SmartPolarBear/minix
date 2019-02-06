PROG=fat32utils
SRCS=fat32.cpp main.cpp
MAN=fat32utils.1

DPADD+=	${LIBSYS}
LDADD+=	-lsys -ltimers -lc++

CPPFLAGS += -D_SYSTEM -Wall -g -std=c++11

.include <bsd.prog.mk>