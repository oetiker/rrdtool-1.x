# Included after the binding's generated Makefile so custom installation
# directories and the current Perl architecture match the installer.
RRDTOOL_PACKLIST_perl = $(DESTINSTALLARCHLIB)
RRDTOOL_PACKLIST_site = $(DESTINSTALLSITEARCH)
RRDTOOL_PACKLIST_vendor = $(DESTINSTALLVENDORARCH)

.PHONY: rrdtool-uninstall
rrdtool-uninstall:
	$(PERL) "$(RRDTOOL_UNINSTALL_SCRIPT)" \
	  "$(DESTDIR)" "$(RRDTOOL_PACKLIST_$(INSTALLDIRS))/auto/$(FULLEXT)/.packlist"
