//  Copyright (c) 2007-2012 Hartmut Kaiser
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <hpx/hpx_fwd.hpp>

#if MPFR_FOUND != 0
#include <hpx/util/portable_binary_iarchive.hpp>
#include <hpx/util/portable_binary_oarchive.hpp>

#include "serialize_mpreal.hpp"
#include "mpreal.h"

///////////////////////////////////////////////////////////////////////////////
// windows needs to initialize MPFR in each shared library
#if defined(BOOST_WINDOWS)

#include "init_mpfr.hpp"

namespace hpx { namespace components { namespace amr
{
    // initialize mpreal default precision
    init_mpfr init_;
}}}
#endif

namespace boost { namespace serialization
{
    ///////////////////////////////////////////////////////////////////////////
    template<class Archive>
    void load(Archive& ar, mpfr::mpreal& d, unsigned int version)
    {
        std::string s;
        ar & s;
        d = s.c_str();
    }

    ///////////////////////////////////////////////////////////////////////////
    template<class Archive>
    void save(Archive& ar, mpfr::mpreal const& d, unsigned int version)
    {
        std::string s(d.to_string());
        ar & s;
    }

    ///////////////////////////////////////////////////////////////////////////
    // explicit instantiation for the correct archive types
    template MPFR_EXPORT
    void save(hpx::util::portable_binary_oarchive&, mpfr::mpreal const& d, unsigned int version);
    template MPFR_EXPORT
    void load(hpx::util::portable_binary_iarchive&, mpfr::mpreal& d, unsigned int version);
}}

#endif
