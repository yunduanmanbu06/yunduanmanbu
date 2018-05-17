//#include "baidu_http_client_log.h"
#include <stdio.h>
#include <stdlib.h>
#include <speex/speex.h>
#include <speex/speex_preprocess.h>
#include "baidu_speex.h"
#include "baidu_time_calculate.h"
#include "lightduer_log.h"
#include "mbed.h"
#include "heap_monitor.h"

//to statistic the speex encoder's encoding time
//#define BAIDU_SPEEX_SPEED_STATISTIC

#if SPEEX_SAMPLE_RATE == 8000
/*The frame size in hardcoded for this sample code but it doesn't have to be*/
#define BDSPX_FRAME_SIZE 160

/*The buffer size to copy frame*/
#define BDSPX_FRAME_COPY_SIZE 200

#elif SPEEX_SAMPLE_RATE == 16000
/*The frame size in hardcoded for this sample code but it doesn't have to be*/
#define BDSPX_FRAME_SIZE 320

/*The buffer size to copy frame*/
#define BDSPX_FRAME_COPY_SIZE 400
#endif

//quality of speech to be encoded by speex, normal is 8
#define BDSPX_ENCODE_QUALITY  5

/*Holds the state of the encoder*/
void *p_g_bdspx_state = NULL;

/*Holds bits so they can be read and written to by the Speex routines*/
SpeexBits g_bdspx_bits;

/*Holds the state of the encoder preprocessor*/
SpeexPreprocessState *p_g_bdspx_pre_state = NULL;

#if 1
#if SPEEX_SAMPLE_RATE == 8000
#define BDSPX_FRAME_HEADER_MAGIC_0     'S'
#elif SPEEX_SAMPLE_RATE == 16000
#define BDSPX_FRAME_HEADER_MAGIC_0     'T'
#endif
#define BDSPX_FRAME_HEADER_MAGIC_1     'P'
#define BDSPX_FRAME_HEADER_MAGIC_2     'X'
#else
#define BDSPX_FRAME_HEADER_MAGIC_0     '\0'
#define BDSPX_FRAME_HEADER_MAGIC_1     '\0'
#define BDSPX_FRAME_HEADER_MAGIC_2     '\0'

#endif

#ifdef BAIDU_SPEEX_SPEED_STATISTIC

mbed::Timer g_spx_timer;
int g_spx_frames = 0;
int g_spx_each_frame_start = 0;
int g_spx_encode_during = 0;
#endif

struct speex_encoder_buf {
    char frame_copy_bits[BDSPX_FRAME_COPY_SIZE];
    char speex_frame_out[BDSPX_FRAME_COPY_SIZE + sizeof(int)];
    short sh_frame_in[BDSPX_FRAME_SIZE];
    float flt_encode_input[BDSPX_FRAME_SIZE];
};

