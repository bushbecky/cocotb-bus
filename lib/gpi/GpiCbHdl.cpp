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
*    * Neither the name of Potential Ventures Ltd,
*       SolarFlare Communications Inc nor the
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

#include "gpi_priv.h"

const char * GpiObjHdl::get_name_str(void)
{
    return m_name.c_str();
}

const char * GpiObjHdl::get_type_str(void)
{
    return m_type.c_str();
}

/* Genertic base clss implementations */
char *GpiHdl::gpi_copy_name(const char *name)
{
    int len;
    char *result;
    const char null[] = "NULL";

    if (name)
        len = strlen(name) + 1;
    else {
        LOG_CRITICAL("GPI: attempt to use NULL from impl");
        len = strlen(null);
        name = null;
    }

    result = (char *)malloc(len);
    if (result == NULL) {
        LOG_CRITICAL("GPI: Attempting allocate string buffer failed!");
        len = strlen(null);
        name = null;
    }

    snprintf(result, len, "%s", name);

    return result;
}

int GpiHdl::initialise(std::string name)
{
    LOG_WARN("Generic initialise, doubt you should have called this");
    return 0;
}

int GpiObjHdl::initialise(std::string name)
{
    m_name = name;
    return 0;
}

int GpiCbHdl::run_callback(void)
{
    LOG_WARN("Generic run_callback");
    return this->gpi_function(m_cb_data);
}

int GpiCbHdl::cleanup_callback(void)
{
    LOG_WARN("Generic cleanu_handler");
    return 0;
}

int GpiCbHdl::arm_callback(void)
{
    LOG_WARN("Generic arm_callback");
    return 0;
}

int GpiCbHdl::set_user_data(const int (*gpi_function)(const void*), const void *data)
{
    if (!gpi_function) {
        LOG_ERROR("gpi_function to set_user_data is NULL");
    }
    this->gpi_function = gpi_function;
    this->m_cb_data = data;
    return 0;
}

const void * GpiCbHdl::get_user_data(void)
{
    return m_cb_data;
}

void GpiCbHdl::set_call_state(gpi_cb_state_e new_state)
{
    m_state = new_state;
}

gpi_cb_state_e GpiCbHdl::get_call_state(void)
{
    return m_state;
}

GpiCbHdl::~GpiCbHdl(void)
{
    LOG_WARN("In GpiCbHdl Destructor");
}