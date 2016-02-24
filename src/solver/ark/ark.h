/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- vim:set ts=4 sw=4 sts=4 noet: */
#ifndef FLINT_SOLVER_ARK_ARK_H_
#define FLINT_SOLVER_ARK_ARK_H_

#include <memory>
#include <utility>
#include <vector>

#include <arkode/arkode.h>
#include <nvector/nvector_serial.h>

namespace flint {

class Layout;

namespace job {
struct Option;
}

namespace solver {

namespace ark {

class Auxv;
class Mass;
class Rhs;

class Ark {
public:
	Ark(const Layout *layout, size_t layer_size,
		Auxv *auxv, Mass *mass, Rhs *rhs);

	~Ark();

	Mass *mass() const;

	Rhs *rhs() const;

	double *data() const;

	void ReadTime(realtype t);

	void ReadData(realtype t, N_Vector y);

	void WriteData(int lo, N_Vector ydot);

	bool Solve(const job::Option &option);

private:
	bool SetProblemDimensions();

	bool SetVectorOfInitialValues(const char *data_file);

	const Layout* layout_;
	int layer_size_;
	Auxv *auxv_;
	Mass *mass_;
	Rhs *rhs_;
	std::unique_ptr<double[]> data_;
	long dim_;
	std::vector<std::pair<int, int> > states_;
	N_Vector y_;
	void *arkode_mem_;
};

}
}
}

#endif
