#include "AlphaFix.h"
#include "AlphaFixDecoder.h"

#include <cstring>
#include <cstdio>
#include <cmath>
#include <string>


// ═══════════════════════════════════════════════════════════════════════════════
// Handle Suite Helpers
// ═══════════════════════════════════════════════════════════════════════════════

static PF_Handle AllocHandle(PF_InData* in_data, size_t size)
{
    AEFX_SuiteScoper<PF_HandleSuite1> suite(
        in_data, kPFHandleSuite, kPFHandleSuiteVersion1, nullptr);
    return suite->host_new_handle(static_cast<A_HandleSize>(size));
}

static void* LockHandle(PF_InData* in_data, PF_Handle h)
{
    AEFX_SuiteScoper<PF_HandleSuite1> suite(
        in_data, kPFHandleSuite, kPFHandleSuiteVersion1, nullptr);
    return suite->host_lock_handle(h);
}

static void UnlockHandle(PF_InData* in_data, PF_Handle h)
{
    AEFX_SuiteScoper<PF_HandleSuite1> suite(
        in_data, kPFHandleSuite, kPFHandleSuiteVersion1, nullptr);
    suite->host_unlock_handle(h);
}

static void DisposeHandle(PF_InData* in_data, PF_Handle h)
{
    AEFX_SuiteScoper<PF_HandleSuite1> suite(
        in_data, kPFHandleSuite, kPFHandleSuiteVersion1, nullptr);
    suite->host_dispose_handle(h);
}


// ═══════════════════════════════════════════════════════════════════════════════
// Plugin Data Entry
// ═══════════════════════════════════════════════════════════════════════════════

extern "C" ALPHAFIX_API PF_Err PluginDataEntryFunction(
    PF_PluginDataPtr    inPtr,
    PF_PluginDataCB     inPluginDataCallBackPtr,
    SPBasicSuite*       inSPBasicSuitePtr,
    const char*         inHostName,
    const char*         inHostVersion)
{
    PF_Err result = PF_Err_NONE;
    result = PF_REGISTER_EFFECT(
        inPtr, inPluginDataCallBackPtr,
        ALPHAFIX_NAME, ALPHAFIX_MATCH_NAME,
        "Channel", AE_RESERVED_INFO);
    return result;
}


// ═══════════════════════════════════════════════════════════════════════════════
// Global Setup
// ═══════════════════════════════════════════════════════════════════════════════

static PF_Err GlobalSetup(PF_InData* in_data, PF_OutData* out_data)
{
    out_data->my_version = PF_VERSION(
        ALPHAFIX_MAJOR_VERSION, ALPHAFIX_MINOR_VERSION,
        ALPHAFIX_BUG_VERSION, PF_Stage_DEVELOP, ALPHAFIX_BUILD_VERSION);

    out_data->out_flags  = PF_OutFlag_DEEP_COLOR_AWARE;
    out_data->out_flags2 = PF_OutFlag2_SUPPORTS_SMART_RENDER |
                           PF_OutFlag2_FLOAT_COLOR_AWARE |
                           PF_OutFlag2_SUPPORTS_THREADED_RENDERING;
    return PF_Err_NONE;
}


// ═══════════════════════════════════════════════════════════════════════════════
// Params Setup
// ═══════════════════════════════════════════════════════════════════════════════

static PF_Err ParamsSetup(PF_InData* in_data, PF_OutData* out_data)
{
    PF_Err      err = PF_Err_NONE;
    PF_ParamDef def;

    AEFX_CLR_STRUCT(def);
    PF_ADD_CHECKBOX("Enable", "Fix Alpha", TRUE, 0, ALPHAFIX_ENABLE);

    AEFX_CLR_STRUCT(def);
    PF_ADD_CHECKBOX("Alpha Only", "Show Alpha", FALSE, 0, ALPHAFIX_ALPHA_ONLY);

    AEFX_CLR_STRUCT(def);
    PF_ADD_POPUP("Alpha Mode", BLEND_NUM_MODES, BLEND_REPLACE + 1,
        "Replace|Multiply|Maximum", ALPHAFIX_BLEND_MODE);

    AEFX_CLR_STRUCT(def);
    PF_ADD_CHECKBOX("Remove Color Matting", "", TRUE, 0, ALPHAFIX_REMOVE_COLOR_MATTING);

    AEFX_CLR_STRUCT(def);
    PF_ADD_SLIDER("Frame Offset", -100, 100, -10, 10, 0, ALPHAFIX_FRAME_OFFSET);

    AEFX_CLR_STRUCT(def);
    PF_ADD_CHECKBOX("Debug Log", "Write to %TEMP%", FALSE, 0, ALPHAFIX_DEBUG_LOG);

    out_data->num_params = ALPHAFIX_NUM_PARAMS;
    return err;
}


