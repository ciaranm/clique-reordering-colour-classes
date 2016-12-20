TARGET := solve_max_clique

SOURCES := \
    solve_max_clique.cc

TGT_LDLIBS := $(boost_ldlibs) -lmax_clique
TGT_LDFLAGS := -L${TARGET_DIR}
TGT_PREREQS := libmax_clique.a

