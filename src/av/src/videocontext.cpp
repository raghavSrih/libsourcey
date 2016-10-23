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


#include "scy/av/videocontext.h"

#ifdef HAVE_FFMPEG

#include "scy/logger.h"


using std::endl;


namespace scy {
namespace av {


VideoContext::VideoContext() :
    stream(nullptr),
    ctx(nullptr),
    codec(nullptr),
    frame(nullptr),
    conv(nullptr),
    pts(0.0)
{
    TraceS(this) << "Create" << endl;
    initializeFFmpeg();
}


VideoContext::~VideoContext()
{
    TraceS(this) << "Destroy" << endl;

    close();
    uninitializeFFmpeg();
}


void VideoContext::create()
{
}


void VideoContext::open()
{
    TraceS(this) << "Opening" << endl;
    assert(ctx);
    assert(codec);

    // Open the video codec
    if (avcodec_open2(ctx, codec, nullptr) < 0)
           throw std::runtime_error("Cannot open the video codec.");
}


void VideoContext::close()
{
    TraceS(this) << "Closing" << endl;

    if (frame) {
        av_free(frame);
        frame = nullptr;
    }

    if (ctx) {
        avcodec_close(ctx);
        ctx = nullptr;
    }

    freeConverter();

    // Streams are managed differently by the external impl
    //if (stream)    {
        //stream = nullptr;
        // Note: The stream is managed by the AVFormatContext
        //av_freep(stream);
    //}

    // pts = 0.0;
    error = "";

    TraceS(this) << "Closing: OK" << endl;
}


AVFrame* VideoContext::convert(AVFrame* iframe) //, VideoCodec& cparams
{
    // While flushing the input frame may be null
    if (!iframe)
        return nullptr;

    assert(iframe->width == iparams.width);
    assert(iframe->height == iparams.height);

    // Recreate the video conversion context on the fly
    // if the input resolution changes.
    if (iframe->width != /*conv->*/oparams.width ||
        iframe->height != /*conv->*/oparams.height) {
        iparams.width = iframe->width;
        iparams.height = iframe->height;
        recreateConverter();
    }

    // Return the input frame if no conversion is required
    if (!conv)
        return iframe;

    // // Set the input PTS or a monotonic value to keep the encoder happy.
    // // The actual setting of the PTS is outside the scope of this encoder.
    // cframe->pts = iframe->pts != AV_NOPTS_VALUE ? iframe->pts : ctx->frame_number;

    // Convert the input frame and return the result
    return conv->convert(iframe);
}


bool VideoContext::recreateConverter()
{
    // if (conv)
    //     throw std::runtime_error("Conversion context already exists.");

    // NOTE: the input output `width`, `height`, and `pixelFmt` parameters work
    // slightly differently for encoders and decoders.
    // For encoders `iparams` is the picture format from the application and
    // `oparams` is the picture format passed into the encoder.
    // For decoders `iparams` is the picture format from the decoder and
    // `oparams` is the picture format passed into the application.

    // Check if conversion is required
    if (iparams.width == oparams.width &&
        iparams.height == oparams.height &&
        iparams.pixelFmt == oparams.pixelFmt) {
        return false;
    }

    // Check if the conversion context needs to be recreated
    if (conv && (
        conv->iparams.width == iparams.width &&
        conv->iparams.height == iparams.height &&
        conv->iparams.pixelFmt == iparams.pixelFmt) && (
        conv->oparams.width == oparams.width &&
        conv->oparams.height == oparams.height &&
        conv->oparams.pixelFmt == oparams.pixelFmt)) {
        return false;
    }

    // Recreate the conversion context
    DebugL << "Recreating video conversion context" << endl;
    freeConverter();
    conv = new VideoConversionContext();
    conv->iparams = iparams;
    conv->oparams = oparams;
    conv->create();
    return true;
}


void VideoContext::freeConverter()
{
    if (conv) {
        delete conv;
        conv = nullptr;
    }

    //if (frame) {
    //    av_free(frame);
    //    frame = nullptr;
    //}
}


//
// Helper functions
//


AVFrame* createVideoFrame(AVPixelFormat pixelFmt, int width, int height)
{
    auto picture = av_frame_alloc();
    if (!picture)
        return nullptr;

    int size = av_image_get_buffer_size(pixelFmt, width, height, 16);
    auto buffer = reinterpret_cast<std::uint8_t*>(av_malloc(size));
    if (!buffer) {
        av_free(picture);
        return nullptr;
    }

    av_image_fill_arrays(picture->data, picture->linesize, buffer, pixelFmt, width, height, 1);

    // FFmpeg v3.1.4 does not set width and height values for us anymore
    picture->width = width;
    picture->height = height;

    return picture;
}


void initVideoCodecFromContext(const AVCodecContext* ctx, VideoCodec& params)
{
    params.enabled = true;
    params.encoder = avcodec_get_name(ctx->codec_id);
    params.pixelFmt = av_get_pix_fmt_name(ctx->pix_fmt);
    params.width = ctx->width;
    params.height = ctx->height;
    params.sampleRate = ctx->sample_rate;
    params.bitRate = ctx->bit_rate;
    params.fps =
        ctx->time_base.den /
        ctx->time_base.num;
}


} } // namespace scy::av


#endif
