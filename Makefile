# menu-typo - typo-tolerant matching for dmenu and rofi
#
#   make                 rofi plugins (typorun.so, typowindow.so)
#   make test            unit tests of src/typo.h
#   make dmenu           dmenu with the patch (downloads the dmenu source)
#   make dmenu-patch     self-contained patch dmenu-typo-$(DMENU_VERSION).diff
#   make install         rofi plugins, and dmenu-typo if it was built
#   make install-rofi    rofi plugins only
#   make install-dmenu   dmenu-typo only (run make dmenu first)
#   make clean

include config.mk

B = build
PLUGINS = $(B)/typorun.so $(B)/typowindow.so
DMENU_DIR = $(B)/dmenu-$(DMENU_VERSION)
DMENU_TAR = $(B)/dmenu-$(DMENU_VERSION).tar.gz
PATCH = dmenu-typo-$(DMENU_VERSION).diff

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

# Pristine source in $(B)/a, patched in $(B)/b; the patch is their difference
# plus src/typo.h as a new file, so it applies to a clean dmenu with patch -p1.
dmenu-patch: $(PATCH)

$(PATCH): $(DMENU_TAR) dmenu/dmenu.c.diff dmenu/config.def.h.diff src/typo.h
	rm -rf $(B)/a $(B)/b
	mkdir -p $(B)/a $(B)/b
	tar -xzf $(DMENU_TAR) -C $(B)/a --strip-components=1
	tar -xzf $(DMENU_TAR) -C $(B)/b --strip-components=1
	cd $(B)/b && patch -s -p1 < ../../dmenu/dmenu.c.diff && patch -s -p1 < ../../dmenu/config.def.h.diff
	cp src/typo.h $(B)/b/typo.h
	cd $(B) && diff -ruN a b | sed -E '/^(---|\+\+\+) /s/\t.*//' > ../$@ || true
	@test -s $@

dmenu: $(DMENU_DIR)/dmenu

$(DMENU_DIR)/dmenu: $(PATCH)
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
	rm -rf $(B) $(PATCH)

.PHONY: all rofi test dmenu dmenu-patch install install-rofi install-dmenu uninstall clean
