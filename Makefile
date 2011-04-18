PROG=		yam
SRCS=		do.c		\
		err.c		\
		graph.c 	\
		ipc.c		\
		main.c		\
		subprocess.c 	\
		yamfile.c

WARNS=		3

CFLAGS+=	-I/usr/local/include/lua51
LDFLAGS+=	-L/usr/local/lib/lua51 -lm -llua

NO_MAN=		yes
BINDIR=		/usr/local/bin
DEBUG_FLAGS=	-g -O0

.include <bsd.prog.mk>