/**
FUNC:
int bdspx_speex_init(int n_sample_rate)

DESC:
init speex

PARAM:
@param[in] n_sample_rate: audio sample rate
@param[out] buf: the buffer created for speex encoder

*/
int bdspx_speex_init(int n_sample_rate, void **buf)
{
    int ret = 0, tmp = 0;
#ifdef SPEEX_PREPROCESS_SUPPORT
    float f = 0.0f;
#endif

    DUER_LOGV("enter\n");

#ifdef BAIDU_SPEEX_SPEED_STATISTIC
    g_spx_timer.start();
#endif

    if (!buf) {
        DUER_LOGE("Speex init failed!\n");
        ret = -1;
        goto ERR;
    }

    *buf = MALLOC(sizeof(speex_encoder_buf), SPEEX_LIB);
    if (*buf == NULL) {
        DUER_LOGE("Speex init failed: shortage of memory!\n");
        ret = -1;
        goto ERR;
    }

#ifdef SPEEX_PREPROCESS_SUPPORT
    p_g_bdspx_pre_state = speex_preprocess_state_init(BDSPX_FRAME_SIZE, n_sample_rate);
    if (!p_g_bdspx_pre_state){
        DUER_LOGE("Speex preprocess state init failed!\n");
        ret = -1;
        goto ERR;
    }

    i=1;
    speex_preprocess_ctl(p_g_bdspx_pre_state, SPEEX_PREPROCESS_SET_DENOISE, &i);
    i=1;
    speex_preprocess_ctl(p_g_bdspx_pre_state, SPEEX_PREPROCESS_SET_VAD, &i);
    i=SPEEX_PREPROCESS_SET_AGC_MAX_GAIN;
    speex_preprocess_ctl(p_g_bdspx_pre_state, SPEEX_PREPROCESS_SET_AGC, &i);
    f=8000;
    speex_preprocess_ctl(p_g_bdspx_pre_state, SPEEX_PREPROCESS_SET_AGC_LEVEL, &f);
    i=0;
    speex_preprocess_ctl(p_g_bdspx_pre_state, SPEEX_PREPROCESS_SET_DEREVERB, &i);
    f=.0;
    speex_preprocess_ctl(p_g_bdspx_pre_state, SPEEX_PREPROCESS_SET_DEREVERB_DECAY, &f);
    f=.0;
    speex_preprocess_ctl(p_g_bdspx_pre_state, SPEEX_PREPROCESS_SET_DEREVERB_LEVEL, &f);
#endif

    /*Create a new encoder state in narrowband mode*/
#if SPEEX_SAMPLE_RATE == 8000
    p_g_bdspx_state = speex_encoder_init(&speex_nb_mode);
#elif SPEEX_SAMPLE_RATE == 16000
    p_g_bdspx_state = speex_encoder_init(&speex_wb_mode);
#endif
    if (!p_g_bdspx_state){
        DUER_LOGE("Speex encoder state init failed!\n");
        ret = -1;
        goto ERR;
    }

    /*Set the quality to 8 (15 kbps)*/
    tmp=BDSPX_ENCODE_QUALITY;  //6 for 11kbps
    speex_encoder_ctl(p_g_bdspx_state, SPEEX_SET_QUALITY, &tmp);

    tmp=0;
    speex_encoder_ctl(p_g_bdspx_state,SPEEX_GET_FRAME_SIZE,&tmp);
    DUER_LOGD("get frame size:%d\n", tmp);

    //tmp=1;
    //speex_encoder_ctl(p_g_bdspx_state, SPEEX_SET_COMPLEXITY, &tmp);

    tmp=0;
    speex_encoder_ctl(p_g_bdspx_state, SPEEX_GET_BITRATE, &tmp);


    /*Initialization of the structure that holds the bits*/
    speex_bits_init(&g_bdspx_bits);

    DUER_LOGV("leave\n");

ERR:
	if ((ret != 0) && (buf != NULL)) {
        if (*buf != NULL) {
            FREE(*buf);
            *buf = NULL;
        }
	}

    return ret;

}

/**
FUNC:
int bdspx_speex_gen_frame(char *p_frame_buff, int n_frame_len, char *p_spx_buff, int n_spx_len)

DESC:
to generate a speex frame with speex data

PARAM:
@param[out] p_frame_buff: generated speex frame("size+data")
@param[in] n_frame_len: buffer lenght of 'p_frame_buff'
@param[in] p_spx_buff: buffer stored speex data
@param[in] n_spx_len: speex data length in 'p_spx_buff'

RETURN:
a speex frame's length, or -1 if failed
*/

int bdspx_speex_gen_frame(char *p_frame_buff, int n_frame_len, char *p_spx_buff, int n_spx_len)
{
    int ret  = 0;

    if (!p_frame_buff || !p_spx_buff || n_frame_len < n_spx_len + sizeof(int)){
        DUER_LOGE("args error!!!");
        return -1;
    }

    p_frame_buff[0] = (char)n_spx_len;
    p_frame_buff[1] = BDSPX_FRAME_HEADER_MAGIC_0;
    p_frame_buff[2] = BDSPX_FRAME_HEADER_MAGIC_1;
    p_frame_buff[3] = BDSPX_FRAME_HEADER_MAGIC_2;

    memcpy((char *)p_frame_buff + sizeof(int), p_spx_buff, n_spx_len);

    ret  = n_spx_len + sizeof(int);

    return ret;
}



