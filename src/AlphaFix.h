#pragma once

#include "AEConfig.h"
#include "entry.h"

#ifdef _WIN32
    #include <windows.h>
#endif

#include "AE_Effect.h"
#include "AE_EffectCB.h"
#include "AE_EffectCBSuites.h"
#include "AE_Macros.h"
#include "AEGP_SuiteHandler.h"
#include "Param_Utils.h"
#include "Smart_Utils.h"
#include "String_Utils.h"
#include "AEFX_SuiteHelper.h"

#ifdef _WIN32
    #define ALPHAFIX_API __declspec(dllexport)
#else
    #define ALPHAFIX_API __attribute__((visibility("default")))
#endif

#define ALPHAFIX_NAME           "AlphaFix"
#define ALPHAFIX_MATCH_NAME     "GuyDev_AlphaFix"
#define ALPHAFIX_MAJOR_VERSION  1
#define ALPHAFIX_MINOR_VERSION  0
#define ALPHAFIX_BUG_VERSION    0
#define ALPHAFIX_BUILD_VERSION  1

enum {
    ALPHAFIX_INPUT = 0,
    ALPHAFIX_ENABLE,
    ALPHAFIX_ALPHA_ONLY,
    ALPHAFIX_BLEND_MODE,
    ALPHAFIX_REMOVE_COLOR_MATTING,
    ALPHAFIX_FRAME_OFFSET,
    ALPHAFIX_DEBUG_LOG,
    ALPHAFIX_NUM_PARAMS
};

enum AlphaBlendMode {
    BLEND_REPLACE = 0,
    BLEND_MULTIPLY,
    BLEND_MAX,
    BLEND_NUM_MODES
};

struct AlphaFixPreRenderData {
    PF_Boolean  enabled;
    PF_Boolean  alphaOnly;
    A_long      blendMode;
    PF_Boolean  removeColorMatting;
    A_long      frameOffset;
    PF_Boolean  debugLog;
    PF_LRect    result_rect;    // Stashed from checkout during PreRender.
                                // Used in SmartRender to offset alpha buffer
                                // lookups when AE gives us a sub-region world
                                // (happens when other effects are in the stack).
};

struct AlphaFixInstanceData {
    char        filePath[2048];
    A_long      lastFrame;
    A_long      cachedWidth;
    A_long      cachedHeight;
    PF_Boolean  isValid;
    void*       ffmpegContext;
};

extern "C" {

ALPHAFIX_API PF_Err PluginDataEntryFunction(
    PF_PluginDataPtr    inPtr,
    PF_PluginDataCB     inPluginDataCallBackPtr,
    SPBasicSuite*       inSPBasicSuitePtr,
    const char*         inHostName,
    const char*         inHostVersion
);

ALPHAFIX_API PF_Err EffectMain(
    PF_Cmd          cmd,
    PF_InData*      in_data,
    PF_OutData*     out_data,
    PF_ParamDef*    params[],
    PF_LayerDef*    output,
    void*           extra
);

}