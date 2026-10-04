/* Original development-environment probe. No Sopwith engine code is included. */
#include "pd_api.h"

static PlaydateAPI *pd;
static unsigned int frames;
static unsigned int last_report_ms;
static unsigned int report_frames;
static unsigned int crank_samples;

static int update(void *userdata)
{
    (void)userdata;
    PDButtons held, pushed, released;
    pd->system->getButtonState(&held, &pushed, &released);
    unsigned int now = pd->system->getCurrentTimeMilliseconds();
    float crank = pd->system->getCrankChange();
    ++frames;
    if (crank > 0.1f || crank < -0.1f) {
        ++crank_samples;
    }
    if (pushed || released) {
        pd->system->logToConsole("SDKCHECK buttons held=%u pushed=%u released=%u",
                                (unsigned int)held, (unsigned int)pushed,
                                (unsigned int)released);
    }
    if (frames == 1 || now - last_report_ms >= 5000) {
        pd->system->logToConsole(
            "SDKCHECK alive frames=%u interval_ms=%u interval_frames=%u crank_samples=%u docked=%d",
            frames, now - last_report_ms, frames - report_frames,
            crank_samples, pd->system->isCrankDocked());
        last_report_ms = now;
        report_frames = frames;
    }
    pd->graphics->clear((held & kButtonA) ? kColorBlack : kColorWhite);
    pd->graphics->setDrawMode((held & kButtonA) ? kDrawModeFillWhite : kDrawModeCopy);
    static const char message[] =
        "Sopwith SDK check - build 3\n"
        "C update callback is running\n"
        "Hold A to invert\n"
        "Try D-pad, B, crank, and Menu\n"
        "Game port is not implemented yet";
    pd->graphics->drawText(message, sizeof(message) - 1, kASCIIEncoding, 20, 30);
    pd->system->drawFPS(20, 170);
    return 1;
}

#ifdef _WIN32
__declspec(dllexport)
#endif
int eventHandler(PlaydateAPI *playdate, PDSystemEvent event, uint32_t arg)
{
    (void)arg;
    if (event == kEventInit) {
        pd = playdate;
        frames = report_frames = crank_samples = 0;
        last_report_ms = pd->system->getCurrentTimeMilliseconds();
        pd->display->setRefreshRate(30);
        pd->system->setUpdateCallback(update, NULL);
        pd->system->logToConsole("SDKCHECK initialized build=3");
    } else if (pd != NULL) {
        if (event == kEventPause || event == kEventLock) {
            pd->system->logToConsole("SDKCHECK suspended event=%d frames=%u",
                                    (int)event, frames);
        } else if (event == kEventResume || event == kEventUnlock) {
            last_report_ms = pd->system->getCurrentTimeMilliseconds();
            report_frames = frames;
            pd->system->logToConsole("SDKCHECK resumed event=%d frames=%u",
                                    (int)event, frames);
        }
    }
    return 0;
}