/**
FUNC:
int bdspx_speex_encode(char *p_pcm_buff, int *p_len, int n_end, bdspx_output_hdlr_t output_hdlr)

DESC:
encode pcm data to speex

PARAM:
@param[in/out] p_pcm_buff: [in]pcm buffer to be speex encoded; [out]remained buffer
@param[in/out] p_len: [in]data length in 'p_pcm_buff'; [out]the remained length in 'p_pcm_buff'
@param[in] n_end: to check if itis the last data
@param[in] output_hdlr: a callback to output encoded speex data
@param[in] buf: a tmp buffer prepared for speex enncoder, used to storage some middle result

RETURN:
error to return -1, or the remained length in 'p_pcm_buff'
*/

int bdspx_speex_encode(char *p_pcm_buff, int *p_len, int n_end, bdspx_output_hdlr_t output_hdlr, void *ctx, void *buf)
{
    int ret = 0, i = 0;
    int n_bytes = 0;
#ifdef SPEEX_PREPROCESS_SUPPORT
    int vad = 0;
#endif
    int n_remain_len = 0;
    int b_flush = 0;
    int n_frame_bytes = 0;
    struct speex_encoder_buf* encoder_buf = (struct speex_encoder_buf*)buf;

    DUER_LOGV("enter\n");

    if (!p_pcm_buff || !p_len || (*p_len == 0) || !buf){
        DUER_LOGE("Args error!\n");
        ret = -1;
        goto ERR;
    }

    n_remain_len  = *p_len;

    while (1)
    {
#ifdef BAIDU_SPEEX_SPEED_STATISTIC
        g_spx_each_frame_start = g_spx_timer.read_ms();
#endif

		PERFORM_STATISTIC_BEGIN(PERFORM_STATISTIC_SPEEX_ENCODE, MAX_TIME_CALCULATE_COUNT);

        /*Read a 16 bits/sample audio frame*/
        if (n_remain_len >= BDSPX_FRAME_SIZE * sizeof(short)){
            memcpy(encoder_buf->sh_frame_in, p_pcm_buff + (*p_len - n_remain_len),
                      BDSPX_FRAME_SIZE * sizeof(short));
            n_remain_len -= BDSPX_FRAME_SIZE * sizeof(short);
            if (n_remain_len == 0 && n_end){
                b_flush = 1;
            }
        }
        else if(n_remain_len == 0){
            DUER_LOGD("Finished encoding!\n");
            *p_len = 0;
            break;
        }
        else{
            if (n_end){
                DUER_LOGD("encode the remained %d bytes\n", n_remain_len);
                memcpy(encoder_buf->sh_frame_in, p_pcm_buff + (*p_len - n_remain_len),
                          n_remain_len);
                memset(encoder_buf->sh_frame_in + n_remain_len/sizeof(short), 0,
		                  sizeof(encoder_buf->sh_frame_in) - n_remain_len);
                *p_len = 0;
                n_remain_len = 0;
                b_flush = 1;
            }
            else{
                DUER_LOGD("remained %d bytes not encoded by speex, and finished encoding\n", n_remain_len);
                memmove(p_pcm_buff, p_pcm_buff + (*p_len - n_remain_len), n_remain_len);
                *p_len = n_remain_len;
                break;
            }
        }

#ifdef SPEEX_PREPROCESS_SUPPORT
        //speex_preprocess_estimate_update(p_g_bdspx_pre_state, in);
        vad = speex_preprocess_run(p_g_bdspx_pre_state, encoder_buf->sh_frame_in);
        //HCLOGI("vad=%d", vad);
#endif

        /*Copy the 16 bits values to float so Speex can work on them*/
        for (i=0; i<BDSPX_FRAME_SIZE; i++)
            encoder_buf->flt_encode_input[i]=encoder_buf->sh_frame_in[i];

        /*Flush all the bits in the struct so we can encode a new frame*/
        speex_bits_reset(&g_bdspx_bits);

        /*Encode the frame*/
        speex_encode(p_g_bdspx_state, encoder_buf->flt_encode_input, &g_bdspx_bits);

        /*Copy the bits to an array of char that can be written*/
        n_bytes = speex_bits_write(&g_bdspx_bits, encoder_buf->frame_copy_bits, BDSPX_FRAME_COPY_SIZE);
        DUER_LOGD("speex frame size:%d\n", n_bytes);

        //to output the encoded speex data
        if (output_hdlr){
            n_frame_bytes = bdspx_speex_gen_frame(encoder_buf->speex_frame_out,
	                                              sizeof(encoder_buf->speex_frame_out),
	                                              encoder_buf->frame_copy_bits, n_bytes);
            if (n_frame_bytes != -1){
                PERFORM_STATISTIC_END(PERFORM_STATISTIC_SPEEX_ENCODE,
		                              n_frame_bytes,
		                              UNCONTINUOUS_TIME_MEASURE);
				//output_hdlr(ctx, encoder_buf->frame_copy_bits, n_bytes, b_flush);
                output_hdlr(ctx, encoder_buf->speex_frame_out, n_frame_bytes, b_flush);
                b_flush = 0;
            }
            else{
                DUER_LOGE("Generating speex frame failed!!!\n");
            }
        }
#ifdef BAIDU_SPEEX_SPEED_STATISTIC
        g_spx_encode_during += g_spx_timer.read_ms() - g_spx_each_frame_start;
        g_spx_frames++;
        DUER_LOGD("*************speex encoding %d frames costs %d ms with speed: %f ms/frame*******************\n",  \
                  g_spx_frames, g_spx_encode_during, g_spx_frames ? 1.0 * g_spx_encode_during / g_spx_frames : 0);
#endif
    }//end of while (1)

ERR:

    DUER_LOGV("leave\n");

    ret = n_remain_len;

    return ret;
}


