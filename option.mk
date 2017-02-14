BOOST_UNUSED_COMPONENTS = \
	atomic \
	chrono \
	container \
	context \
	coroutine \
	coroutine2 \
	date_time \
	exception \
	fiber \
	graph \
	graph_parallel \
	iostreams \
	locale \
	log \
	metaparse \
	mpi \
	python \
	regex \
	serialization \
	signals \
	thread \
	timer \
	type_erasure \
	wave

B2_WITHOUT_OPTIONS = $(foreach c,$(BOOST_UNUSED_COMPONENTS),--without-$(c))

CLIBSEDML_CONFIGURE_OPTIONS = --enable-silent-rules --disable-static

CZMQ_CONFIGURE_OPTIONS = \
	--enable-silent-rules \
	--disable-static \
	--disable-Werror

SOSLIB_CONFIGURE_OPTIONS = \
	--disable-static \
	--without-grace \
	--without-graphviz \
	--without-xerces

LIBSBML_CONFIGURE_OPTIONS = --with-xerces=no

LIBXML2_CONFIGURE_OPTIONS = \
	--disable-static \
	--without-debug \
	--without-ftp \
	--without-http \
	--without-lzma \
	--without-python \
	--without-threads

LIBZIP_CONFIGURE_OPTIONS = --enable-silent-rules --disable-static

PROTOBUF_CONFIGURE_OPTONS = --enable-silent-rules --disable-static

SUNDIALS_CMAKE_OPTIONS = -DBUILD_STATIC_LIBS=OFF -DBUILD_SHARED_LIBS=ON

WXWIDGETS_CONFIGURE_OPTIONS = \
	--enable-aui \
	--enable-cxx11 \
	--enable-dnd \
	--enable-monolithic \
	--disable-arcstream \
	--disable-compat30 \
	--disable-constraints \
	--disable-ftp \
	--disable-help \
	--disable-html \
	--disable-htmlhelp \
	--disable-mediactrl \
	--disable-mshtmlhelp \
	--disable-postscript \
	--disable-printarch \
	--disable-propgrid \
	--disable-protocol-ftp \
	--disable-ribbon \
	--disable-richtext \
	--disable-sound \
	--disable-static \
	--disable-svg \
	--disable-webkit \
	--disable-webview \
	--disable-xrc \
	--without-expat \
	--without-gtkprint \
	--without-opengl \
	--without-sdl

ZEROMQ_CONFIGURE_OPTIONS = --enable-silent-rules --disable-static
