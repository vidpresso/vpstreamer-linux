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

#include "vp_render_logger.h"
#include <cstring>
#include <sstream>
#include <cmath>
#include "time_utils.h"


VPRenderLogger *g_vpRenderLogger = nullptr;


VPRenderLogger::VPRenderLogger(std::string *logDir, double startT)
{
    m_logDir = logDir;
    m_startT = startT;
    m_logSegmentIndex = -1;
    m_filePtr = NULL;
}

VPRenderLogger::~VPRenderLogger()
{
    if (m_filePtr) {
        fclose(m_filePtr);
        m_filePtr = NULL;
    }
}

void VPRenderLogger::openSegmentIfNeeded(double t)
{
    int segmentIdx = (int)floor(t / 10.0);

    if (segmentIdx != m_logSegmentIndex) {
        m_logSegmentIndex = segmentIdx;

        if (m_filePtr) {
            fflush(m_filePtr);
            fclose(m_filePtr);
            m_filePtr = NULL;
        }

        if ( !m_logDir->empty()) {
            std::stringstream ss;
            ss << *m_logDir << "/streamlogsegment_" << m_logSegmentIndex;

            m_filePtr = fopen(ss.str().c_str(), "wb");

            fprintf(m_filePtr, "%.3f | s | Started log segment %d\n", t, m_logSegmentIndex);
        }
    }
}

void VPRenderLogger::writeText(VPRenderLogType logType, const char *text)
{
    std::lock_guard<std::mutex> guard(m_logMutex);

    double t = VPMonotonicTime() - m_startT;

    this->openSegmentIfNeeded(t);

    if ( !m_filePtr)
        return;

    const char *typeStr = "";
    switch (logType) {
        case VP_RENDERLOG_STATUS: typeStr = "s"; break;
        case VP_RENDERLOG_VIDEO:  typeStr = "v"; break;
        case VP_RENDERLOG_AUDIO:  typeStr = "a"; break;
    }

    fprintf(m_filePtr, "%.3f | %s | %s\n", t, typeStr, text);
}