// ═══════════════════════════════════════════════════════════════════════════════
// Helper: Get source file path via AEGP
// ═══════════════════════════════════════════════════════════════════════════════

static PF_Err GetLayerSourceFilePath(PF_InData* in_data, char* outPath, A_long maxPathLen)
{
    PF_Err err = PF_Err_NONE;
    AEGP_SuiteHandler suites(in_data->pica_basicP);

    AEGP_PluginID pluginId = 0;
    ERR(suites.UtilitySuite6()->AEGP_RegisterWithAEGP(nullptr, "AlphaFix", &pluginId));
    if (err) return err;

    AEGP_EffectRefH effectRef = nullptr;
    ERR(suites.PFInterfaceSuite1()->AEGP_GetNewEffectForEffect(
        pluginId, in_data->effect_ref, &effectRef));
    if (err || !effectRef) return err;

    AEGP_LayerH layerH = nullptr;
    ERR(suites.PFInterfaceSuite1()->AEGP_GetEffectLayer(
        in_data->effect_ref, &layerH));

    if (!err && layerH) {
        AEGP_ItemH itemH = nullptr;
        ERR(suites.LayerSuite8()->AEGP_GetLayerSourceItem(layerH, &itemH));

        if (!err && itemH) {
            AEGP_ItemType itemType;
            ERR(suites.ItemSuite9()->AEGP_GetItemType(itemH, &itemType));

            if (!err && itemType == AEGP_ItemType_FOOTAGE) {
                AEGP_FootageH footageH = nullptr;
                ERR(suites.FootageSuite5()->AEGP_GetMainFootageFromItem(itemH, &footageH));

                if (!err && footageH) {
                    AEGP_MemHandle pathMemH = nullptr;
                    ERR(suites.FootageSuite5()->AEGP_GetFootagePath(
                        footageH, 0, AEGP_FOOTAGE_MAIN_FILE_INDEX, &pathMemH));

                    if (!err && pathMemH) {
                        A_UTF16Char* utf16Path = nullptr;
                        ERR(suites.MemorySuite1()->AEGP_LockMemHandle(
                            pathMemH, reinterpret_cast<void**>(&utf16Path)));
                        if (!err && utf16Path) {
#ifdef _WIN32
                            WideCharToMultiByte(CP_UTF8, 0,
                                reinterpret_cast<LPCWSTR>(utf16Path), -1,
                                outPath, maxPathLen, nullptr, nullptr);
#else
                            outPath[0] = '\0';
#endif
                            suites.MemorySuite1()->AEGP_UnlockMemHandle(pathMemH);
                        }
                        suites.MemorySuite1()->AEGP_FreeMemHandle(pathMemH);
                    }
                }
            }
        }
    }

    suites.EffectSuite4()->AEGP_DisposeEffect(effectRef);
    return err;
}


// ═══════════════════════════════════════════════════════════════════════════════
// Bit-depth aware helpers
// ═══════════════════════════════════════════════════════════════════════════════

static int GetBitDepth(PF_EffectWorld* world)
{
    if (world->rowbytes >= world->width * 16)
        return 32;
    if (PF_WORLD_IS_DEEP(world))
        return 16;
    return 8;
}

static void CopyPixels(PF_EffectWorld* src, PF_EffectWorld* dst)
{
    A_long h = (dst->height < src->height) ? dst->height : src->height;
    for (A_long y = 0; y < h; y++) {
        char* inRow  = reinterpret_cast<char*>(src->data) + y * src->rowbytes;
        char* outRow = reinterpret_cast<char*>(dst->data) + y * dst->rowbytes;
        A_long copyBytes = (src->rowbytes < dst->rowbytes) ? src->rowbytes : dst->rowbytes;
        memcpy(outRow, inRow, copyBytes);
    }
}


