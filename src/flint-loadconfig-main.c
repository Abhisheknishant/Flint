/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- vim:set ts=4 sw=4 sts=4 noet: */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void PrintCommonRecipe(void)
{
	printf("filter: modeldb spec.txt layout\n");
	printf("\tflint-filter $^ $@\n");
	printf("\n");
	printf("track: filter\n");
	printf("\tflint-track $< > $@\n");
	printf("\n");
	printf("isdh: filter\n");
	printf("\tflint-isdh $< $@\n");
	printf("\n");
	printf("form.txt: db\n");
	printf("\tflint-form $< > $@\n");
	printf("\n");
	printf("generated-layout: db form.txt\n");
	printf("\tflint-layout $< $@\n"); /* TODO */
	printf("\n");
}

static void PrintRecipeForCellml(void)
{
	printf("MODEL_LANG = cellml\n");
	printf("\n");
	printf("modeldb: model\n");
	printf("\tflint-concat $< $@\n");
	printf("\tflint-cellml $@ iv.txt function.txt ode.txt\n");
	printf("\n");
	printf("iv.txt function.txt ode.txt: modeldb\n");
	printf("\n");
	printf("layout: modeldb\n");
	printf("\tflint-layout $< $@\n");
	printf("\n");
	printf("flow.txt: modeldb layout\n");
	printf("\tflint-flow $^ > $@\n");
	printf("\n");
	printf("value.txt: iv.txt function.txt\n");
	printf("\tflint-concat $^ $@\n");
	printf("\n");
	printf("const.txt: value.txt\n");
	printf("\tflint-assign < $< > $@\n");
	printf("\n");
	printf("const-bc: const.txt\n");
	printf("\tflint-compile const.db < $< > $@\n");
	printf("\n");
	printf("statement.txt: function.txt ode.txt\n");
	printf("\tflint-concat $^ $@\n");
	printf("\n");
	printf("init: layout const-bc flow.txt\n");
	printf("\tflint-init $^ $@\n");
	printf("\n");
	printf("var: layout\n");
	printf("\tflint-var $^ > $@\n");
	printf("\n");
}

static void PrintRecipeForPhml(void)
{
	printf("MODEL_LANG = phml\n");
	printf("\n");
	printf("extra-a.txt: modeldb\n");
	printf("\tflint-extra -a $< > $@\n");
	printf("\n");
	printf("extra-b.txt: modeldb\n");
	printf("\tflint-extra -b $< > $@\n");
	printf("\n");
	printf("impl-sv.txt: modeldb\n");
	printf("\tflint-impl -s -v $< > $@\n");
	printf("\n");
	printf("impl-vx.txt: modeldb\n");
	printf("\tflint-impl -v -x $< > $@\n");
	printf("\n");
	printf("iv.txt: modeldb\n");
	printf("\tflint-iv $< > $@\n");
	printf("\n");
	printf("nc: modeldb\n");
	printf("\tflint-nc $< method.txt > $@\n");
	printf("\n");
	printf("method.txt: nc\n");
	printf("\n");
	printf("unit: modeldb\n");
	printf("\tflint-unit $< > $@\n");
	printf("\n");
	printf("tsc.txt: modeldb\n");
	printf("\tflint-tsc $< tsc.bin > $@\n");
	printf("\n");
	printf("tsc.bin: tsc.txt\n");
	printf("\n");
	printf("combineall.txt: modeldb\n");
	printf("\tflint-combineall $< combine-value.txt combine-function.txt combine-ode.txt > $@\n");
	printf("\n");
	printf("combine-value.txt combine-function.txt combine-ode.txt: combineall.txt\n");
	printf("\n");
	printf("value.txt: impl-sv.txt iv.txt tsc.txt combine-value.txt combine-function.txt\n");
	printf("\tflint-concat $^ $@\n");
	printf("\n");
	printf("layout: modeldb combineall.txt\n");
	printf("\tflint-layout $< $@\n");
	printf("\n");
	printf("flow.txt: modeldb layout\n");
	printf("\tflint-flow $^ > $@\n");
	printf("\n");
	printf("const.txt: value.txt\n");
	printf("\tflint-assign < $< > $@\n");
	printf("\n");
	printf("const-bc: const.txt\n");
	printf("\tflint-compile const.db < $< > $@\n");
	printf("\n");
	printf("after.txt: extra-a.txt\n");
	printf("\tflint-event < $< > $@\n");
	printf("\n");
	printf("after-bc: after.txt\n");
	printf("\tflint-compile after.db < $< > $@\n");
	printf("\n");
	printf("before.txt: extra-b.txt\n");
	printf("\tflint-event < $< > $@\n");
	printf("\n");
	printf("before-bc: before.txt\n");
	printf("\tflint-compile before.db < $< > $@\n");
	printf("\n");
	printf("statement.txt: impl-vx.txt tsc.txt combine-function.txt combine-ode.txt\n");
	printf("\tflint-concat $^ $@\n");
	printf("\n");
	printf("init: tsc.bin layout const-bc flow.txt\n");
	printf("\tflint-init --db modeldb --input-timeseries $^ $@\n");
	printf("\n");
	printf("param: layout\n");
	printf("\tflint-param $^ > $@\n");
	printf("\n");
	printf("unitoftime: unit\n");
	printf("\tflint-unitoftime < $^ > $@\n");
	printf("\n");
	printf("var: layout\n");
	printf("\tflint-var $^ > $@\n");
	printf("\n");
	printf("lands.txt: nc unitoftime\n");
	printf("\tflint-lands $^ > $@\n");
	printf("\n");
	printf("load: nc init param unitoftime var\n");
	printf("\n");
}

