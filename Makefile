include $(TOPDIR)/rules.mk

PKG_NAME:=OpenWrt_NXT_Server

PKG_RELEASE:=1

PKG_BUILD_DIR := $(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk

define Package/OpenWrt_NXT_Server
	SECTION:=utils
	CATEGORY:=Utilities
	TITLE:=NXT Server
	DEPENDS:=libusb-1.0
endef

define Package/OpenWrt_NXT_Server/description
	OpenWrt NXT Server
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Package/openwrt_NXT_Server/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/OpenWrt_NXT_Server $(1)/usr/sbin/
endef

$(eval $(call BuildPackage,OpenWrt_NXT_Server))
