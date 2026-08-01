#
# This file is the zyrenith-capture recipe.
#

SUMMARY = "Simple zyrenith-capture application"
SECTION = "PETALINUX/apps"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "file://zyrenith-capture.c \
	   file://video_capture.c \
	   file://video_capture.h \
	   file://storage.c \
	   file://storage.h \
	   file://Makefile \
		  "

S = "${WORKDIR}"

do_compile() {
	     oe_runmake
}

do_install() {
	     install -d ${D}${bindir}
	     install -m 0755 zyrenith-capture ${D}${bindir}
}
