TARGET := create_random_graph

SOURCES := \
    create_random_graph.cc

TGT_LDLIBS := $(boost_ldlibs) -lmax_clique
TGT_LDFLAGS := -L${TARGET_DIR}
TGT_PREREQS := libmax_clique.a

