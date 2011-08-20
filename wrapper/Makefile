SHLIB_NAME=	libwrp.so
SRCS=		wrapper.c


OPSYS!=		uname
.if ${OPSYS} == "FreeBSD"
WARNS=		6
.elif ${OPSYS} == "Linux"
WARN=		0
.endif

CFLAGS+=	-I../contrib
LDFLAGS+=	-lpthread

LIBDIR=		/usr/local/lib

.include <bsd.lib.mk>
