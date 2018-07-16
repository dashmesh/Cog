#include "meta.h"
#include "../util.h"
#include "../coding/coding.h"
#include "../layout/layout.h"
#include "opus_interleave_streamfile.h"

/* Nintendo OPUS - from Switch games, including header variations (not the same as Ogg Opus) */

static VGMSTREAM * init_vgmstream_opus(STREAMFILE *streamFile, meta_t meta_type, off_t offset, int32_t num_samples, int32_t loop_start, int32_t loop_end) {
    VGMSTREAM * vgmstream = NULL;
    off_t start_offset;
    int loop_flag = 0, channel_count;
    off_t data_offset;
    size_t data_size, skip = 0;


    if ((uint32_t)read_32bitLE(offset + 0x00,streamFile) != 0x80000001)
        goto fail;

    channel_count = read_8bit(offset + 0x09, streamFile);
    /* 0x0a: packet size if CBR, 0 if VBR */
    data_offset = offset + read_32bitLE(offset + 0x10, streamFile);
    skip = read_32bitLE(offset + 0x1c, streamFile);

    if ((uint32_t)read_32bitLE(data_offset, streamFile) != 0x80000004)
        goto fail;

    data_size = read_32bitLE(data_offset + 0x04, streamFile);

    start_offset = data_offset + 0x08;
    loop_flag = (loop_end > 0); /* -1 when not set */


    /* build the VGMSTREAM */
    vgmstream = allocate_vgmstream(channel_count,loop_flag);
    if (!vgmstream) goto fail;

    vgmstream->sample_rate = read_32bitLE(offset + 0x0c,streamFile);
    vgmstream->meta_type = meta_OPUS;

    vgmstream->num_samples = num_samples;
    vgmstream->loop_start_sample = loop_start;
    vgmstream->loop_end_sample = loop_end;

#ifdef VGM_USE_FFMPEG
    {
        uint8_t buf[0x100];
        size_t bytes;
        ffmpeg_custom_config cfg = {0};
        ffmpeg_codec_data *ffmpeg_data;

        bytes = ffmpeg_make_opus_header(buf,0x100, vgmstream->channels, skip, vgmstream->sample_rate);
        if (bytes <= 0) goto fail;

        cfg.type = FFMPEG_SWITCH_OPUS;

        ffmpeg_data = init_ffmpeg_config(streamFile, buf,bytes, start_offset,data_size, &cfg);
        if (!ffmpeg_data) goto fail;

        vgmstream->codec_data = ffmpeg_data;
        vgmstream->coding_type = coding_FFmpeg;
        vgmstream->layout_type = layout_none;

        if (ffmpeg_data->skipSamples <= 0) {
            ffmpeg_set_skip_samples(ffmpeg_data, skip);
        }

        if (vgmstream->num_samples == 0) {
            vgmstream->num_samples = switch_opus_get_samples(start_offset, data_size,
                vgmstream->sample_rate, streamFile) - skip;
        }
    }
#else
    goto fail;
#endif

    /* open the file for reading */
    if ( !vgmstream_open_stream(vgmstream, streamFile, start_offset) )
        goto fail;
    return vgmstream;

fail:
    close_vgmstream(vgmstream);
    return NULL;
}


/* standard Switch Opus, Nintendo header + raw data (generated by opus_test.c?) [Lego City Undercover (Switch)] */
VGMSTREAM * init_vgmstream_opus_std(STREAMFILE *streamFile) {
	STREAMFILE * PSIFile = NULL;
    off_t offset = 0;
    int num_samples = 0, loop_start = 0, loop_end = 0;

    /* checks */
    if (!check_extensions(streamFile,"opus,lopus"))
        goto fail;

	/* BlazBlue: Cross Tag Battle (Switch) PSI Metadata for corresponding Opus */
	/* Maybe future Arc System Works games will use this too? */
	PSIFile = open_streamfile_by_ext(streamFile, "psi");

	offset = 0x00;

	if (PSIFile){
		num_samples = read_32bitLE(0x8C, PSIFile);
		loop_start = read_32bitLE(0x84, PSIFile);
		loop_end = read_32bitLE(0x88, PSIFile);
		close_streamfile(PSIFile);
	}
	else {
		num_samples = 0;
		loop_start = 0;
		loop_end = 0;
	}

    return init_vgmstream_opus(streamFile, meta_OPUS, offset, num_samples,loop_start,loop_end);
fail:
    return NULL;
}

