PROG=	fat32
SRCS=	main.cpp requests.cpp fat32.cpp

DPADD+=	${LIBSYS}
LDADD+=	-lsys -ltimers

CPPFLAGS+=-D_SYSTEM -Wall -std=c++14

.include <minix.service.mk>
