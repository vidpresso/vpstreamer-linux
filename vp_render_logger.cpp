//
// Created by pauli on 11/3/17.
//

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