#ifdef BAIDU_SPEEX_ENCODE_STACK_MONITOR
extern "C" char *g_p_spx_enc_stk_top;
extern "C" char *g_p_spx_enc_stk_base;
#endif

/**
FUNC:
void bdspx_speex_destroy()

DESC:
destroy speex

PARAM:
@param[in] buf: a buffer needed by speex encoder.
                It is created in bdspx_speex_init, should be freed here.

*/
void bdspx_speex_destroy(void *buf)
{
#ifdef SPEEX_PREPROCESS_SUPPORT
    /*Destroy the preprocess state*/
    speex_preprocess_state_destroy(p_g_bdspx_pre_state);
#endif

    if (buf) {
        FREE(buf);
    }

    /*Destroy the encoder state*/
    speex_encoder_destroy(p_g_bdspx_state);

    /*Destroy the bit-packing struct*/
    speex_bits_destroy(&g_bdspx_bits);

#ifdef BAIDU_SPEEX_ENCODE_STACK_MONITOR
    DUER_LOGD("***********g_p_spx_enc_stk_top=0x%x, g_p_spx_enc_stk_base=0x%x, max size=%d******************\n", \
        g_p_spx_enc_stk_top, g_p_spx_enc_stk_base, g_p_spx_enc_stk_top - g_p_spx_enc_stk_base);
#endif

}

#if 1

/*********************the following for testing******************************************/
#define BDSPX_SPEEX_SAMPLE_RATE   8000

#define BDSPX_MAX_FRAMES_PER_PACKAGE    10

#define BDSPX_FRAME_LEN_MAX       50//just estimate

#define BDSPX_PACKAGE_LEN_MAX    (BDSPX_MAX_FRAMES_PER_PACKAGE * (BDSPX_FRAME_LEN_MAX + 4) + 4)


typedef struct bdspeex_frame_t{
    int n_bytes;
    char data[];
}bdspeex_frame_t;


typedef struct bdspeex_package_t{
    int n_frames;
    bdspeex_frame_t p_frame[];
}bdspeex_package_t;

FILE *p_speex_file = NULL;

//to buffer encoded speex data
char g_bdspx_package[BDSPX_PACKAGE_LEN_MAX] = {0};


