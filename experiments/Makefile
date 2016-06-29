SHELL := /bin/bash
VARIANTS := default 2df sdf shuffle
RESULTS := results
SIZES := 150
DENSITIES := 0.9
REPEATS := $(shell seq -w 00 99 )
TIMEOUT := 0

$(RESULTS)/%.scatter : $(RESULTS)/%.R
	R --slave < $(RESULTS)/$*.R > $(RESULTS)/$*.scatter

$(RESULTS)/%-default.heatmap : $(foreach i,$(REPEATS),$(RESULTS)/%-$(i)-default.scatter )
	ruby heatmapify.rb $(foreach i,$(REPEATS),$(RESULTS)/$*-$(i)-default.scatter ) > $@

$(RESULTS)/%-sdf.heatmap : $(foreach i,$(REPEATS),$(RESULTS)/%-$(i)-sdf.scatter )
	ruby heatmapify.rb $(foreach i,$(REPEATS),$(RESULTS)/$*-$(i)-sdf.scatter ) > $@

$(RESULTS)/%-2df.heatmap : $(foreach i,$(REPEATS),$(RESULTS)/%-$(i)-2df.scatter )
	ruby heatmapify.rb $(foreach i,$(REPEATS),$(RESULTS)/$*-$(i)-2df.scatter ) > $@

$(RESULTS)/%-shuffle.heatmap : $(foreach i,$(REPEATS),$(RESULTS)/%-$(i)-shuffle.scatter )
	ruby heatmapify.rb $(foreach i,$(REPEATS),$(RESULTS)/$*-$(i)-shuffle.scatter ) > $@

all : $(foreach s,$(SIZES),$(foreach d,$(DENSITIES),$(foreach v,$(VARIANTS),$(RESULTS)/$(s)-$(d)-$(v).heatmap $(foreach i,$(REPEATS),$(RESULTS)/$(s)-$(d)-$(i)-$(v).R $(RESULTS)/$(s)-$(d)-$(i)-$(v).scatter ))))

define INSTANCE_template
$(RESULTS)/$(1)-$(2)-%-default.R :
	../code/solve_max_clique <(../code/create_random_graph $(1) $(2) $$* ) --tau 2>$$@

$(RESULTS)/$(1)-$(2)-%-2df.R :
	../code/solve_max_clique <(../code/create_random_graph $(1) $(2) $$* ) --tau --2df 2>$$@

$(RESULTS)/$(1)-$(2)-%-sdf.R :
	../code/solve_max_clique <(../code/create_random_graph $(1) $(2) $$* ) --tau --sdf 2>$$@

$(RESULTS)/$(1)-$(2)-%-shuffle.R :
	../code/solve_max_clique <(../code/create_random_graph $(1) $(2) $$* ) --tau --shuffle-before-tau 2>$$@
endef

$(foreach s,$(SIZES),$(foreach d,$(DENSITIES),$(eval $(call INSTANCE_template,$(s),$(d)))))

