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

#ifndef COCOTB_VHPI_IMPL_H_ 
#define COCOTB_VHPI_IMPL_H_ 

#include "../gpi/gpi_priv.h"
#include <vhpi_user.h>
#include <vector>
#include <map>

// Should be run after every VHPI call to check error status
static inline int __check_vhpi_error(const char *file, const char *func, long line)
{
    int level=0;
#if VHPI_CHECKING
    vhpiErrorInfoT info;
    int loglevel;
    level = vhpi_check_error(&info);
    if (level == 0)
        return 0;

    switch (level) {
        case vhpiNote:
            loglevel = GPIInfo;
            break;
        case vhpiWarning:
            loglevel = GPIWarning;
            break;
        case vhpiError:
            loglevel = GPIError;
            break;
        case vhpiFailure:
        case vhpiSystem:
        case vhpiInternal:
            loglevel = GPICritical;
            break;
    }

    gpi_log("cocotb.gpi", loglevel, file, func, line,
            "VHPI Error level %d: %s\nFILE %s:%d",
            info.severity, info.message, info.file, info.line);

#endif
    return level;
}

#define check_vhpi_error() do { \
    __check_vhpi_error(__FILE__, __func__, __LINE__); \
} while (0)

class VhpiCbHdl : public virtual GpiCbHdl {
public:
    VhpiCbHdl(GpiImplInterface *impl); 
    virtual ~VhpiCbHdl() { }

    virtual int arm_callback(void);
    virtual int cleanup_callback(void);

protected:
    vhpiCbDataT cb_data;
    vhpiTimeT vhpi_time;
};

class VhpiSignalObjHdl;

class VhpiValueCbHdl : public VhpiCbHdl, public GpiValueCbHdl {
public:
    VhpiValueCbHdl(GpiImplInterface *impl, VhpiSignalObjHdl *sig, int edge);
    virtual ~VhpiValueCbHdl() { }
    int cleanup_callback(void) {
        return VhpiCbHdl::cleanup_callback();
    }
private:
    std::string initial_value;
    bool rising;
    bool falling;
    VhpiSignalObjHdl *signal;
};

class VhpiTimedCbHdl : public VhpiCbHdl {
public:
    VhpiTimedCbHdl(GpiImplInterface *impl, uint64_t time_ps);
    virtual ~VhpiTimedCbHdl() { }
    int cleanup_callback();
};

class VhpiReadOnlyCbHdl : public VhpiCbHdl {
public:
    VhpiReadOnlyCbHdl(GpiImplInterface *impl);
    virtual ~VhpiReadOnlyCbHdl() { }
};

class VhpiNextPhaseCbHdl : public VhpiCbHdl {
public:
    VhpiNextPhaseCbHdl(GpiImplInterface *impl);
    virtual ~VhpiNextPhaseCbHdl() { }
};

class VhpiStartupCbHdl : public VhpiCbHdl {
public:
    VhpiStartupCbHdl(GpiImplInterface *impl);
    int run_callback(void);
    int cleanup_callback(void) {
        /* Too many sims get upset with this so we override to do nothing */
        return 0;
    }
    virtual ~VhpiStartupCbHdl() { }
};

class VhpiShutdownCbHdl : public VhpiCbHdl {
public:
    VhpiShutdownCbHdl(GpiImplInterface *impl);
    int run_callback(void);
    int cleanup_callback(void) {
        /* Too many sims get upset with this so we override to do nothing */
        return 0;
    }
    virtual ~VhpiShutdownCbHdl() { }
};

class VhpiReadwriteCbHdl : public VhpiCbHdl {
public:
    VhpiReadwriteCbHdl(GpiImplInterface *impl);
    virtual ~VhpiReadwriteCbHdl() { }
};

class VhpiSignalObjHdl : public GpiSignalObjHdl {
public:
    VhpiSignalObjHdl(GpiImplInterface *impl, vhpiHandleT hdl, gpi_objtype_t objtype) :
                                                                GpiSignalObjHdl(impl, hdl, objtype),
                                                                m_size(0),
                                                                m_rising_cb(impl, this, GPI_RISING),
                                                                m_falling_cb(impl, this, GPI_FALLING),
                                                                m_either_cb(impl, this, GPI_FALLING | GPI_RISING) { }
    virtual ~VhpiSignalObjHdl();

    const char* get_signal_value_binstr(void);
    double get_signal_value_real(void);
    long get_signal_value_long(void);


    int set_signal_value(const long value);
    int set_signal_value(const double value);
    int set_signal_value(std::string &value);

    /* Value change callback accessor */
    GpiCbHdl *value_change_cb(unsigned int edge);
    int initialise(std::string &name);

    int get_num_elems(void);

private:
    const vhpiEnumT chr2vhpi(const char value);
    unsigned int m_size;
    vhpiValueT m_value;
    vhpiValueT m_binvalue;
    VhpiValueCbHdl m_rising_cb;
    VhpiValueCbHdl m_falling_cb;
    VhpiValueCbHdl m_either_cb;
};

class KindMappings {
public:
    KindMappings();

public:
    std::map<vhpiClassKindT, std::vector<vhpiOneToManyT> > options_map;
    std::vector<vhpiOneToManyT>* get_options(vhpiClassKindT type);

private:
    void add_to_options(vhpiClassKindT type, vhpiOneToManyT *options);
};

class VhpiIterator : public GpiIterator {
public:
    VhpiIterator(GpiImplInterface *impl, vhpiHandleT hdl);

    virtual ~VhpiIterator();

    GpiObjHdl *next_handle(void);

private:
    vhpiHandleT m_iterator;
    vhpiHandleT m_iter_obj;
    static KindMappings iterate_over;      /* Possible mappings */
    std::vector<vhpiOneToManyT> *selected; /* Mapping currently in use */
    std::vector<vhpiOneToManyT>::iterator one2many;
};

class VhpiImpl : public GpiImplInterface {
public:
    VhpiImpl(const std::string& name) : GpiImplInterface(name),
                                        m_read_write(this),
                                        m_next_phase(this),
                                        m_read_only(this) { }

     /* Sim related */
    void sim_end(void);
    void get_sim_time(uint32_t *high, uint32_t *low);

    /* Hierachy related */
    GpiObjHdl *get_root_handle(const char *name);
    GpiIterator *iterate_handle(GpiObjHdl *obj_hdl);
    GpiObjHdl *next_handle(GpiIterator *iter);

    /* Callback related, these may (will) return the same handle*/
    GpiCbHdl *register_timed_callback(uint64_t time_ps);
    GpiCbHdl *register_readonly_callback(void);
    GpiCbHdl *register_nexttime_callback(void);
    GpiCbHdl *register_readwrite_callback(void);
    int deregister_callback(GpiCbHdl *obj_hdl);
    GpiObjHdl* native_check_create(std::string &name, GpiObjHdl *parent);
    GpiObjHdl* native_check_create(uint32_t index, GpiObjHdl *parent);

    const char * reason_to_string(int reason);
    const char * format_to_string(int format);

    GpiObjHdl *create_gpi_obj_from_handle(vhpiHandleT new_hdl, std::string &name);
private:
    VhpiReadwriteCbHdl m_read_write;
    VhpiNextPhaseCbHdl m_next_phase;
    VhpiReadOnlyCbHdl m_read_only;
};

#endif /*COCOTB_VHPI_IMPL_H_  */
