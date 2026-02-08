#ifndef UI_SHARED_LINKS_INTERNAL_H
#define UI_SHARED_LINKS_INTERNAL_H

void cgit_site_link(const char *page, const char *name, const char *title,
		    const char *class, const char *search, const char *sort,
		    int ofs, int always_root);
void cgit_reporevlink(const char *page, const char *name, const char *title,
		      const char *class, const char *head, const char *rev,
		      const char *path);
void cgit_self_link(char *name, const char *title, const char *class);

#endif
