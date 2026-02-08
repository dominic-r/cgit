# This Makefile is run in the "git" directory in order to re-use Git's
# build variables and operating system detection.  Hence all files in
# CGit's directory must be prefixed with "../".
include Makefile

CGIT_PREFIX = ../

-include $(CGIT_PREFIX)cgit.conf

# The CGIT_* variables are inherited when this file is called from the
# main Makefile - they are defined there.

$(CGIT_PREFIX)VERSION: force-version
	@cd $(CGIT_PREFIX) && '$(SHELL_PATH_SQ)' ./gen-version.sh "$(CGIT_VERSION)"
-include $(CGIT_PREFIX)VERSION
.PHONY: force-version

# CGIT_CFLAGS is a separate variable so that we can track it separately
# and avoid rebuilding all of Git when these variables change.
CGIT_CFLAGS += -DCGIT_CONFIG='"$(CGIT_CONFIG)"'
CGIT_CFLAGS += -DCGIT_SCRIPT_NAME='"$(CGIT_SCRIPT_NAME)"'
CGIT_CFLAGS += -DCGIT_CACHE_ROOT='"$(CACHE_ROOT)"'
CGIT_CFLAGS += -I$(CGIT_PREFIX)include
CGIT_CFLAGS += -I$(CGIT_PREFIX)include/cgit

PKG_CONFIG ?= pkg-config

ifdef NO_C99_FORMAT
	CFLAGS += -DNO_C99_FORMAT
endif

ifdef NO_LUA
	LUA_MESSAGE := linking without specified Lua support
	CGIT_CFLAGS += -DNO_LUA
else
ifeq ($(LUA_PKGCONFIG),)
	LUA_PKGCONFIG := $(shell for pc in luajit lua lua5.2 lua5.1; do \
			$(PKG_CONFIG) --exists $$pc 2>/dev/null && echo $$pc && break; \
			done)
	LUA_MODE := autodetected
else
	LUA_MODE := specified
endif
ifneq ($(LUA_PKGCONFIG),)
	LUA_MESSAGE := linking with $(LUA_MODE) $(LUA_PKGCONFIG)
	LUA_LIBS := $(shell $(PKG_CONFIG) --libs $(LUA_PKGCONFIG) 2>/dev/null)
	LUA_CFLAGS := $(shell $(PKG_CONFIG) --cflags $(LUA_PKGCONFIG) 2>/dev/null)
	CGIT_LIBS += $(LUA_LIBS)
	CGIT_CFLAGS += $(LUA_CFLAGS)
else
	LUA_MESSAGE := linking without autodetected Lua support
	NO_LUA := YesPlease
	CGIT_CFLAGS += -DNO_LUA
endif

endif

# Add -ldl to linker flags on systems that commonly use GNU libc.
ifneq (,$(filter $(uname_S),Linux GNU GNU/kFreeBSD))
	CGIT_LIBS += -ldl
endif

# glibc 2.1+ offers sendfile which the most common C library on Linux
ifeq ($(uname_S),Linux)
	HAVE_LINUX_SENDFILE = YesPlease
endif

ifdef HAVE_LINUX_SENDFILE
	CGIT_CFLAGS += -DHAVE_LINUX_SENDFILE
endif

CGIT_CORE_OBJ_NAMES += src/core/cgit.o
CGIT_CORE_OBJ_NAMES += src/core/cache.o
CGIT_CORE_OBJ_NAMES += src/core/cache-processing.o
CGIT_CORE_OBJ_NAMES += src/core/cgit-auth.o
CGIT_CORE_OBJ_NAMES += src/core/cgit-config.o
CGIT_CORE_OBJ_NAMES += src/core/cgit-context.o
CGIT_CORE_OBJ_NAMES += src/core/cgit-repo.o
CGIT_CORE_OBJ_NAMES += src/core/cgit-repolist.o
CGIT_CORE_OBJ_NAMES += src/core/cmd.o
CGIT_CORE_OBJ_NAMES += src/core/configfile.o
CGIT_CORE_OBJ_NAMES += src/core/filter.o
CGIT_CORE_OBJ_NAMES += src/core/filter-exec.o
CGIT_CORE_OBJ_NAMES += src/core/filter-lua.o
CGIT_CORE_OBJ_NAMES += src/core/html.o
CGIT_CORE_OBJ_NAMES += src/core/parsing.o
CGIT_CORE_OBJ_NAMES += src/core/scan-tree.o
CGIT_CORE_OBJ_NAMES += src/core/shared.o
CGIT_CORE_OBJ_NAMES += src/core/shared-diff.o
CGIT_CORE_OBJ_NAMES += src/core/shared-utils.o

