//  Copyright (c) 2007-2012 Hartmut Kaiser
// 
//  Distributed under the Boost Software License, Version 1.0. (See accompanying 
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <cstring>
#include <iostream>

#include <hpx/hpx.hpp>
#include <hpx/hpx_init.hpp>

#include <boost/lexical_cast.hpp>
#include <boost/program_options.hpp>
#include <boost/thread.hpp>

//#include "amr/stencil_value.hpp"
#include "init_mpfr.hpp"
#include "amr/dynamic_stencil_value.hpp"
#include "amr/functional_component.hpp"
#include "amr/unigrid_mesh.hpp"
#include "amr_c/stencil.hpp"
#include "amr_c/logging.hpp"

#include "amr_c_test/rand.hpp"

namespace po = boost::program_options;

using namespace hpx;

///////////////////////////////////////////////////////////////////////////////
// initialize mpreal default precision
namespace hpx { namespace components { namespace amr 
{
    init_mpfr init_(true);
}}}

///////////////////////////////////////////////////////////////////////////////
int hpx_main(std::size_t numvals, std::size_t numsteps,bool do_logging,
             components::amr::Parameter const& par)
{
    // get component types needed below
    components::component_type function_type = 
        components::get_component_type<components::amr::stencil>();
    components::component_type logging_type = 
        components::get_component_type<components::amr::server::logging>();

    {
        naming::id_type here = applier::get_applier().get_runtime_support_gid();

        if ( par->loglevel > 0 ) {
          // over-ride a false command line argument
          do_logging = true;
        }

        hpx::util::high_resolution_timer t;
        std::vector<naming::id_type> result_data;
        
        // we are in spherical symmetry, r=0 is the smallest radial domain point             
        components::amr::unigrid_mesh unigrid_mesh;
        unigrid_mesh.create(here);
        result_data = unigrid_mesh.init_execute(function_type, numvals, numsteps,
            do_logging ? logging_type : components::component_invalid,par);
        printf("Elapsed time: %f s\n", t.elapsed());
    
    // provide some wait time to read the elapsed time measurement
    //std::cout << " Hit return " << std::endl;
    //int junk;
    //std::cin >> junk;

        // get some output memory_block_data instances
        /*
        std::cout << "Results: " << std::endl;
        for (std::size_t i = 0; i < result_data.size(); ++i)
        {
            components::access_memory_block<components::amr::stencil_data> val(
                components::stubs::memory_block::get(result_data[i]));
            std::cout << i << ": " << val->value_ << std::endl;
        }
        */

//         boost::this_thread::sleep(boost::posix_time::seconds(3)); 

        for (std::size_t i = 0; i < result_data.size(); ++i)
            components::stubs::memory_block::free(result_data[i]);
    }   // amr_mesh needs to go out of scope before shutdown

    // initiate shutdown of the runtime systems on all localities
    components::stubs::runtime_support::shutdown_all();

    return 0;
}

