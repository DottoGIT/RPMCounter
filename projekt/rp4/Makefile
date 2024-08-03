include $(TOPDIR)/rules.mk

PKG_NAME:=SKPS
PKG_VERSION:=1.0
PKG_RELEASE:=1


PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)
include $(INCLUDE_DIR)/package.mk

define Package/${PKG_NAME}
  SECTION:=MSLS
  CATEGORY:=Examples
  TITLE:=Skps
endef

define Package/${PKG_NAME}/description
	SKPS project package - tracking velocity of a spinning wheel
endef

define Build/Compile
	$(MAKE) -C $(PKG_BUILD_DIR) clean
	$(MAKE) -C $(PKG_BUILD_DIR) CC="$(TARGET_CC)" CFLAGS="$(TARGET_CFLAGS)" LDFLAGS="$(TARGET_LDFLAGS)"
endef

define Package/${PKG_NAME}/install
	# TODO
endef