CGIT_UI_OBJ_NAMES += src/ui/ui-atom.o
CGIT_UI_OBJ_NAMES += src/ui/ui-blame.o
CGIT_UI_OBJ_NAMES += src/ui/ui-blob.o
CGIT_UI_OBJ_NAMES += src/ui/ui-clone.o
CGIT_UI_OBJ_NAMES += src/ui/ui-commit.o
CGIT_UI_OBJ_NAMES += src/ui/ui-compare.o
CGIT_UI_OBJ_NAMES += src/ui/ui-diff.o
CGIT_UI_OBJ_NAMES += src/ui/ui-diff-render.o
CGIT_UI_OBJ_NAMES += src/ui/ui-log.o
CGIT_UI_OBJ_NAMES += src/ui/ui-log-commit.o
CGIT_UI_OBJ_NAMES += src/ui/ui-patch.o
CGIT_UI_OBJ_NAMES += src/ui/ui-plain.o
CGIT_UI_OBJ_NAMES += src/ui/ui-refs.o
CGIT_UI_OBJ_NAMES += src/ui/ui-repolist.o
CGIT_UI_OBJ_NAMES += src/ui/ui-search-render.o
CGIT_UI_OBJ_NAMES += src/ui/ui-shared.o
CGIT_UI_OBJ_NAMES += src/ui/ui-shared-forms.o
CGIT_UI_OBJ_NAMES += src/ui/ui-shared-layout.o
CGIT_UI_OBJ_NAMES += src/ui/ui-shared-links-nav.o
CGIT_UI_OBJ_NAMES += src/ui/ui-shared-links-submodule.o
CGIT_UI_OBJ_NAMES += src/ui/ui-shared-links-url.o
CGIT_UI_OBJ_NAMES += src/ui/ui-shared-snapshot.o
CGIT_UI_OBJ_NAMES += src/ui/ui-shared-time-http.o
CGIT_UI_OBJ_NAMES += src/ui/ui-snapshot.o
CGIT_UI_OBJ_NAMES += src/ui/ui-ssdiff.o
CGIT_UI_OBJ_NAMES += src/ui/ui-stats.o
CGIT_UI_OBJ_NAMES += src/ui/ui-stats-periods.o
CGIT_UI_OBJ_NAMES += src/ui/ui-stats-render.o
CGIT_UI_OBJ_NAMES += src/ui/ui-summary.o
CGIT_UI_OBJ_NAMES += src/ui/ui-tag.o
CGIT_UI_OBJ_NAMES += src/ui/ui-search.o
CGIT_UI_OBJ_NAMES += src/ui/ui-tree.o

CGIT_OBJ_NAMES += $(CGIT_CORE_OBJ_NAMES)
CGIT_OBJ_NAMES += $(CGIT_UI_OBJ_NAMES)

CGIT_OBJS := $(addprefix $(CGIT_PREFIX),$(CGIT_OBJ_NAMES))

# Only cgit.c reference CGIT_VERSION so we only rebuild its objects when the
# version changes.
CGIT_VERSION_OBJS := $(addprefix $(CGIT_PREFIX),src/core/cgit.o src/core/cgit.sp)
$(CGIT_VERSION_OBJS): $(CGIT_PREFIX)VERSION
$(CGIT_VERSION_OBJS): EXTRA_CPPFLAGS = \
	-DCGIT_VERSION='"$(CGIT_VERSION)"'

# ui-shared.c uses __DATE__/__TIME__ for asset cache busting, so always rebuild.
$(CGIT_PREFIX)src/ui/ui-shared.o: FORCE

# Git handles dependencies using ":=" so dependencies in CGIT_OBJ are not
# handled by that and we must handle them ourselves.
cgit_dep_files := $(foreach f,$(CGIT_OBJS),$(dir $f).depend/$(notdir $f).d)
cgit_dep_files_present := $(wildcard $(cgit_dep_files))
ifneq ($(cgit_dep_files_present),)
include $(cgit_dep_files_present)
endif

cgit_dep_dirs := $(addsuffix .depend,$(sort $(dir $(CGIT_OBJS))))
missing_dep_dirs += $(filter-out $(wildcard $(cgit_dep_dirs)),$(cgit_dep_dirs))

$(cgit_dep_dirs):
	@mkdir -p $@

$(CGIT_PREFIX)CGIT-CFLAGS: FORCE
	@FLAGS='$(subst ','\'',$(CGIT_CFLAGS))'; \
	    if test x"$$FLAGS" != x"`cat ../CGIT-CFLAGS 2>/dev/null`" ; then \
		echo 1>&2 "    * new CGit build flags"; \
		echo "$$FLAGS" >$(CGIT_PREFIX)CGIT-CFLAGS; \
            fi

$(CGIT_OBJS): %.o: %.c GIT-CFLAGS $(CGIT_PREFIX)CGIT-CFLAGS $(missing_dep_dirs)
	$(QUIET_CC)$(CC) -o $*.o -c $(dep_args) $(ALL_CFLAGS) $(EXTRA_CPPFLAGS) $(CGIT_CFLAGS) $<

$(CGIT_PREFIX)cgit: $(CGIT_OBJS) GIT-LDFLAGS $(GITLIBS)
	@echo 1>&1 "    * $(LUA_MESSAGE)"
	$(QUIET_LINK)$(CC) $(ALL_CFLAGS) -o $@ $(ALL_LDFLAGS) $(filter %.o,$^) $(LIBS) $(CGIT_LIBS)

CGIT_SP_OBJS := $(patsubst %.o,%.sp,$(CGIT_OBJS))

$(CGIT_SP_OBJS): %.sp: %.c GIT-CFLAGS $(CGIT_PREFIX)CGIT-CFLAGS FORCE
	$(QUIET_SP)cgcc -no-compile $(ALL_CFLAGS) $(EXTRA_CPPFLAGS) $(CGIT_CFLAGS) $(SPARSE_FLAGS) $<

cgit-sparse: $(CGIT_SP_OBJS)
