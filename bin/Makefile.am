ACLOCAL_AMFLAGS = -I m4

SUBDIRS =

if WITH_CURL
if WITH_MICROHTTPD

if WITH_FUSE
SUBDIRS += fuse
endif

if WITH_LEGACY_FUSE
SUBDIRS += fuse
endif

if WITH_QT
if WITH_CLOUDBROWSER
SUBDIRS += cloudbrowser
endif
endif

endif
endif
