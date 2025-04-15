#!/bin/sh

find \
	. \
	-type f \
	-name "*.cpp" \
	! -path "./deps/tclap_loc/*" \
| xargs cppcheck \
	--language=c++ \
	--std=c++11 \
	--enable=all \
	--check-level=exhaustive \
	--suppress=ctuOneDefinitionRuleViolation \
	--suppress=unusedFunction \
	--suppress=missingOverride \
	--suppress=cstyleCast \
	--suppress=variableScope \
	--suppress=missingInclude \
	--suppress=missingIncludeSystem \
	--suppress=noExplicitConstructor \
	--suppress=uselessAssignmentPtrArg \
	--suppress=unmatchedSuppression \
	--suppress=checkersReport \
	$@

