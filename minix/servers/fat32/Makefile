PROG=	fat32
SRCS=	main.cpp requests.cpp fat32.cpp avl.cpp

DPADD+=	${LIBSYS}
LDADD+=	-lsys -ltimers -fno-rtti

CPPFLAGS+=-D_SYSTEM -Wall -std=c++14 -fno-rtti

.include <minix.service.mk>
