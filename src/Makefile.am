AM_CFLAGS = \
	$(TDM_EXYNOS_CFLAGS) \
	-I$(top_srcdir)/src

libtdm_exynos_la_LTLIBRARIES = libtdm-exynos.la
libtdm_exynos_ladir = $(TDM_MODULE_PATH)
libtdm_exynos_la_LDFLAGS = -module -avoid-version
libtdm_exynos_la_LIBADD = $(TDM_EXYNOS_LIBS) -ldl

libtdm_exynos_la_SOURCES = \
	tdm_exynos_format.c \
	tdm_exynos_display.c \
	tdm_exynos_output.c \
	tdm_exynos_layer.c \
	tdm_exynos_pp.c \
	tdm_exynos_capture.c \
	tdm_exynos.c
