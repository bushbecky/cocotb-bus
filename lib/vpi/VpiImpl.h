/******************************************************************************
* Copyright (c) 2013 Potential Ventures Ltd
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*    * Redistributions of source code must retain the above copyright
*      notice, this list of conditions and the following disclaimer.
*    * Redistributions in binary form must reproduce the above copyright
*      notice, this list of conditions and the following disclaimer in the
*      documentation and/or other materials provided with the distribution.
*    * Neither the name of Potential Ventures Ltd
*      names of its contributors may be used to endorse or promote products
*      derived from this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
* ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL POTENTIAL VENTURES LTD BE LIABLE FOR ANY
* DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
* ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
* SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
******************************************************************************/

#ifndef COCOTB_VPI_IMPL_H_ 
#define COCOTB_VPI_IMPL_H_ 

#include "../gpi/gpi_priv.h"
#include <vpi_user.h>

#define VPI_CHECKING 1

// Should be run after every VPI call to check error status
static inline int __check_vpi_error(const char *func, long line)
{
    int level=0;
#if VPI_CHECKING
    s_vpi_error_info info;
    int loglevel;
    level = vpi_chk_error(&info);
    if (level == 0)
        return 0;

    switch (level) {
        case vpiNotice:
            loglevel = GPIInfo;
            break;
        case vpiWarning:
            loglevel = GPIWarning;
            break;
        case vpiError:
            loglevel = GPIError;
            break;
        case vpiSystem:
        case vpiInternal:
            loglevel = GPICritical;
            break;
    }

    gpi_log("cocotb.gpi", loglevel, __FILE__, func, line,
            "VPI Error level %d\nPROD %s\nCODE %s\nFILE %s",
            info.message, info.product, info.code, info.file);

#endif
    return level;
}

#define check_vpi_error() do { \
    __check_vpi_error(__func__, __LINE__); \
} while (0)

class VpiImpl : public GpiImplInterface {
public:
    VpiImpl(const std::string& name) : GpiImplInterface(name) { }

     /* Sim related */
    void sim_end(void);
    void get_sim_time(uint32_t *high, uint32_t *low);

    /* Hierachy related */
    GpiObjHdl *get_root_handle(const char *name);

    /* Callback related, these may (will) return the same handle*/
    GpiCbHdl *register_timed_callback(uint64_t time_ps);
    GpiCbHdl *register_readonly_callback(void);
    GpiCbHdl *register_nexttime_callback(void);
    GpiCbHdl *register_readwrite_callback(void);
    int deregister_callback(GpiCbHdl *obj_hdl);
    bool native_check(std::string &name, GpiObjHdl *parent);
    GpiObjHdl* native_check_create(std::string &name, GpiObjHdl *parent);

    const char * reason_to_string(int reason);
};

class VpiObjHdl : public GpiObjHdl {
public:
    VpiObjHdl(GpiImplInterface *impl, vpiHandle hdl) : GpiObjHdl(impl),
                                                       vpi_hdl(hdl) { }
    virtual ~VpiObjHdl() { }

    virtual GpiObjHdl *get_handle_by_name(std::string &name);
    virtual GpiObjHdl *get_handle_by_index(uint32_t index);
    virtual GpiIterator *iterate_handle(uint32_t type) { return NULL ;}
    virtual GpiObjHdl *next_handle(GpiIterator *iterator) { return NULL; }

    //const char* get_name_str(void);
    //const char* get_type_str(void);

    vpiHandle get_handle(void); 

protected:
    vpiHandle vpi_hdl;

};

class VpiCbHdl : public GpiCbHdl {
public:
    VpiCbHdl(GpiImplInterface *impl) : GpiCbHdl(impl),
                                       vpi_hdl(NULL) { }
    virtual ~VpiCbHdl() { }

    virtual int arm_callback(void);
    virtual int cleanup_callback(void);

protected:
    int register_cb(p_cb_data cb_data);
    vpiHandle vpi_hdl;
};

class VpiSignalObjHdl : public VpiObjHdl, public GpiSignalObjHdl {
public:
    VpiSignalObjHdl(GpiImplInterface *impl, vpiHandle hdl) : VpiObjHdl(impl, hdl),
                                                             GpiSignalObjHdl(impl) { }
    virtual ~VpiSignalObjHdl() { }

    const char* get_signal_value_binstr(void);

    int set_signal_value(const int value);
    int set_signal_value(std::string &value);
    //virtual GpiCbHdl monitor_value(bool rising_edge) = 0; this was for the triggers
    // but the explicit ones are probably better

    // Also think we want the triggers here?
    virtual GpiCbHdl *rising_edge_cb(void) { return NULL; }
    virtual GpiCbHdl *falling_edge_cb(void) { return NULL; }
    virtual GpiCbHdl *value_change_cb(void) { return NULL; }

    /* Functions that I would like to inherit but do not ?*/
    virtual GpiObjHdl *get_handle_by_name(std::string &name) {
        return VpiObjHdl::get_handle_by_name(name);
    }
    virtual GpiObjHdl *get_handle_by_index(uint32_t index) {
        return VpiObjHdl::get_handle_by_index(index);
    }
    virtual GpiIterator *iterate_handle(uint32_t type)
    {
        return VpiObjHdl::iterate_handle(type);
    }
    virtual GpiObjHdl *next_handle(GpiIterator *iterator)
    {
        return VpiObjHdl::next_handle(iterator);
    }
    virtual int initialise(std::string name) {
        return VpiObjHdl::initialise(name);
    }
};

class VpiTimedCbHdl : public VpiCbHdl {
public:
    VpiTimedCbHdl(GpiImplInterface *impl) : VpiCbHdl(impl) { }
    int arm_callback(uint64_t time_ps);
    virtual ~VpiTimedCbHdl() { }
};

#if 0
class VpiReadOnlyCbHdl : public VpiCbHdl {

};

class VpiReadWriteCbHdl : public VpiCbHdl {

};

class VpiNextPhaseCbHdl : public VpiCbHdl {

};
#endif

class VpiStartupCbHdl : public VpiCbHdl {
public:
    VpiStartupCbHdl(GpiImplInterface *impl) : VpiCbHdl(impl) { }
    int run_callback(void);
    int arm_callback(void);
    virtual ~VpiStartupCbHdl() { }
};

class VpiShutdownCbHdl : public VpiCbHdl {
public:
    VpiShutdownCbHdl(GpiImplInterface *impl) : VpiCbHdl(impl) { }
    int run_callback(void);
    int arm_callback(void);
    virtual ~VpiShutdownCbHdl() { }
};

class VpiReadwriteCbHdl : public VpiCbHdl {
public:
    VpiReadwriteCbHdl(GpiImplInterface *impl) : VpiCbHdl(impl) { }
    int arm_callback(void);
    virtual ~VpiReadwriteCbHdl() { }
};

#endif /*COCOTB_VPI_IMPL_H_  */