/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- vim:set ts=4 sw=4 sts=4 noet: */
#include "layout.h"

#include <iomanip>
#include <unordered_set>

#include "bc/locater.h"
#include "bc/mounter.h"

namespace flint {

Layout::Layout() = default;

Layout::~Layout() = default;

bool Layout::ContainsDependentVariable() const
{
	for (const auto &dp : dv_) {
		switch (dp->type()) {
		case lo::S:
		case lo::T:
			break;
		default:
			return true;
		}
	}
	return false;
}

int Layout::Calculate(DataOffsetMap *dom, SectorOffsetMap *som)
{
	int offset = kOffsetBase;
	int di = 0;
	int si = 0;
	boost::uuids::uuid track_id;
	boost::uuids::uuid sector_id;
	for (const auto &tp : tv_) {
		std::memcpy(&track_id, tp->id().data(), track_id.size());

		int nos = tp->nos();
		int nod = tp->nod();
		int die = di + nod;
		int sie = si + nos;
		int sector_size = 0;

		std::unique_ptr<Locater> locater(new Locater);
		while (di < die) {
			const auto &dp = dv_.at(di++);
			locater->SetPosition(dp->name(), sector_size);
			if (dom) (*dom)[track_id].insert(std::make_pair(dp->id(), sector_size));
			sector_size += dp->col() * dp->row();
		}
		lm_.insert(std::make_pair(track_id, std::move(locater)));

		std::unique_ptr<Mounter> mounter(new Mounter(nos));
		for (int i=0;i<nos;i++) {
			mounter->SetOffset(i, offset);
			const auto &sp = sv_.at(si++);
			if (som) {
				std::memcpy(&sector_id, sp->id().data(), sector_id.size());
				som->insert(std::make_pair(sector_id, offset));
			}
			offset += sector_size;
		}
		mm_.insert(std::make_pair(track_id, std::move(mounter)));
		assert(si == sie);
	}
	return offset;
}

const Locater *Layout::GetLocater(const std::string &id) const
{
	boost::uuids::uuid u;
	std::memcpy(&u, id.data(), u.size());
	auto it = lm_.find(u);
	if (it == lm_.end()) return nullptr;
	return it->second.get();
}

const Mounter &Layout::GetMounter(const std::string &id) const
{
	boost::uuids::uuid u;
	std::memcpy(&u, id.data(), u.size());
	assert(mm_.find(u) != mm_.end());
	return *mm_.at(u);
}

void Layout::DetectRed(size_t size, const size_t *color) const
{
	std::unordered_set<std::string> red;

	size_t offset = kOffsetBase;
	int di = 0;
	for (const auto &tp : tv_) {
		int nos = tp->nos();
		int nod = tp->nod();
		int dib = di;
		int die = di + nod;

		for (int i=0;i<nos;i++) {
			di = dib;
			while (di < die) {
				const auto &dp = dv_.at(di++);
				assert(offset < size);
				if (!color[offset]) {
					boost::uuids::uuid u;
					std::memcpy(&u, tp->id().data(), u.size());
					std::string us = to_string(u);
					us += ":";
					us += dp->name();
					if (red.insert(us).second) {
						std::cerr << us << std::endl;
					}
				}
				offset += dp->col() * dp->row();
			}
		}
	}
	assert(offset == size);
}

void Layout::CollectConstant(int nol, size_t size, std::set<int> *addrs) const
{
	size_t offset = kOffsetBase;
	int di = 0;
	for (const auto &tp : tv_) {
		int nos = tp->nos();
		int nod = tp->nod();
		int dib = di;
		int die = di + nod;

		for (int i=0;i<nos;i++) {
			di = dib;
			while (di < die) {
				const auto &dp = dv_.at(di++);
				assert(offset < size);
				switch (dp->type()) {
				case lo::S:
					for (int j=0;j<nol;j++) {
						addrs->insert(offset + (j * size));
					}
					break;
				default:
					// nothing to do
					break;
				}
				offset += dp->col() * dp->row();
			}
		}
	}
	assert(offset == size);
}

size_t Layout::MarkConstant(int nol, size_t size, char *arr) const
{
	size_t num_of_on = 0;
	size_t offset = kOffsetBase;
	int di = 0;
	for (const auto &tp : tv_) {
		int nos = tp->nos();
		int nod = tp->nod();
		int dib = di;
		int die = di + nod;

		for (int i=0;i<nos;i++) {
			di = dib;
			while (di < die) {
				const auto &dp = dv_.at(di++);
				assert(offset < size);
				switch (dp->type()) {
				case lo::S:
					for (int j=0;j<nol;j++) {
						arr[offset + (j * size)] = 1;
						num_of_on++;
					}
					break;
				default:
					// nothing to do
					break;
				}
				offset += dp->col() * dp->row();
			}
		}
	}
	assert(offset == size);
	return num_of_on;
}

long Layout::SelectStates(std::vector<std::pair<int, int> > *states) const
{
	long total = 0;
	size_t offset = kOffsetBase;
	int di = 0;
	for (const auto &tp : tv_) {
		int nos = tp->nos();
		int nod = tp->nod();
		int dib = di;
		int die = di + nod;
		for (int i=0;i<nos;i++) {
			di = dib;
			while (di < die) {
				const auto &dp = dv_.at(di++);
				int data_size = dp->col() * dp->row();
				switch (dp->type()) {
				case lo::X:
					if (states)
						states->push_back(std::make_pair(offset, data_size));
					total += data_size;
					break;
				default:
					break;
				}
				offset += data_size;
			}
		}
	}
	return total;
}

void Layout::Debug(size_t size) const
{
	size_t offset = kOffsetBase;
	int si = 0;
	int di = 0;
	for (const auto &tp : tv_) {
		boost::uuids::uuid u;
		std::memcpy(&u, tp->id().data(), u.size());
		std::cout << "T " << u << " " << tp->name() << std::endl;

		int nos = tp->nos();
		int nod = tp->nod();
		int dib = di;
		int die = di + nod;

		for (int i=0;i<nos;i++) {
			std::memcpy(&u, sv_[si++]->id().data(), u.size());
			std::cout << "S " << u << std::endl;

			di = dib;
			while (di < die) {
				const auto &dp = dv_.at(di++);
				assert(offset < size);
				std::cout << "|" << std::setw(4) << offset;
				switch (dp->type()) {
				case lo::S:
					std::cout << "|S|";
					break;
				case lo::T:
					std::cout << "|T|";
					break;
				case lo::V:
					std::cout << "|V|";
					break;
				case lo::X:
					std::cout << "|X|";
					break;
				}
				std::cout << dp->name() << std::endl;
				offset += dp->col() * dp->row();
			}
		}
	}
	assert(offset == size);
}

}
