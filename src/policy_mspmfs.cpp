/*
  Copyright (c) 2016, Antonio SJ Musumeci <trapexit@spawn.link>

  Permission to use, copy, modify, and/or distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.

  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include "errno.hpp"
#include "fs_exists.hpp"
#include "fs_info.hpp"
#include "fs_path.hpp"
#include "fs_statvfs_cache.hpp"
#include "policies.hpp"
#include "policy.hpp"
#include "policy_error.hpp"
#include "policy_mspmfs.hpp"

#include <limits>
#include <string>
#include <vector>

using std::string;
using std::vector;


namespace mspmfs
{
  static
  const
  string*
  create_1(const Branches::CPtr &branches_,
           const string         &fusepath_,
           int                  *err_)
  {
    int rv;
    uint64_t mfs;
    fs::info_t info;
    const string *basepath;

    basepath = NULL;
    mfs = std::numeric_limits<uint64_t>::min();
    for(const auto &branch : *branches_)
      {
        if(branch.ro_or_nc())
          error_and_continue(*err_,EROFS);
        if(!fs::exists(branch.path,fusepath_))
          error_and_continue(*err_,ENOENT);
        rv = fs::info(branch.path,&info);
        if(rv == -1)
          error_and_continue(*err_,ENOENT);
        if(info.readonly)
          error_and_continue(*err_,EROFS);
        if(info.spaceavail < branch.minfreespace())
          error_and_continue(*err_,ENOSPC);
        if(info.spaceavail < mfs)
          continue;

        mfs = info.spaceavail;
        basepath = &branch.path;
      }

    return basepath;
  }

  static
  int
  create(const Branches::CPtr &branches_,
         const char           *fusepath_,
         StrVec               *paths_)
  {
    int error;
    string fusepath;
    const string *basepath;

    error = ENOENT;
    fusepath = fusepath_;
    for(;;)
      {
        basepath = mspmfs::create_1(branches_,fusepath,&error);
        if(basepath)
          break;
        if(fusepath == "/")
          break;
        fusepath = fs::path::dirname(fusepath);
      }

    if(basepath == NULL)
      return (errno=error,-1);

    paths_->push_back(*basepath);

    return 0;
  }
}

int
Policy::MSPMFS::Action::operator()(const Branches::CPtr &branches_,
                                   const char           *fusepath_,
                                   StrVec               *paths_) const
{
  return Policies::Action::epmfs(branches_,fusepath_,paths_);
}

int
Policy::MSPMFS::Create::operator()(const Branches::CPtr &branches_,
                                   const char           *fusepath_,
                                   StrVec               *paths_) const
{
  return ::mspmfs::create(branches_,fusepath_,paths_);
}

int
Policy::MSPMFS::Search::operator()(const Branches::CPtr &branches_,
                                   const char           *fusepath_,
                                   StrVec               *paths_) const
{
  return Policies::Search::epmfs(branches_,fusepath_,paths_);
}
