# menu-typo - typo-tolerant matching for dmenu and rofi
#
#   make                 rofi plugins (typorun.so, typowindow.so)
#   make test            unit tests of src/typo.h
#   make dmenu           dmenu with the patch (downloads the dmenu source)
#   make dmenu-patch     regenerate dmenu/dmenu-typo-$(DMENU_VERSION).diff (needs git)
#   make install         rofi plugins, and dmenu-typo if it was built
#   make install-rofi    rofi plugins only
#   make install-dmenu   dmenu-typo only (run make dmenu first)
#   make clean

include config.mk

B = build
PLUGINS = $(B)/typorun.so $(B)/typowindow.so
DMENU_DIR = $(B)/dmenu-$(DMENU_VERSION)
DMENU_TAR = $(B)/dmenu-$(DMENU_VERSION).tar.gz
PATCH = dmenu/dmenu-typo-$(DMENU_VERSION).diff
DMENU_GIT_DIR = $(B)/dmenu-git

all: rofi

rofi: $(PLUGINS)

$(B)/%.so: rofi/%.c rofi/common.h src/typo.h config.mk
	@mkdir -p $(B)
	$(CC) $(CFLAGS) -std=gnu99 $(ROFI_CFLAGS) -shared -o $@ $< $(ROFI_LIBS)

test: $(B)/test_typo
	$(B)/test_typo

$(B)/test_typo: tests/test_typo.c src/typo.h
	@mkdir -p $(B)
	$(CC) $(CFLAGS) -o $@ tests/test_typo.c

$(DMENU_TAR):
	@mkdir -p $(B)
	curl -fsSL -o $@.part $(DMENU_URL)
	echo "$(DMENU_SHA256)  $@.part" | sha256sum -c -
	mv $@.part $@

# The patch for dmenu, as published on the suckless wiki: git format-patch of
# one commit on top of the release tag, made from dmenu/*.diff and
# src/typo.h. It is kept in the repository, so building dmenu does not need
# git or this step; the CI checks that it is up to date.
dmenu-patch:
	rm -rf $(DMENU_GIT_DIR)
	git -c advice.detachedHead=false clone -q --depth 1 --branch $(DMENU_VERSION) \
		$(DMENU_GIT) $(DMENU_GIT_DIR)
	cd $(DMENU_GIT_DIR) && for f in dmenu.c config.def.h dmenu.1; do \
		patch -s -p1 < ../../dmenu/$$f.diff || exit 1; done
	cp src/typo.h $(DMENU_GIT_DIR)/typo.h
	cd $(DMENU_GIT_DIR) && git add -A && \
		GIT_AUTHOR_DATE="$(PATCH_DATE)" GIT_COMMITTER_DATE="$(PATCH_DATE)" \
		git -c user.name="$(PATCH_NAME)" -c user.email="$(PATCH_EMAIL)" \
		commit -q -F ../../dmenu/commit-message.txt && \
		git format-patch --stdout HEAD^ > ../../$(PATCH)

dmenu: $(DMENU_DIR)/dmenu

$(DMENU_DIR)/dmenu: $(DMENU_TAR) $(PATCH)
	rm -rf $(DMENU_DIR)
	mkdir -p $(DMENU_DIR)
	tar -xzf $(DMENU_TAR) -C $(DMENU_DIR) --strip-components=1
	cd $(DMENU_DIR) && patch -s -p1 < ../../$(PATCH) && $(MAKE)

install: install-rofi
	if [ -x $(DMENU_DIR)/dmenu ]; then $(MAKE) install-dmenu; fi

install-rofi: rofi
	install -Dm644 $(B)/typorun.so $(DESTDIR)$(ROFI_PLUGINDIR)/typorun.so
	install -Dm644 $(B)/typowindow.so $(DESTDIR)$(ROFI_PLUGINDIR)/typowindow.so
	install -Dm644 README.md $(DESTDIR)$(PREFIX)/share/doc/rofi-typo/README.md
	install -Dm644 LICENSE $(DESTDIR)$(PREFIX)/share/licenses/rofi-typo/LICENSE

# dmenu-typo next to the regular dmenu (dmenu_run and dmenu_path stay
# theirs); dmenu's own license goes with it.
install-dmenu:
	test -x $(DMENU_DIR)/dmenu
	install -Dm755 $(DMENU_DIR)/dmenu $(DESTDIR)$(PREFIX)/bin/dmenu-typo
	install -Dm644 $(DMENU_DIR)/dmenu.1 $(DESTDIR)$(PREFIX)/share/man/man1/dmenu-typo.1
	install -Dm644 README.md $(DESTDIR)$(PREFIX)/share/doc/dmenu-typo/README.md
	install -Dm644 LICENSE $(DESTDIR)$(PREFIX)/share/licenses/dmenu-typo/LICENSE
	install -Dm644 $(DMENU_DIR)/LICENSE $(DESTDIR)$(PREFIX)/share/licenses/dmenu-typo/LICENSE.dmenu

uninstall:
	rm -f $(DESTDIR)$(ROFI_PLUGINDIR)/typorun.so $(DESTDIR)$(ROFI_PLUGINDIR)/typowindow.so
	rm -f $(DESTDIR)$(PREFIX)/bin/dmenu-typo $(DESTDIR)$(PREFIX)/share/man/man1/dmenu-typo.1
	rm -rf $(DESTDIR)$(PREFIX)/share/doc/rofi-typo $(DESTDIR)$(PREFIX)/share/licenses/rofi-typo
	rm -rf $(DESTDIR)$(PREFIX)/share/doc/dmenu-typo $(DESTDIR)$(PREFIX)/share/licenses/dmenu-typo

clean:
	rm -rf $(B)

.PHONY: all rofi test dmenu dmenu-patch install install-rofi install-dmenu uninstall clean
