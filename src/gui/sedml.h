/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- vim:set ts=4 sw=4 sts=4 noet: */
#ifndef FLINT_GUI_SEDML_H_
#define FLINT_GUI_SEDML_H_

#include <wx/filename.h>

namespace flint {
namespace gui {

struct Simulation;

bool WriteSedml(const Simulation *sim, const wxFileName &filename);

}
}

#endif