/**
FUNC:
void bdspx_output_hdlr_inst(char *p_spx_buf, int n_bytes, int b_flush)

DESC:
output speex data

PARAM:
@param[in] p_spx_buf: speex buffer to output
@param[in] n_bytes: speex buffer length
@param[in] b_flush: to flush all speex data to output

RETURN:
none
*/
void bdspx_output_hdlr_inst(void *ctx, char *p_spx_buf, int n_bytes, int b_flush)
{
#if 1
    int i = 0;
    int n_total_size = 0;
    bdspeex_package_t *p_spx_pkg = (bdspeex_package_t *)g_bdspx_package;
    bdspeex_frame_t *p_spx_frame = p_spx_pkg->p_frame;
    if (n_bytes > BDSPX_FRAME_LEN_MAX){
        DUER_LOGI("Warning: speex buffer frame size (%d) is bigger than the default max(%d)\n", n_bytes, BDSPX_FRAME_LEN_MAX);
    }
    n_total_size += sizeof(p_spx_pkg->n_frames);
    for (i = 0; i < p_spx_pkg->n_frames; i++){
        n_total_size += p_spx_frame->n_bytes + sizeof(p_spx_frame->n_bytes);
        p_spx_frame = (bdspeex_frame_t *)((char *)p_spx_frame->data+ p_spx_frame->n_bytes);
    }
    if (n_total_size + n_bytes + sizeof(p_spx_frame->n_bytes) > BDSPX_PACKAGE_LEN_MAX){
        DUER_LOGD("Triggle output  (%d) bytes  when package frame count is %d rather than default(%d)\n", n_total_size - sizeof(p_spx_pkg->n_frames),  p_spx_pkg->n_frames, BDSPX_MAX_FRAMES_PER_PACKAGE);
        //to handle package 'g_bdspx_package'
        //xxxxxxxx
        fwrite(p_spx_pkg->p_frame, 1, n_total_size - sizeof(p_spx_pkg->n_frames),  p_speex_file);

        //reset speex package
        p_spx_pkg->n_frames = 0;
        p_spx_frame = p_spx_pkg->p_frame;
    }

    p_spx_frame->n_bytes = n_bytes;
    memcpy(p_spx_frame->data, p_spx_buf, n_bytes);

    n_total_size += p_spx_frame->n_bytes + sizeof(p_spx_frame->n_bytes);

    p_spx_pkg->n_frames++;
    if (p_spx_pkg->n_frames >= BDSPX_MAX_FRAMES_PER_PACKAGE || b_flush){
        DUER_LOGD("To Triggle (%d) bytes speex data output when package frame counter is %d\n", n_total_size - sizeof(p_spx_pkg->n_frames), p_spx_pkg->n_frames);
        //to handle package 'g_bdspx_package'
        //xxxxxxxx
        if (b_flush){
            DUER_LOGD("This package is flushed!\n");
        }
        fwrite(p_spx_pkg->p_frame, 1, n_total_size - sizeof(p_spx_pkg->n_frames),  p_speex_file);
        p_spx_pkg->n_frames = 0;
    }

    return;

#else
    fwrite(&n_bytes, sizeof(int), 1, p_speex_file);
    fwrite(p_spx_buf, 1, n_bytes, p_speex_file);
#endif
}


#include "wavfmt.h"
//#define SPEEX_SAMPLE_RATE   8000


//speex decoder

#define SPEEX_DECODER_INPUT_FILE      "/sd/bd_recording.spx"
#define SPEEX_DECODER_OUTPUT_FILE   "/sd/bd_slience10s_dec.wav"

#define FRAME_SIZE 160