static void PrintRecipeForSbml(void)
{
	printf("MODEL_LANG = sbml\n");
	printf("\n");
	printf("sbml.txt: model\n");
	printf("\tflint-sbml $< value.txt function.txt ode.txt > $@\n");
	printf("\n");
	printf("value.txt function.txt ode.txt: sbml.txt\n");
	printf("\n");
	printf("modeldb: model sbml.txt\n");
	printf("\tflint-concat $< $@\n");
	printf("\n");
	printf("layout: modeldb\n");
	printf("\tflint-layout $< $@\n");
	printf("\n");
	printf("flow.txt:\n");
	printf("\tflint-concat $@\n");
	printf("\n");
	printf("input.txt: value.txt function.txt\n");
	printf("\tflint-concat $^ $@\n");
	printf("\n");
	printf("output.txt: input.txt\n");
	printf("\tflint-assign < $< > $@\n");
	printf("\n");
	printf("output-bc: output.txt\n");
	printf("\tflint-compile output.db < $< > $@\n");
	printf("\n");
	printf("statement.txt: function.txt ode.txt\n");
	printf("\tflint-concat $^ $@\n");
	printf("\n");
	printf("init: layout output-bc\n");
#ifdef _WIN32
	/* use NUL */
	printf("\tflint-init $^ NUL $@\n");
#else
	printf("\tflint-init $^ /dev/null $@\n");
#endif
	printf("\n");
	printf("param: layout\n");
	printf("\tflint-param $^ > $@\n");
	printf("\n");
	printf("var: layout\n");
	printf("\tflint-var $^ > $@\n");
	printf("\n");
	printf("load: init param var\n");
	printf("\n");
}

static void Usage(void)
{
	fprintf(stderr, "usage: flint-loadconfig MODE\n");
}

int main(int argc, char *argv[])
{
	if (argc != 2) {
		Usage();
		return EXIT_FAILURE;
	}
	if ( strcmp(argv[1], "-h") == 0 ||
		 strcmp(argv[1], "--help") == 0 ) {
		Usage();
		return EXIT_SUCCESS;
	}
	char type[32]; /* TODO: long enough? */
	if (!fgets(type, 32, stdin)) {
		fprintf(stderr, "failed to read input\n");
		return EXIT_FAILURE;
	}
	if ( strncmp(type, "isml", 4) == 0 ||
		 strncmp(type, "phml", 4) == 0 ) {
		printf("modeldb: model\n");
		printf("\tflint-concat $< $@\n");
		printf("\tflint-phml $@\n");
		printf("\n");
		PrintRecipeForPhml();
		if (strcmp(argv[1], "run") == 0) {
			printf("conf.txt: method.txt lands.txt\n");
			printf("\tflint-concat $^ $@\n");
			printf("\n");
		}
	} else if (strncmp(type, "phz", 3) == 0) {
		printf("modeldb: model\n");
		printf("\tflint-concat $< $@\n");
		printf("\tflint-phz $@ phz\n");
		printf("\tflint-phml $@\n");
		printf("\n");
		PrintRecipeForPhml();
		if (strcmp(argv[1], "run") == 0) {
			printf("conf.txt: method.txt lands.txt\n");
			printf("\tflint-concat $^ $@\n");
			printf("\n");
		}
	} else if (strncmp(type, "cellml", 6) == 0) {
		PrintRecipeForCellml();
		if (strcmp(argv[1], "run") == 0) {
			printf("conf.txt:\n");
			printf("\techo rk4 2000 0.01 > $@\n");
			printf("\n");
		}
	} else if (strncmp(type, "sbml", 4) == 0) {
		PrintRecipeForSbml();
		if (strcmp(argv[1], "run") == 0) {
			printf("conf.txt:\n");
			printf("\techo rk4 100 0.01 > $@\n");
			printf("\n");
		}
	} else {
		fprintf(stderr, "unknown file type: %s\n", type);
		return EXIT_FAILURE;
	}
	PrintCommonRecipe();
	if (strcmp(argv[1], "run") == 0) {
		/* create the list of all variables */
		printf("spec.txt: layout\n");
		printf("\tflint-spec $^ > $@\n");
		printf("\n");
	}
	return EXIT_SUCCESS;
}
