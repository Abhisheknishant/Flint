/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- vim:set ts=4 sw=4 sts=4 noet: */
#ifndef FLINT_RUNTIME_FLOW_HH_
#define FLINT_RUNTIME_FLOW_HH_

#include <unordered_map>
#include <unordered_set>

#include "sqlite3.h"
#include "reduction.hh"

namespace flint {

struct InboundChannel {
	Reduction reduction;
	std::unordered_set<int> sources;
	int size;
};

typedef std::unordered_map<int, InboundChannel> FlowInboundMap;

/*
 * Note that db is for read only.
 */
bool LoadFlows(sqlite3 *db, FlowInboundMap *im);

}

#endif
