PROG=	fat32
SRCS=	main.cpp requests.cpp fat32.cpp avl.cpp

DPADD+=	${LIBSYS}
LDADD+=	-lsys -ltimers

CPPFLAGS+=-D_SYSTEM -Wall -std=c++1z

.include <minix.service.mk>
