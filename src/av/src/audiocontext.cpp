//
// LibSourcey
// Copyright (C) 2005, Sourcey <http://sourcey.com>
//
// LibSourcey is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
//
// LibSourcey is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
//


#include "scy/av/audiocontext.h"
#include "scy/av/audioresampler.h"


#ifdef HAVE_FFMPEG

#include "scy/logger.h"
#include "scy/av/ffmpeg.h"


using std::endl;


namespace scy {
namespace av {


AudioContext::AudioContext() :
    stream(nullptr),
    codec(nullptr),
    frame(nullptr),
    resampler(nullptr),
    pts(0)
{
    initializeFFmpeg();
}


AudioContext::~AudioContext()
{
    close();
    uninitializeFFmpeg();
}


// void AudioContext::open()
// {
// }
//
//
// void AudioContext::open()
// {
//     TraceS(this) << "Opening" << endl;
//     assert(ctx);
//     assert(codec);
//
//     // Open the audio codec
//     if (avcodec_open2(ctx, codec, nullptr) < 0)
//          throw std::runtime_error("Cannot open the audio codec.");
//
//     // Create the resampler if resampling is required
//     if (iparams.channels != oparams.channels ||
//         iparams.sampleRate != oparams.sampleRate ||
//         iparams.sampleFmt != oparams.sampleFmt) {
//         recreateResampler();
//     }
// }


void AudioContext::close()
{
    if (frame) {
        // av_free(frame);
        av_frame_free(&frame);
        frame = nullptr;
    }

    if (ctx) {
        avcodec_close(ctx);
        ctx = nullptr;
    }

    if (stream) {
        // The stream pointer is managed by the AVFormatContext
        stream = nullptr;
    }

    if (resampler) {
        delete resampler;
        resampler = nullptr;
    }

    pts = 0;
    //ptsSeconds = 0.0;
    error = "";
}


double AudioContext::ptsSeconds()
{
    double val = 0.0;

    // Local PTS value represented as decimal seconds
    // if (opacket->dts != AV_NOPTS_VALUE) {
    //     *pts = (double)opacket->pts;
    //     *pts *= av_q2d(stream->time_base);
    // }

    // Local PTS value represented as decimal seconds
    if (stream && pts > 0 && pts != AV_NOPTS_VALUE) {
        val = (double)pts;
        val *= av_q2d(stream->time_base);
    }

    return val;
}


// if (iparams.sampleFmt != oparams.sampleFmt ||
//     iparams.sampleRate != oparams.sampleRate ||
//     iparams.channels != oparams.channels) {

// AVFrame* VideoContext::resample(AVFrame* iframe) //, VideoCodec& cparams
// {
//     // While flushing the input frame may be null
//     if (!iframe)
//         return nullptr;
//
//     assert(iframe->channels == iparams.channels);
//     assert(iframe->sampleRate == iparams.sampleRate);
//
//     // Recreate the video resampler context on the fly
//     // if the input resolution changes.
//     if (iframe->channels != /*resampler->*/oparams.channels ||
//         iframe->sampleRate != /*resampler->*/oparams.sampleRate) {
//         iparams.channels = iframe->channels;
//         iparams.sampleRate = iframe->sampleRate;
//         recreateResampler();
//     }
//
//     // Return the input frame if no resampler is required
//     if (!resampler)
//         return iframe;
//
//     // // Set the input PTS or a monotonic value to keep the encoder happy.
//     // // The actual setting of the PTS is outside the scope of this encoder.
//     // cframe->pts = iframe->pts != AV_NOPTS_VALUE ? iframe->pts : ctx->frame_number;
//
//     // Convert the input frame and return the result
//     return resampler->resamplerert(iframe);
// }


bool AudioContext::recreateResampler()
{
    // if (resampler)
    //     throw std::runtime_error("Conversion context already exists.");

    // NOTE: the input output `channels`, `sampleRate`, and `sampleFmt` parameters work
    // slightly differently for encoders and decoders.
    // For encoders `iparams` is the picture format from the application and
    // `oparams` is the picture format passed into the encoder.
    // For decoders `iparams` is the picture format from the decoder and
    // `oparams` is the picture format passed into the application.

    // // Check if resampler is required
    // if (iparams.channels == oparams.channels &&
    //     iparams.sampleRate == oparams.sampleRate &&
    //     iparams.sampleFmt == oparams.sampleFmt) {
    //     return false;
    // }
    //
    // // Check if the resampler context needs to be recreated
    // if (resampler && (
    //     resampler->iparams.channels == iparams.channels &&
    //     resampler->iparams.sampleRate == iparams.sampleRate &&
    //     resampler->iparams.sampleFmt == iparams.sampleFmt) && (
    //     resampler->oparams.channels == oparams.channels &&
    //     resampler->oparams.sampleRate == oparams.sampleRate &&
    //     resampler->oparams.sampleFmt == oparams.sampleFmt)) {
    //     return false;
    // }

    // Recreate the resampler context
    DebugL << "Recreating audio resampler context" << endl;
    // freeResampler();
    if (resampler)
        delete resampler;
    resampler = new AudioResampler();
    resampler->iparams = iparams;
    resampler->oparams = oparams;
    resampler->open();
    return true;
}


//
// Helper functions
//


void initAudioCodecFromContext(const AVCodecContext* ctx, AudioCodec& params)
{
    params.enabled = true;
    params.encoder = avcodec_get_name(ctx->codec_id);
    params.sampleFmt = av_get_sample_fmt_name(ctx->sample_fmt);
    params.channels = ctx->channels;
    params.sampleRate = ctx->sample_rate;
    params.bitRate = ctx->bit_rate;
}


bool isSampleFormatSupported(AVCodec* codec, enum AVSampleFormat sampleFormat)
{
    const enum AVSampleFormat *p = codec->sample_fmts;
    while (*p != AV_SAMPLE_FMT_NONE) {
        if (*p == sampleFormat)
            return true;
        p++;
    }
    return false;
}


AVSampleFormat selectSampleFormat(AVCodec* codec, av::AudioCodec& params)
{
    enum AVSampleFormat compatible = AV_SAMPLE_FMT_NONE;
    enum AVSampleFormat requested = av_get_sample_fmt(params.sampleFmt.c_str());
    bool planar = av_sample_fmt_is_planar(requested);
    const enum AVSampleFormat *p = codec->sample_fmts;
    while (*p != AV_SAMPLE_FMT_NONE) {
        if (compatible == AV_SAMPLE_FMT_NONE && av_sample_fmt_is_planar(*p) == planar)
            compatible = *p;  // or use the first compatible format
        if (*p == requested)
            return requested; // always try to return requested format
        p++;
    }
    return compatible;
}


} } // namespace scy::av


#endif