/* Nippon1 variation [Disgaea 5 (Switch)] */
VGMSTREAM * init_vgmstream_opus_n1(STREAMFILE *streamFile) {
    off_t offset = 0;
    int num_samples = 0, loop_start = 0, loop_end = 0;

    /* checks */
    if ( !check_extensions(streamFile,"opus,lopus"))
        goto fail;

    if (!((read_32bitBE(0x04,streamFile) == 0x00000000 && read_32bitBE(0x0c,streamFile) == 0x00000000) ||
          (read_32bitBE(0x04,streamFile) == 0xFFFFFFFF && read_32bitBE(0x0c,streamFile) == 0xFFFFFFFF)))
        goto fail;

    offset = 0x10;
    num_samples = 0;
    loop_start = read_32bitLE(0x00,streamFile);
    loop_end = read_32bitLE(0x08,streamFile);

    return init_vgmstream_opus(streamFile, meta_OPUS, offset, num_samples,loop_start,loop_end);
fail:
    return NULL;
}

/* Capcom variation [Ultra Street Fighter II (Switch), Resident Evil: Revelations (Switch)] */
VGMSTREAM * init_vgmstream_opus_capcom(STREAMFILE *streamFile) {
    VGMSTREAM *vgmstream = NULL;
    off_t offset = 0;
    int num_samples = 0, loop_start = 0, loop_end = 0;
    int channel_count;

    /* checks */
    if ( !check_extensions(streamFile,"opus,lopus"))
        goto fail;

    channel_count = read_32bitLE(0x04,streamFile);
    if (channel_count != 1 && channel_count != 2 && channel_count != 6)
        goto fail; /* unknown stream layout */

    num_samples = read_32bitLE(0x00,streamFile);
    /* 0x04: channels, >2 uses interleaved streams (2ch+2ch+2ch) */
    loop_start = read_32bitLE(0x08,streamFile);
    loop_end = read_32bitLE(0x0c,streamFile);
    /* 0x10: frame size (with extra data) */
    /* 0x14: extra chunk count */
    /* 0x18: null */
    offset = read_32bitLE(0x1c,streamFile);
    /* 0x20-8: config? (0x0077C102 04000000 E107070C) */
    /* 0x2c: some size? */
    /* 0x30+: extra chunks (0x00: 0x7f, 0x04: num_sample), alt loop starts/regions? */

    if (channel_count == 6) {
        /* 2ch multistream hacky-hacks, don't try this at home. We'll end up with:
         * main vgmstream > N vgmstream layers > substream IO deinterleaver > opus meta > Opus IO transmogrifier (phew) */
        //todo deinterleave has some problems with reading after total_size
        layered_layout_data* data = NULL;
        int layers = channel_count / 2;
        int i;
        int loop_flag = (loop_end > 0);


        /* build the VGMSTREAM */
        vgmstream = allocate_vgmstream(channel_count,loop_flag);
        if (!vgmstream) goto fail;

        vgmstream->layout_type = layout_layered;

        /* init layout */
        data = init_layout_layered(layers);
        if (!data) goto fail;
        vgmstream->layout_data = data;

        /* open each layer subfile */
        for (i = 0; i < layers; i++) {
            STREAMFILE* temp_streamFile = setup_opus_interleave_streamfile(streamFile, offset+0x28*i, layers);
            if (!temp_streamFile) goto fail;

            data->layers[i] = init_vgmstream_opus(temp_streamFile, meta_OPUS, 0x00, num_samples,loop_start,loop_end);
            close_streamfile(temp_streamFile);
            if (!data->layers[i]) goto fail;
        }

        /* setup layered VGMSTREAMs */
        if (!setup_layout_layered(data))
            goto fail;

        vgmstream->sample_rate = data->layers[0]->sample_rate;
        vgmstream->num_samples = data->layers[0]->num_samples;
        vgmstream->loop_start_sample = data->layers[0]->loop_start_sample;
        vgmstream->loop_end_sample = data->layers[0]->loop_end_sample;
        vgmstream->meta_type = meta_OPUS;
        vgmstream->coding_type = data->layers[0]->coding_type;

        return vgmstream;
    }
    else {
        return init_vgmstream_opus(streamFile, meta_OPUS, offset, num_samples,loop_start,loop_end);
    }


fail:
    close_vgmstream(vgmstream);
    return NULL;
}