int hpx_main(po::variables_map & vm)
{
    std::size_t numvals;

    std::size_t numsteps = 400;
    if (vm.count("numsteps"))
        numsteps = vm["numsteps"].as<std::size_t>();

    bool do_logging = vm.count("verbose") ? true : false;

    components::amr::Parameter par;

    // default pars
    par->allowedl    = 0;
    par->loglevel    = 2;
    par->output      = 1.0;
    par->output_stdout = 1;
    par->lambda      = 0.15;
    int nx0          = 33;
    par->nt0         = numsteps;
    par->minx0       =   0.0;
    par->maxx0       =  15.0;
    par->ethreshold  =  0.005;
    par->R0          =  8.0;
    par->amp         =  0.1;
    par->delta       =  1.0;
    par->PP          =  7;
    par->eps         =  0.0;
    par->output_level =  0;
    par->granularity =  3;
    for (int i=0;i<maxlevels;i++) {
      // default
      par->refine_level[i] = 1.5;
    }

    int scheduler = 1;  // 0: global scheduler
                        // 1: parallel scheduler
    std::string parfile;
    if (vm.count("parfile")) {
        parfile = vm["parfile"].as<std::string>();
        hpx::util::section pars(parfile);

        if ( pars.has_section("had_amr") ) {
          hpx::util::section *sec = pars.get_section("had_amr");
          if ( sec->has_entry("lambda") ) {
            std::string tmp = sec->get_entry("lambda");
            par->lambda = atof(tmp.c_str());
          }
          if ( sec->has_entry("allowedl") ) {
            std::string tmp = sec->get_entry("allowedl");
            par->allowedl = atoi(tmp.c_str());
          }
          if ( sec->has_entry("loglevel") ) {
            std::string tmp = sec->get_entry("loglevel");
            par->loglevel = atoi(tmp.c_str());
          }
          if ( sec->has_entry("output") ) {
            std::string tmp = sec->get_entry("output");
            par->output = atof(tmp.c_str());
          }
          if ( sec->has_entry("output_stdout") ) {
            std::string tmp = sec->get_entry("output_stdout");
            par->output_stdout = atoi(tmp.c_str());
          }
          if ( sec->has_entry("output_level") ) {
            std::string tmp = sec->get_entry("output_level");
            par->output_level = atoi(tmp.c_str());
          }
          if ( sec->has_entry("nx0") ) {
            std::string tmp = sec->get_entry("nx0");
            nx0 = atoi(tmp.c_str());
          }
          if ( sec->has_entry("nt0") ) {
            std::string tmp = sec->get_entry("nt0");
            par->nt0 = atoi(tmp.c_str());
            // over-ride command line argument if present
            numsteps = par->nt0;
          }
          if ( sec->has_entry("thread_scheduler") ) {
            std::string tmp = sec->get_entry("thread_scheduler");
            scheduler = atoi(tmp.c_str());
            BOOST_ASSERT( scheduler == 0 || scheduler == 1 );
          }
          if ( sec->has_entry("maxx0") ) {
            std::string tmp = sec->get_entry("maxx0");
            par->maxx0 = atof(tmp.c_str());
          }
          if ( sec->has_entry("ethreshold") ) {
            std::string tmp = sec->get_entry("ethreshold");
            par->ethreshold = atof(tmp.c_str());
          }
          if ( sec->has_entry("R0") ) {
            std::string tmp = sec->get_entry("R0");
            par->R0 = atof(tmp.c_str());
          }
          if ( sec->has_entry("delta") ) {
            std::string tmp = sec->get_entry("delta");
            par->delta = atof(tmp.c_str());
          }
          if ( sec->has_entry("amp") ) {
            std::string tmp = sec->get_entry("amp");
            par->amp = atof(tmp.c_str());
          }
          if ( sec->has_entry("PP") ) {
            std::string tmp = sec->get_entry("PP");
            par->PP = atoi(tmp.c_str());
          }
          if ( sec->has_entry("eps") ) {
            std::string tmp = sec->get_entry("eps");
            par->eps = atof(tmp.c_str());
          }
          if ( sec->has_entry("granularity") ) {
            std::string tmp = sec->get_entry("granularity");
            par->granularity = atoi(tmp.c_str());
           // if ( par->granularity < 3 ) {
           //   std::cerr << " Problem: granularity must be at least 3 : " << par->granularity << std::endl;
           //   BOOST_ASSERT(false);
           // }
          }
          for (int i=0;i<par->allowedl;i++) {
            char tmpname[80];
            sprintf(tmpname,"refine_level_%d",i);
            if ( sec->has_entry(tmpname) ) {
              std::string tmp = sec->get_entry(tmpname);
              par->refine_level[i] = atof(tmp.c_str());
            }
          }

        }
    }

    
    // derived parameters
    if ( nx0%par->granularity != 0 ) {
      std::cerr << " PROBLEM : nx0 must be divisible by the granularity " << std::endl;
      std::cerr << " nx0 " << nx0 << " granularity " << par->granularity << std::endl;
      BOOST_ASSERT(false);
    }
    par->nx0 = nx0/par->granularity;

    par->nx[0] = par->nx0;
    for (int i=1;i<par->allowedl+1;i++) {
      par->nx[i] = int(par->refine_level[i-1]*par->nx[i-1]);
    }

    for (int j=0;j<=par->allowedl;j++) {
      par->rowsize.push_back(par->nx[par->allowedl]);
      for (int i=par->allowedl-1;i>=j;i--) {
        // remove duplicates
        par->rowsize[j] += par->nx[i] - (par->nx[i+1]+1)/2;
      }
    }

    for (int j=0;j<=par->allowedl;j++) {
      if ( j != par->allowedl ) par->level_begin.push_back(par->rowsize[j+1]);
      else par->level_begin.push_back(0);
      par->level_end.push_back(par->rowsize[j]);
    }

    // Compute dx
    had_double_type tmp = 0.0;
    for (int j=par->allowedl;j>0;j--) {
      tmp += (par->level_end[j]-par->level_begin[j])*par->granularity/pow(2.0,j);
    }

    for (int j=par->level_begin[0];j<par->rowsize[0]-1;j++) {
      tmp += par->granularity;
    }

    par->dx0 = (par->maxx0 - par->minx0)/(tmp + par->granularity-1);

    //par->dx0 = (par->maxx0 - par->minx0)/((par->nx0-1)*par->granularity);
    par->dt0 = par->lambda*par->dx0;

    // figure out the number of points
    numvals = par->rowsize[0];

    //had_double_type tmp2 = 3*pow(2,par->allowedl);
    //int num_rows = (int) tmp2;
    //numsteps = numsteps*3 - 2;

    // create output file to append to
    FILE *fdata;
    fdata = fopen("chi.dat","w");
    fprintf(fdata,"\n");
    fclose(fdata);

    fdata = fopen("Phi.dat","w");
    fprintf(fdata,"\n");
    fclose(fdata);

    fdata = fopen("Pi.dat","w");
    fprintf(fdata,"\n");
    fclose(fdata);

    fdata = fopen("energy.dat","w");
    fprintf(fdata,"\n");
    fclose(fdata);

    fdata = fopen("logcode1.dat","w");
    fprintf(fdata,"\n");
    fclose(fdata);

    fdata = fopen("logcode2.dat","w");
    fprintf(fdata,"\n");
    fclose(fdata);
    return hpx_main(numvals, numsteps, do_logging, par);
}

///////////////////////////////////////////////////////////////////////////////
int main(int argc, char* argv[])
{
    try {
        po::options_description desc_cmdline ("Usage: hpx_runtime [options]");
        desc_cmdline.add_options()
            ("dist,d", po::value<std::string>(), 
                "random distribution type (uniform or normal)")
            ("numsteps,s", po::value<std::size_t>(), 
                "the number of time steps to use for the computation")
            ("parfile,p", po::value<std::string>(), 
                "the parameter file")
            ("verbose,v", "print calculated values after each time step")
        ;

        hpx::init(desc_cmdline, argc, argv);
    }
    catch (std::exception& e) {
        std::cerr << "std::exception caught: " << e.what() << "\n";
        return -1;
    }
    catch (...) {
        std::cerr << "unexpected exception caught\n";
        return -2;
    }

    return 0;
}

