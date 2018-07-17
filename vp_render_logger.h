//
// Created by pauli on 11/3/17.
//
/******************************************************************************
    Copyright (C) 2017 by Vidpresso Inc.
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#ifndef VPSTREAMER_VP_RENDER_LOGGER_H
#define VPSTREAMER_VP_RENDER_LOGGER_H

#include <string>
#include <mutex>

class VPRenderLogger;


// global logger object
extern VPRenderLogger *g_vpRenderLogger;



class VPRenderLogger {
public:
    VPRenderLogger(std::string *logDir, double startT);
    ~VPRenderLogger();

    typedef enum {
        VP_RENDERLOG_STATUS = 0,
        VP_RENDERLOG_VIDEO = 1,
        VP_RENDERLOG_AUDIO,
    } VPRenderLogType;

    void writeText(VPRenderLogType logType, const char *text);

private:
    std::string *m_logDir;
    int m_logSegmentIndex;
    FILE *m_filePtr;
    double m_startT;
    std::mutex m_logMutex;

    void openSegmentIfNeeded(double t);
};


#endif //VPSTREAMER_VP_RENDER_LOGGER_H