// ═══════════════════════════════════════════════════════════════════════════════
// SmartFX: PreRender
// ═══════════════════════════════════════════════════════════════════════════════

static PF_Err PreRender(PF_InData* in_data, PF_OutData* out_data, PF_PreRenderExtra* extra)
{
    PF_Err err = PF_Err_NONE;

    PF_RenderRequest req = extra->input->output_request;
    PF_CheckoutResult checkout;
    req.preserve_rgb_of_zero_alpha = TRUE;

    ERR(extra->cb->checkout_layer(
        in_data->effect_ref, ALPHAFIX_INPUT, ALPHAFIX_INPUT,
        &req, in_data->current_time, in_data->time_step, in_data->time_scale, &checkout));

    if (!err) {
        extra->output->result_rect     = checkout.result_rect;
        extra->output->max_result_rect = checkout.max_result_rect;
    }

    PF_Handle preDataH = AllocHandle(in_data, sizeof(AlphaFixPreRenderData));
    if (preDataH) {
        AlphaFixPreRenderData* preData =
            reinterpret_cast<AlphaFixPreRenderData*>(LockHandle(in_data, preDataH));

        if (preData) {
            PF_ParamDef p;

            AEFX_CLR_STRUCT(p);
            ERR(PF_CHECKOUT_PARAM(in_data, ALPHAFIX_ENABLE,
                in_data->current_time, in_data->time_step, in_data->time_scale, &p));
            preData->enabled = p.u.bd.value;
            PF_CHECKIN_PARAM(in_data, &p);

            AEFX_CLR_STRUCT(p);
            ERR(PF_CHECKOUT_PARAM(in_data, ALPHAFIX_ALPHA_ONLY,
                in_data->current_time, in_data->time_step, in_data->time_scale, &p));
            preData->alphaOnly = p.u.bd.value;
            PF_CHECKIN_PARAM(in_data, &p);

            AEFX_CLR_STRUCT(p);
            ERR(PF_CHECKOUT_PARAM(in_data, ALPHAFIX_BLEND_MODE,
                in_data->current_time, in_data->time_step, in_data->time_scale, &p));
            preData->blendMode = p.u.pd.value - 1;
            PF_CHECKIN_PARAM(in_data, &p);

            AEFX_CLR_STRUCT(p);
            ERR(PF_CHECKOUT_PARAM(in_data, ALPHAFIX_REMOVE_COLOR_MATTING,
                in_data->current_time, in_data->time_step, in_data->time_scale, &p));
            preData->removeColorMatting = p.u.bd.value;
            PF_CHECKIN_PARAM(in_data, &p);

            AEFX_CLR_STRUCT(p);
            ERR(PF_CHECKOUT_PARAM(in_data, ALPHAFIX_FRAME_OFFSET,
                in_data->current_time, in_data->time_step, in_data->time_scale, &p));
            preData->frameOffset = p.u.sd.value;
            PF_CHECKIN_PARAM(in_data, &p);

            AEFX_CLR_STRUCT(p);
            ERR(PF_CHECKOUT_PARAM(in_data, ALPHAFIX_DEBUG_LOG,
                in_data->current_time, in_data->time_step, in_data->time_scale, &p));
            preData->debugLog = p.u.bd.value;
            PF_CHECKIN_PARAM(in_data, &p);

            UnlockHandle(in_data, preDataH);
        }
        extra->output->pre_render_data = preDataH;
    }

    return err;
}


// ═══════════════════════════════════════════════════════════════════════════════
// SmartFX: SmartRender — 8/16/32 bpc with pre-process unmult
// ═══════════════════════════════════════════════════════════════════════════════

