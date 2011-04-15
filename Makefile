PROG=		yam
SRCS=		main.c	\
		graph.c \
		subprocess.c \
		yamfile.c

WARNS=		3

CFLAGS+=	-I/usr/local/include/lua51
LDFLAGS+=	-L/usr/local/lib/lua51 -lm -llua

NO_MAN=		yes
DEBUG_FLAGS=	-g -O0

.include <bsd.prog.mk>