/* Procyon Studio variation [Xenoblade Chronicles 2 (Switch)] */
VGMSTREAM * init_vgmstream_opus_nop(STREAMFILE *streamFile) {
    off_t offset = 0;
    int num_samples = 0, loop_start = 0, loop_end = 0, loop_flag;

    /* checks */
    if (!check_extensions(streamFile,"nop"))
        goto fail;
    if (read_32bitBE(0x00, streamFile) != 0x73616466 || /* "sadf" */
        read_32bitBE(0x08, streamFile) != 0x6f707573)   /* "opus" */
        goto fail;

    offset = read_32bitLE(0x1c, streamFile);
    num_samples = read_32bitLE(0x28, streamFile);
    loop_flag = read_8bit(0x19, streamFile);
    if (loop_flag) {
        loop_start = read_32bitLE(0x2c, streamFile);
        loop_end = read_32bitLE(0x30, streamFile);
    }

    return init_vgmstream_opus(streamFile, meta_OPUS, offset, num_samples,loop_start,loop_end);
fail:
    return NULL;
}

/* Shin'en variation [Fast RMX (Switch)] */
VGMSTREAM * init_vgmstream_opus_shinen(STREAMFILE *streamFile) {
    off_t offset = 0;
    int num_samples = 0, loop_start = 0, loop_end = 0;

    /* checks */
    if ( !check_extensions(streamFile,"opus,lopus"))
        goto fail;

    if (read_32bitBE(0x08,streamFile) != 0x01000080)
        goto fail;

    offset = 0x08;
    num_samples = 0;
    loop_start = read_32bitLE(0x00,streamFile);
    loop_end = read_32bitLE(0x04,streamFile); /* 0 if no loop */

    if (loop_start > loop_end)
        goto fail; /* just in case */

    return init_vgmstream_opus(streamFile, meta_OPUS, offset, num_samples,loop_start,loop_end);
fail:
    return NULL;
}

/* Bandai Namco Opus (found in NUS3Banks) [Taiko no Tatsujin: Nintendo Switch Version!] */
VGMSTREAM * init_vgmstream_opus_nus3(STREAMFILE *streamFile) {
    off_t offset = 0;
    int num_samples = 0, loop_start = 0, loop_end = 0, loop_flag;

    /* checks */
    if (!check_extensions(streamFile, "lopus"))
        goto fail;
    if (read_32bitBE(0x00, streamFile) != 0x4F505553) /* "OPUS" */
        goto fail;

	/* Here's an interesting quirk, OPUS header contains big endian values
       while the Nintendo Opus header and data that follows remain little endian as usual */
    offset = read_32bitBE(0x20, streamFile);
    num_samples = read_32bitBE(0x08, streamFile);

	/* Check if there's a loop end value to determine loop_flag*/
    loop_flag = read_32bitBE(0x18, streamFile);
    if (loop_flag) {
        loop_start = read_32bitBE(0x14, streamFile);
        loop_end = read_32bitBE(0x18, streamFile);
    }

    return init_vgmstream_opus(streamFile, meta_OPUS, offset, num_samples, loop_start, loop_end);
fail:
    return NULL;
}

/* Nihon Falcom NLSD Opus [Ys VIII: Lacrimosa of Dana (Switch)]
   Similar to Nippon Ichi Software "AT9" Opus (Penny Punching Princess (Switch)
   Unlike Penny Punching Princess, this is not segmented */
VGMSTREAM * init_vgmstream_opus_nlsd(STREAMFILE *streamFile) {
    off_t offset = 0;
    int num_samples = 0, loop_start = 0, loop_end = 0, loop_flag;

    /* checks */
    if (!check_extensions(streamFile, "nlsd"))
        goto fail;
    /* File type check - DSP is 0x8, Opus is 0x9 */
    if (read_32bitBE(0x00, streamFile) != 0x09000000)
        goto fail;

	offset = 0x1C;
    num_samples = read_32bitLE(0x0C, streamFile);

    /* Check if there's a loop_end "adjuster" value to determine loop_flag
       Setting loop_flag to loop_start would work too but it may cause conflicts
       With files that may have a 0 sample loop_start. */
    // Not sure why this is a thing but let's roll with it.
    loop_flag = read_32bitLE(0x18, streamFile);
    if (loop_flag) {
        loop_start = read_32bitLE(0x10, streamFile);
        /* They were being really creative here */
        loop_end = (num_samples - read_32bitLE(0x18, streamFile));
    }

    return init_vgmstream_opus(streamFile, meta_OPUS, offset, num_samples, loop_start, loop_end);
fail:
    return NULL;
}
