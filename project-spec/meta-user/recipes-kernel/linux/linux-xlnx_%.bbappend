FILESEXTRAPATHS:prepend := "${THISDIR}/${PN}:"

SRC_URI:append = " file://bsp.cfg"
KERNEL_FEATURES:append = " bsp.cfg"
SRC_URI += "file://user_2023-05-28-21-16-00.cfg \
            file://user_2023-05-28-23-18-00.cfg \
            "

