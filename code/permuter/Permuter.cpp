#include <list>
#include <vector>

#include "Permuter.h"
#include "../utils/utils.h"

namespace fs_testing {
namespace permuter {

using std::list;
using std::size_t;
using std::vector;

using fs_testing::utils::disk_write;

namespace {

struct range {
  unsigned int start;
  unsigned int end;
};

}  // namespace


size_t BioVectorHash::operator() (const vector<unsigned int>& permutation)
    const {
  unsigned int seed = permutation.size();
  for (const auto& bio_pos : permutation) {
    seed ^= bio_pos + 0x9e3779b9 + (seed << 6) + (seed >> 2);
  }
  return seed;
}

bool BioVectorEqual::operator() (const std::vector<unsigned int>& a,
    const std::vector<unsigned int>& b) const {
  if (a.size() != b.size()) {
    return false;
  }
  for (unsigned int i = 0; i < a.size(); ++i) {
    if (a.at(i) != b.at(i)) {
      return false;
    }
  }
  return true;
}

void Permuter::InitDataVector(vector<disk_write> *data) {
  epochs_.clear();
  unsigned int index = 0;
  list<range> overlaps;
  while (index < data->size()) {
    struct epoch current_epoch;
    current_epoch.has_barrier = false;
    current_epoch.overlaps = false;

    // Get all ops in this epoch and add them to either the sync_op or async_op
    // lists.
    while (index < data->size() && !(data->at(index)).is_barrier_write()) {
      disk_write curr = data->at(index);
      // Find overlapping ranges.
      unsigned int start = curr.metadata.write_sector;
      unsigned int end = start + curr.metadata.size;
      for (auto range_iter = overlaps.begin(); range_iter != overlaps.end();
          range_iter++) {
        range r = *range_iter;
        if ((r.start <= start && r.end >= start)
            || (r.start <= end && r.end >= end)) {
          if (r.start > start) {
            r.start = start;
          }
          if (r.end < end) {
            r.end = end;
          }
          current_epoch.overlaps = true;
          break;
        } else if (r.start > end) {
          // Since this is an ordered list, if the next spot is past where we're
          // looking now then we won't find anything past here. We may as well
          // insert an item here.
          range new_range = {start, end};
          overlaps.insert(range_iter, new_range);
        }
      }

      epoch_op curr_op = {index, data->at(index)};
      current_epoch.ops.push_back(curr_op);
      current_epoch.num_meta += data->at(index).is_meta();
      ++index;
    }

    // Check is the op at the current index is a "barrier." If it is then add it
    // to the special spot in the epoch, otherwise just push the current epoch
    // onto the list and move to the next segment of the log.
    if (index < data->size() && (data->at(index)).is_barrier_write()) {
      epoch_op curr_op = {index, data->at(index)};
      current_epoch.ops.push_back(curr_op);
      current_epoch.num_meta += data->at(index).is_meta();
      current_epoch.has_barrier = true;
      ++index;
    }
    epochs_.push_back(current_epoch);
  }
}

vector<epoch>* Permuter::GetEpochs() {
  return &epochs_;
}


bool Permuter::GenerateCrashState(vector<disk_write>& res) {
  vector<epoch_op> crash_state;
  unsigned long retries = 0;
  unsigned int exists = 0;
  bool new_state = true;
  do {
    new_state = gen_one_state(crash_state);

    vector<unsigned int> crash_state_hash(crash_state.size());
    for (unsigned int i = 0; i < crash_state.size(); ++i) {
      crash_state_hash.at(i) = crash_state.at(i).abs_index;
    }

    ++retries;
    exists = completed_permutations_.count(crash_state_hash);
    if (retries >= 2 * crash_state.size()) {
      // We've likely found all possible crash states so just break.
      break;
    }
  } while (exists > 0);

  // Move the permuted crash state data over into the returned crash state
  // vector.
  res.resize(crash_state.size());
  for (unsigned int i = 0; i < crash_state.size(); ++i) {
    res.at(i) = crash_state.at(i).op;
  }

  if (exists == 0) {
    // We broke out of the above loop because this state is unique.
    return new_state;
  }

  // We broke out of the above loop because we haven't found a new state in some
  // time.
  return false;
}

}  // namespace permuter
}  // namespace fs_testing