static PF_Err SmartRender(PF_InData* in_data, PF_OutData* out_data, PF_SmartRenderExtra* extra)
{
    PF_Err err = PF_Err_NONE;

    PF_Handle preDataH = reinterpret_cast<PF_Handle>(extra->input->pre_render_data);
    if (!preDataH) return PF_Err_INTERNAL_STRUCT_DAMAGED;

    AlphaFixPreRenderData* preData =
        reinterpret_cast<AlphaFixPreRenderData*>(LockHandle(in_data, preDataH));
    if (!preData) return PF_Err_INTERNAL_STRUCT_DAMAGED;

    PF_EffectWorld* inputWorld = nullptr;
    ERR(extra->cb->checkout_layer_pixels(in_data->effect_ref, ALPHAFIX_INPUT, &inputWorld));

    PF_EffectWorld* outputWorld = nullptr;
    ERR(extra->cb->checkout_output(in_data->effect_ref, &outputWorld));

    if (err || !inputWorld || !outputWorld) {
        UnlockHandle(in_data, preDataH);
        return err ? err : PF_Err_INTERNAL_STRUCT_DAMAGED;
    }

    if (!preData->enabled) {
        CopyPixels(inputWorld, outputWorld);
        UnlockHandle(in_data, preDataH);
        return PF_Err_NONE;
    }

    char filePath[2048] = {0};
    PF_Err pathErr = GetLayerSourceFilePath(in_data, filePath, sizeof(filePath));

    if (pathErr || filePath[0] == '\0') {
        CopyPixels(inputWorld, outputWorld);
        UnlockHandle(in_data, preDataH);
        return PF_Err_NONE;
    }

    AlphaFixDecoder::Context* decoder = AlphaFixDecoder::Create(filePath);
    if (!decoder) {
        CopyPixels(inputWorld, outputWorld);
        UnlockHandle(in_data, preDataH);
        return PF_Err_NONE;
    }
    decoder->debugLog = preData->debugLog ? true : false;

    // Compute frame number from AE time values.
    // current_time and time_scale are integers — use them directly to avoid
    // float truncation errors (e.g. 122.9999 → 122 instead of 123).
    // Round instead of truncate: add 0.5 before casting.
    A_long frameNum = static_cast<A_long>(
        (static_cast<double>(in_data->current_time) /
         static_cast<double>(in_data->time_scale)) * decoder->fps + 0.5);
    frameNum += preData->frameOffset;
    if (frameNum < 0) frameNum = 0;

    uint8_t* alphaBuffer = nullptr;
    A_long   alphaWidth  = 0;
    A_long   alphaHeight = 0;

    PF_Err decodeErr = AlphaFixDecoder::DecodeAlpha(
        decoder, frameNum, &alphaBuffer, &alphaWidth, &alphaHeight);

    if (decodeErr || !alphaBuffer) {
        AlphaFixDecoder::Destroy(decoder);
        CopyPixels(inputWorld, outputWorld);
        UnlockHandle(in_data, preDataH);
        return PF_Err_NONE;
    }

    int bitDepth = GetBitDepth(outputWorld);
    A_long outW = outputWorld->width;
    A_long outH = outputWorld->height;

    for (A_long y = 0; y < outH; y++) {
        A_long srcY = (alphaHeight != outH)
            ? static_cast<A_long>((static_cast<double>(y) / outH) * alphaHeight) : y;
        if (srcY >= alphaHeight) srcY = alphaHeight - 1;

        if (bitDepth == 32) {
            PF_PixelFloat* inRow = reinterpret_cast<PF_PixelFloat*>(
                reinterpret_cast<char*>(inputWorld->data) + y * inputWorld->rowbytes);
            PF_PixelFloat* outRow = reinterpret_cast<PF_PixelFloat*>(
                reinterpret_cast<char*>(outputWorld->data) + y * outputWorld->rowbytes);

            for (A_long x = 0; x < outW; x++) {
                A_long srcX = (alphaWidth != outW)
                    ? static_cast<A_long>((static_cast<double>(x) / outW) * alphaWidth) : x;
                if (srcX >= alphaWidth) srcX = alphaWidth - 1;

                PF_FpShort correctAlphaF = alphaBuffer[srcY * alphaWidth + srcX] / 255.0f;
                PF_FpShort r = inRow[x].red, g = inRow[x].green, b = inRow[x].blue;
                PF_FpShort oldA = inRow[x].alpha;

                if (preData->alphaOnly) {
                    outRow[x].red = outRow[x].green = outRow[x].blue = correctAlphaF;
                    outRow[x].alpha = 1.0f;
                } else {
                    // Step 1: Apply alpha fix
                    PF_FpShort newA = correctAlphaF;
                    switch (preData->blendMode) {
                        case BLEND_REPLACE:  newA = correctAlphaF; break;
                        case BLEND_MULTIPLY: newA = oldA * correctAlphaF; break;
                        case BLEND_MAX:      newA = (correctAlphaF > oldA) ? correctAlphaF : oldA; break;
                        default:             newA = correctAlphaF; break;
                    }
                    outRow[x].alpha = newA;

                    // Step 2: Remove color matting using the CORRECTED alpha
                    if (preData->removeColorMatting && newA > 0.0f && newA < 1.0f) {
                        r /= newA; g /= newA; b /= newA;
                        if (r > 1.0f) r = 1.0f; if (g > 1.0f) g = 1.0f; if (b > 1.0f) b = 1.0f;
                    }
                    outRow[x].red = r; outRow[x].green = g; outRow[x].blue = b;
                }
            }

        } else if (bitDepth == 16) {
            PF_Pixel16* inRow = reinterpret_cast<PF_Pixel16*>(
                reinterpret_cast<char*>(inputWorld->data) + y * inputWorld->rowbytes);
            PF_Pixel16* outRow = reinterpret_cast<PF_Pixel16*>(
                reinterpret_cast<char*>(outputWorld->data) + y * outputWorld->rowbytes);

            for (A_long x = 0; x < outW; x++) {
                A_long srcX = (alphaWidth != outW)
                    ? static_cast<A_long>((static_cast<double>(x) / outW) * alphaWidth) : x;
                if (srcX >= alphaWidth) srcX = alphaWidth - 1;

                A_u_short correctA16 = static_cast<A_u_short>(
                    (static_cast<A_long>(alphaBuffer[srcY * alphaWidth + srcX]) * 32768) / 255);
                A_u_short r = inRow[x].red, g = inRow[x].green, b = inRow[x].blue;
                A_u_short oldA16 = inRow[x].alpha;

                if (preData->alphaOnly) {
                    outRow[x].red = outRow[x].green = outRow[x].blue = correctA16;
                    outRow[x].alpha = 32768;
                } else {
                    // Step 1: Apply alpha fix
                    A_u_short newA16 = correctA16;
                    switch (preData->blendMode) {
                        case BLEND_REPLACE:  newA16 = correctA16; break;
                        case BLEND_MULTIPLY: newA16 = (A_u_short)((A_long)oldA16 * correctA16 / 32768); break;
                        case BLEND_MAX:      newA16 = (correctA16 > oldA16) ? correctA16 : oldA16; break;
                        default:             newA16 = correctA16; break;
                    }
                    outRow[x].alpha = newA16;

                    // Step 2: Remove color matting using CORRECTED alpha
                    if (preData->removeColorMatting && newA16 > 0 && newA16 < 32768) {
                        A_long sR = (A_long)r * 32768 / newA16;
                        A_long sG = (A_long)g * 32768 / newA16;
                        A_long sB = (A_long)b * 32768 / newA16;
                        r = (A_u_short)(sR > 32768 ? 32768 : sR);
                        g = (A_u_short)(sG > 32768 ? 32768 : sG);
                        b = (A_u_short)(sB > 32768 ? 32768 : sB);
                    }
                    outRow[x].red = r; outRow[x].green = g; outRow[x].blue = b;
                }
            }

        } else {
            PF_Pixel* inRow = reinterpret_cast<PF_Pixel*>(
                reinterpret_cast<char*>(inputWorld->data) + y * inputWorld->rowbytes);
            PF_Pixel* outRow = reinterpret_cast<PF_Pixel*>(
                reinterpret_cast<char*>(outputWorld->data) + y * outputWorld->rowbytes);

            for (A_long x = 0; x < outW; x++) {
                A_long srcX = (alphaWidth != outW)
                    ? static_cast<A_long>((static_cast<double>(x) / outW) * alphaWidth) : x;
                if (srcX >= alphaWidth) srcX = alphaWidth - 1;

                uint8_t correctAlpha = alphaBuffer[srcY * alphaWidth + srcX];
                A_u_char r = inRow[x].red, g = inRow[x].green, b = inRow[x].blue;
                A_u_char oldAlpha = inRow[x].alpha;

                if (preData->alphaOnly) {
                    outRow[x].red = outRow[x].green = outRow[x].blue = correctAlpha;
                    outRow[x].alpha = 255;
                } else {
                    // Step 1: Apply alpha fix
                    A_u_char newAlpha = correctAlpha;
                    switch (preData->blendMode) {
                        case BLEND_REPLACE:  newAlpha = correctAlpha; break;
                        case BLEND_MULTIPLY: newAlpha = (A_u_char)((A_long)oldAlpha * correctAlpha / 255); break;
                        case BLEND_MAX:      newAlpha = (correctAlpha > oldAlpha) ? correctAlpha : oldAlpha; break;
                        default:             newAlpha = correctAlpha; break;
                    }
                    outRow[x].alpha = newAlpha;

                    // Step 2: Remove color matting using CORRECTED alpha
                    if (preData->removeColorMatting && newAlpha > 0 && newAlpha < 255) {
                        A_long sR = (A_long)r * 255 / newAlpha;
                        A_long sG = (A_long)g * 255 / newAlpha;
                        A_long sB = (A_long)b * 255 / newAlpha;
                        r = (A_u_char)(sR > 255 ? 255 : sR);
                        g = (A_u_char)(sG > 255 ? 255 : sG);
                        b = (A_u_char)(sB > 255 ? 255 : sB);
                    }
                    outRow[x].red = r; outRow[x].green = g; outRow[x].blue = b;
                }
            }
        }
    }

    AlphaFixDecoder::FreeAlphaBuffer(alphaBuffer);
    AlphaFixDecoder::Destroy(decoder);
    UnlockHandle(in_data, preDataH);

    return err;
}


