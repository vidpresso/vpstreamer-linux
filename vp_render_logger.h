//
// Created by pauli on 11/3/17.
//

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