int speex_decode_wave()
{
    char *inFile, *outFile;
    FILE *fin, *fout;
    /*Holds the audio that will be written to file (16 bits per sample)*/
    short out[FRAME_SIZE];
    /*Speex handle samples as float, so we need an array of floats*/
    float output[FRAME_SIZE];
    char cbits[200];
    int nbBytes;
    /*Holds the state of the decoder*/
    void *state;
    /*Holds bits so they can be read and written to by the Speex routines*/
    SpeexBits bits;
    int i, tmp;
    int nFileLen = 0, nOutFileLen = 0;

    int frames = 0;

    DUER_LOGD("speex decoder testing!\n");

    /*Create a new decoder state in narrowband mode*/
    state = speex_decoder_init(&speex_nb_mode);//speex_wb_mode speex_nb_mode
    /*Set the perceptual enhancement on*/

    tmp=1;
    speex_decoder_ctl(state, SPEEX_SET_ENH, &tmp);

    inFile = SPEEX_DECODER_INPUT_FILE;
    fin = fopen(inFile, "r");
    if (!fin)
    {
        printf("file open failed: %s\n", inFile);
        return -1;
    }

    fseek(fin,0,SEEK_END);
    nFileLen = ftell(fin);
    DUER_LOGD("input file Bubbly.spx size: %d Bytes\n", nFileLen);
    fseek(fin,0,SEEK_SET);

    outFile =SPEEX_DECODER_OUTPUT_FILE;
    fout = fopen(outFile, "w");
    if (!fout)
    {
        printf("file open failed: %s\n", outFile);
        fclose(fin);
        return -1;
    }
    wavfmt_add_riff_chunk(fout, 0);
    wavfmt_add_fmt_chunk(fout, WAVE_CHANNEL_MONO, SPEEX_SAMPLE_RATE, 0, 8*2);
    wavfmt_add_data_chunk_header(fout, 0);

    /*Initialization of the structure that holds the bits*/
    speex_bits_init(&bits);

    int rs = 0;

    while (1)
    {

        /*Read the size encoded by sampleenc, this part will likely be
        different in your application*/
        rs = fread(&nbBytes, sizeof(int), 1, fin);
        printf("nbBytes: %d, rs: %d\n", nbBytes, rs);

        if (nbBytes < 0)
        {
            break;
        }

        //fprintf (stderr, "nbBytes: %d\n", nbBytes);
        if (feof(fin))
            break;

        /*Read the "packet" encoded by sampleenc*/
        fread(cbits, 1, nbBytes, fin);

        /*Copy the data into the bit-stream struct*/
        speex_bits_read_from(&bits, cbits, nbBytes);

        /*Decode the data*/
        speex_decode(state, &bits, output);


        frames++;

        /*Copy from float to short (16 bits) for output*/
        for (i=0;i<FRAME_SIZE;i++)
            out[i]=output[i];

        /*Write the decoded audio to file*/
        fwrite(out, sizeof(short), FRAME_SIZE, fout);
    }

    DUER_LOGD("\n*************xxxxxxxxxxxxxx!***************\n\n");

    /*Destroy the decoder state*/
    speex_decoder_destroy(state);

    /*Destroy the bit-stream truct*/
    speex_bits_destroy(&bits);

    nOutFileLen = ftell(fout);
    wavfmt_add_riff_chunk(fout, nOutFileLen - 8);
    wavfmt_add_data_chunk_header(fout, nOutFileLen - WAVE_HEADER_SIZE);

    fclose(fin);
    fclose(fout);

    DUER_LOGD("\n*************speex testing decoder finished!***************\n\n");

    return 0;
}


char p_pcm_buff[1024 * 4];

int _____main()
{
    FILE *fin;
    int nbBytes = 0;
    //short in[BDSPX_FRAME_SIZE];
    //float input[BDSPX_FRAME_SIZE];
    int len = 0;
    int is_end = 0;
    int to_read_bytes = 0;
    void *buf;

    p_speex_file = fopen("/sd/bd_recording.spx", "w");
    if (!p_speex_file){
        DUER_LOGE("open file failed!\n");
        return -1;
    }

    bdspx_speex_init(BDSPX_SPEEX_SAMPLE_RATE, &buf);

    fin = fopen("/sd/slience10s.wav", "r");
    if (!fin){
        DUER_LOGE("open file /sd/slience10s.wav failed!\n");
        return -1;
    }

    fseek(fin,0,SEEK_SET);

    wavfmt_remove_header(fin);

    while (1)
    {
        to_read_bytes = BDSPX_FRAME_SIZE * 2 + 20;
        /*Read a 16 bits/sample audio frame*/
        nbBytes = fread(p_pcm_buff + len, 1, to_read_bytes, fin);
        if (nbBytes == 0)
            break;
        if (feof(fin)){
            DUER_LOGD("end of file!!!\n");
        }
        if (nbBytes != to_read_bytes || feof(fin)){
            is_end = 1;
        }

        len += nbBytes;
        if (bdspx_speex_encode((char *)p_pcm_buff, &len, is_end, bdspx_output_hdlr_inst, NULL, buf) == -1){
            DUER_LOGE("speex encode error!!!\n");
            break;
        }
        DUER_LOGD("return %d bytes\n", len);
    }

    fclose(fin);

    fclose(p_speex_file);
    bdspx_speex_destroy(buf);

    speex_decode_wave();
    return 0;
}

/*********************the UP for testing******************************************/

#endif