// ═══════════════════════════════════════════════════════════════════════════════
// Legacy Render
// ═══════════════════════════════════════════════════════════════════════════════

static PF_Err Render(PF_InData* in_data, PF_OutData* out_data,
    PF_ParamDef* params[], PF_LayerDef* output)
{
    if (params[ALPHAFIX_INPUT]) {
        PF_LayerDef* input = &params[ALPHAFIX_INPUT]->u.ld;
        for (A_long y = 0; y < output->height; y++) {
            char* inRow  = reinterpret_cast<char*>(input->data)  + y * input->rowbytes;
            char* outRow = reinterpret_cast<char*>(output->data) + y * output->rowbytes;
            memcpy(outRow, inRow, output->rowbytes);
        }
    }
    return PF_Err_NONE;
}


// ═══════════════════════════════════════════════════════════════════════════════
// Main Dispatcher
// ═══════════════════════════════════════════════════════════════════════════════

extern "C" ALPHAFIX_API PF_Err EffectMain(
    PF_Cmd cmd, PF_InData* in_data, PF_OutData* out_data,
    PF_ParamDef* params[], PF_LayerDef* output, void* extra)
{
    PF_Err err = PF_Err_NONE;

    try {
        switch (cmd) {
            case PF_Cmd_ABOUT:
                sprintf_s(out_data->return_msg, sizeof(out_data->return_msg),
                    "%s v%d.%d\nFixes 8-bit ProRes 4444 alpha artifacts.\nby Guy Micciche",
                    ALPHAFIX_NAME, ALPHAFIX_MAJOR_VERSION, ALPHAFIX_MINOR_VERSION);
                break;
            case PF_Cmd_GLOBAL_SETUP:
                err = GlobalSetup(in_data, out_data);
                break;
            case PF_Cmd_GLOBAL_SETDOWN:
                return PF_Err_NONE;
            case PF_Cmd_PARAMS_SETUP:
                err = ParamsSetup(in_data, out_data);
                break;
            case PF_Cmd_SEQUENCE_SETUP:
            case PF_Cmd_SEQUENCE_RESETUP:
            case PF_Cmd_SEQUENCE_SETDOWN:
            case PF_Cmd_SEQUENCE_FLATTEN:
                break;
            case PF_Cmd_SMART_PRE_RENDER:
                err = PreRender(in_data, out_data,
                    reinterpret_cast<PF_PreRenderExtra*>(extra));
                break;
            case PF_Cmd_SMART_RENDER:
                err = SmartRender(in_data, out_data,
                    reinterpret_cast<PF_SmartRenderExtra*>(extra));
                break;
            case PF_Cmd_RENDER:
                err = Render(in_data, out_data, params, output);
                break;
            default:
                break;
        }
    }
    catch (PF_Err& thrown_err) { err = thrown_err; }
    catch (...) { err = PF_Err_INTERNAL_STRUCT_DAMAGED; }

    return err;
}