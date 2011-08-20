PROG=		yam
SRCS=		do.c		\
		err.c		\
		graph.c 	\
		ipc.c		\
		log.c		\
		main.c		\
		subprocess.c 	\
		yamfile.c

CFLAGS+=	-I../contrib

OPSYS!=		uname
.if ${OPSYS} == "FreeBSD"
CFLAGS+=	-DFREEBSD
CFLAGS+=	`pkg-config --cflags lua-5.1`
LDFLAGS+=	`pkg-config --libs lua-5.1`
WARNS=		3
.elif ${OPSYS} == "Linux"
CFLAGS+=	-DLINUX
CFLAGS+=	`pkg-config --cflags lua5.1`
LDFLAGS+=	`pkg-config --libs lua5.1`
WARNS=		0
.endif

NO_MAN=		yes
BINDIR=		/usr/local/bin
DEBUG_FLAGS=	-g -O0

.include <bsd.prog.mk>
