AM_CFLAGS = \
	$(TDM_BACKEND_ANDROID_CFLAGS) \
	-I$(top_srcdir)/src

libtdm_android_la_LTLIBRARIES = libtdm-android.la
libtdm_android_ladir = $(TDM_MODULE_PATH)
libtdm_android_la_LDFLAGS = -module -avoid-version
libtdm_android_la_LIBADD = $(TDM_BACKEND_ANDROID_LIBS) -ldl

libtdm_android_la_SOURCES = \
	tdm_android_format.c \
	tdm_android_display.c \
	tdm_android_output.c \
	tdm_android_layer.c \
	tdm_android.c \
	tdm_hwc.c
