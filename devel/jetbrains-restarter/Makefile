PORTNAME=	restarter
DISTVERSION=	233.14015.106
PORTREVISION=	2
CATEGORIES=	devel
PKGNAMEPREFIX=	jetbrains-
DIST_SUBDIR=	jetbrains

MAINTAINER=	dmitry.wagin@ya.ru
COMMENT=	IntelliJ cross-platform restarter
WWW=		https://github.com/JetBrains/intellij-community/tree/master/native/restarter

LICENSE=	APACHE20
LICENSE_FILE=	${WRKSRC}/../../LICENSE.txt

ONLY_FOR_ARCHS=	aarch64 amd64 powerpc64le

USES=		cargo

USE_GITHUB=	yes
GH_ACCOUNT=	JetBrains
GH_PROJECT=	intellij-community
GH_TAGNAME=	1b8c945fe9801a1129e4bd03f6feda5ceae1804c

DATADIR=	${PREFIX}/share/jetbrains/${PORTNAME}
WRKSRC_SUBDIR=	native/restarter
PLIST_FILES=	share/jetbrains/${PORTNAME}/restarter

do-install:
	${MKDIR} ${STAGEDIR}${DATADIR}
	${INSTALL_PROGRAM} \
		${CARGO_TARGET_DIR}/${CARGO_BUILD_TARGET}/*/restarter \
		${STAGEDIR}${DATADIR}

post-install:
	${STRIP_CMD} ${STAGEDIR}${DATADIR}/restarter

.include <bsd.port.mk>
