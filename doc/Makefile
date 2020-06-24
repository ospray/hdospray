SHELL := /bin/bash

webpages := $(addprefix www/, $(addsuffix .html, index news documentation gallery downloads legal))
process_version := $(addprefix tmp/, $(addsuffix .md, readme_head))

PANDOC := pandoc
PDMINVERSION := 2000000
PDOK := $(shell expr `$(PANDOC) --version|head -n 1|cut -d' ' -f 2| sed -e 's/\.\([0-9][0-9]\)/\1/g' -e 's/\.\([0-9]\)/0\1/g' -e 's/^[0-9]\{3,4\}$$/&00/' -e 's/^[0-9]\{5,6\}$$/&00/'` \>= $(PDMINVERSION))
ifneq "$(PDOK)" "1"
  $(error You need at least pandoc v2.0)
endif

all: www doc
www: $(webpages) www/stylesheet.css $(webimages)
doc: ../README.md

.PHONY: all www doc clean realclean submodule

submodule:
	git submodule update --init --remote

if_is_devel_then = if [[ "${CI_BUILD_REF_NAME}" == "devel" || (-d ../.git && "`git rev-parse --abbrev-ref HEAD`" == "devel") ]] ; then

HDOSPRAY_VERSION := "0.1.0"
HDOSPRAY_VERSION_MAJOR := "0"
HDOSPRAY_VERSION_MINOR := "1"
HDOSPRAY_VERSION_PATCH := "0"

### directories
########################################################################
$(webpages) www/stylesheet.css: | wwwdir
wwwdir:
	@mkdir -p www

$(webimages): | wwwimgdir
wwwimgdir:
	@mkdir -p www/images


### webpages
########################################################################

markdown2web = $(PANDOC) --email-obfuscation=none -f markdown $(filter-out webtemplate.html,$+) --template webtemplate -V select_$(basename $(@F)) -o $@

$(webpages): webtemplate.html

www/%.html:
	$(markdown2web)

www/stylesheet.css: stylesheet.css
	cp $< $@

www/index.html: teaser.html overview.md
www/news.html: news.md
www/gallery.html: gallery.md
www/downloads.html: compilation.md
www/legal.html: legal.md
www/documentation.html: compilation.md documentation.md 


### markdown
########################################################################

# also works around pandoc #5121, should be fixed in v2.5.1
convert_nbsp_width = sed -e 's@\\ @ @g' -e 's/" style="width:/" width="/g' -e "s@^</figure>@\0\n\n@"

../README.md: readme_head.md overview.md gallery.md compilation.md documentation.md news.md
	$(PANDOC) --from markdown --to=gfm --verbose --output $@ $+

#html $+ | $(PANDOC) --from=html --to=markdown-raw_html-native_divs --output $@

### clean
########################################################################

clean:
	rm -rf www tmp __pycache__

realclean: clean
	rm -irf images
