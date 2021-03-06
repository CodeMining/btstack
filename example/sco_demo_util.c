/*
 * Copyright (C) 2016 BlueKitchen GmbH
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holders nor the names of
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 * 4. Any redistribution, use, or modification is done solely for
 *    personal benefit and not for any commercial purpose or for
 *    monetary gain.
 *
 * THIS SOFTWARE IS PROVIDED BY BLUEKITCHEN GMBH AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MATTHIAS
 * RINGWALD OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Please inquire about commercial licensing options at 
 * contact@bluekitchen-gmbh.com
 *
 */
 
/*
 * sco_demo_util.c - send/receive test data via SCO, used by hfp_*_demo and hsp_*_demo
 */


#include <stdio.h>

#include "sco_demo_util.h"
#include "btstack_debug.h"
#include "classic/btstack_sbc.h"
#include "classic/btstack_cvsd_plc.h"
#include "classic/hfp_msbc.h"
#include "classic/hfp.h"

#ifdef HAVE_POSIX_FILE_IO
#include "wav_util.h"
#endif

// configure test mode
#define SCO_DEMO_MODE_SINE		0
#define SCO_DEMO_MODE_ASCII		1
#define SCO_DEMO_MODE_COUNTER	2
#define SCO_DEMO_MODE_55        3
#define SCO_DEMO_MODE_00        4


// SCO demo configuration
#define SCO_DEMO_MODE SCO_DEMO_MODE_SINE
#define SCO_REPORT_PERIOD 100

#ifdef HAVE_POSIX_FILE_IO
#define SCO_WAV_FILENAME      "sco_input.wav"
#define SCO_MSBC_OUT_FILENAME "sco_output.msbc"
#define SCO_MSBC_IN_FILENAME  "sco_input.msbc"

#define SCO_WAV_DURATION_IN_SECONDS 15
#endif


#if defined(HAVE_PORTAUDIO) && (SCO_DEMO_MODE == SCO_DEMO_MODE_SINE)
#define USE_PORTAUDIO
#endif


#ifdef USE_PORTAUDIO
#include <portaudio.h>
#include "btstack_ring_buffer.h"

// portaudio config
#define NUM_CHANNELS            1

#define CVSD_SAMPLE_RATE        8000
#define CVSD_FRAMES_PER_BUFFER  24
#define CVSD_PA_SAMPLE_TYPE     paInt8
#define CVSD_BYTES_PER_FRAME    (1*NUM_CHANNELS)
#define CVSD_PREBUFFER_MS       5
#define CVSD_PREBUFFER_BYTES    (CVSD_PREBUFFER_MS * CVSD_SAMPLE_RATE/1000 * CVSD_BYTES_PER_FRAME)

#define MSBC_SAMPLE_RATE        16000
#define MSBC_FRAMES_PER_BUFFER  120
#define MSBC_PA_SAMPLE_TYPE     paInt16
#define MSBC_BYTES_PER_FRAME    (2*NUM_CHANNELS)
#define MSBC_PREBUFFER_MS       50
#define MSBC_PREBUFFER_BYTES    (MSBC_PREBUFFER_MS * MSBC_SAMPLE_RATE/1000 * MSBC_BYTES_PER_FRAME)

// portaudio globals
static  PaStream * stream;
static uint8_t pa_stream_started = 0;

static uint8_t ring_buffer_storage[2*MSBC_PREBUFFER_BYTES];
static btstack_ring_buffer_t ring_buffer;
#endif

static int dump_data = 1;
static int count_sent = 0;
static int count_received = 0;
static uint8_t negotiated_codec = 0; 
#if SCO_DEMO_MODE != SCO_DEMO_MODE_55
static int phase = 0;
#endif

FILE * msbc_file_in;
FILE * msbc_file_out;

#if SCO_DEMO_MODE == SCO_DEMO_MODE_SINE

// input signal: pre-computed sine wave, at 8000 kz
static const uint8_t sine_uint8[] = {
      0,  15,  31,  46,  61,  74,  86,  97, 107, 114,
    120, 124, 126, 126, 124, 120, 114, 107,  97,  86,
     74,  61,  46,  31,  15,   0, 241, 225, 210, 195,
    182, 170, 159, 149, 142, 136, 132, 130, 130, 132,
    136, 142, 149, 159, 170, 182, 195, 210, 225, 241,
};


// input signal: pre-computed sine wave, 160 Hz at 16000 kHz
static const int16_t sine_int16[] = {
     0,    2057,    4107,    6140,    8149,   10126,   12062,   13952,   15786,   17557,
 19260,   20886,   22431,   23886,   25247,   26509,   27666,   28714,   29648,   30466,
 31163,   31738,   32187,   32509,   32702,   32767,   32702,   32509,   32187,   31738,
 31163,   30466,   29648,   28714,   27666,   26509,   25247,   23886,   22431,   20886,
 19260,   17557,   15786,   13952,   12062,   10126,    8149,    6140,    4107,    2057,
     0,   -2057,   -4107,   -6140,   -8149,  -10126,  -12062,  -13952,  -15786,  -17557,
-19260,  -20886,  -22431,  -23886,  -25247,  -26509,  -27666,  -28714,  -29648,  -30466,
-31163,  -31738,  -32187,  -32509,  -32702,  -32767,  -32702,  -32509,  -32187,  -31738,
-31163,  -30466,  -29648,  -28714,  -27666,  -26509,  -25247,  -23886,  -22431,  -20886,
-19260,  -17557,  -15786,  -13952,  -12062,  -10126,   -8149,   -6140,   -4107,   -2057,
};

static void sco_demo_sine_wave_int8(int num_samples, int8_t * data){
    int i;
    for (i=0; i<num_samples; i++){
        data[i] = (int8_t)sine_uint8[phase];
        phase++;
        if (phase >= sizeof(sine_uint8)) phase = 0;
    }  
}

static void sco_demo_sine_wave_int16(int num_samples, int16_t * data){
    int i;
    for (i=0; i < num_samples; i++){
        data[i] = sine_int16[phase++];
        if (phase >= (sizeof(sine_int16) / sizeof(int16_t))){
            phase = 0;
        }
    }
}
static int num_audio_frames = 0;

static void sco_demo_fill_audio_frame(void){
    if (!hfp_msbc_can_encode_audio_frame_now()) return;
    int num_samples = hfp_msbc_num_audio_samples_per_frame();
    int16_t sample_buffer[num_samples];
    sco_demo_sine_wave_int16(num_samples, sample_buffer);
    hfp_msbc_encode_audio_frame(sample_buffer);
    num_audio_frames++;
}
#ifdef SCO_WAV_FILENAME
static btstack_sbc_decoder_state_t decoder_state;
static btstack_cvsd_plc_state_t cvsd_plc_state;
static int num_samples_to_write;

#ifdef USE_PORTAUDIO
static int patestCallback( const void *inputBuffer, void *outputBuffer,
                           unsigned long framesPerBuffer,
                           const PaStreamCallbackTimeInfo* timeInfo,
                           PaStreamCallbackFlags statusFlags,
                           void *userData ) {
    (void) timeInfo; /* Prevent unused variable warnings. */
    (void) statusFlags;
    (void) inputBuffer;
    
    uint32_t bytes_read = 0;
    int bytes_per_buffer = framesPerBuffer;
    if (negotiated_codec == HFP_CODEC_MSBC){
        bytes_per_buffer *= MSBC_BYTES_PER_FRAME;
    } else {
        bytes_per_buffer *= CVSD_BYTES_PER_FRAME;
    }

    if (btstack_ring_buffer_bytes_available(&ring_buffer) >= bytes_per_buffer){
        btstack_ring_buffer_read(&ring_buffer, outputBuffer, bytes_per_buffer, &bytes_read);
    } else {
        printf("NOT ENOUGH DATA!\n");
        memset(outputBuffer, 0, bytes_per_buffer);
    }
    // printf("bytes avail after read: %d\n", btstack_ring_buffer_bytes_available(&ring_buffer));
    return 0;
}
#endif

static void handle_pcm_data(int16_t * data, int num_samples, int num_channels, int sample_rate, void * context){
    UNUSED(context);
    UNUSED(sample_rate);

    // printf("handle_pcm_data num samples %u, sample rate %d\n", num_samples, num_channels);
#ifdef USE_PORTAUDIO
    if (!pa_stream_started && btstack_ring_buffer_bytes_available(&ring_buffer) >= MSBC_PREBUFFER_BYTES){
        /* -- start stream -- */
        PaError err = Pa_StartStream(stream);
        if (err != paNoError){
            printf("Error starting the stream: \"%s\"\n",  Pa_GetErrorText(err));
            return;
        }
        pa_stream_started = 1; 
    }
    btstack_ring_buffer_write(&ring_buffer, (uint8_t *)data, num_samples*num_channels*2);
    // printf("bytes avail after write: %d\n", btstack_ring_buffer_bytes_available(&ring_buffer));
#else
    UNUSED(num_channels);
#endif 

    if (!num_samples_to_write) return;
    
    num_samples = btstack_min(num_samples, num_samples_to_write);
    num_samples_to_write -= num_samples;

    wav_writer_write_int16(num_samples, data);

    if (num_samples_to_write == 0){
        sco_demo_close();
    }
}

static void sco_demo_init_mSBC(void){
    int sample_rate = 16000;
    wav_writer_open(SCO_WAV_FILENAME, 1, sample_rate);
    btstack_sbc_decoder_init(&decoder_state, SBC_MODE_mSBC, &handle_pcm_data, NULL);    

    num_samples_to_write = sample_rate * SCO_WAV_DURATION_IN_SECONDS;
    
    hfp_msbc_init();
    sco_demo_fill_audio_frame();

#ifdef SCO_MSBC_IN_FILENAME
    msbc_file_in = fopen(SCO_MSBC_IN_FILENAME, "wb");
    printf("SCO Demo: creating mSBC in file %s, %p\n", SCO_MSBC_IN_FILENAME, msbc_file_in);
#endif   
#ifdef SCO_MSBC_OUT_FILENAME
    msbc_file_out = fopen(SCO_MSBC_OUT_FILENAME, "wb");
    printf("SCO Demo: creating mSBC out file %s, %p\n", SCO_MSBC_OUT_FILENAME, msbc_file_out);
#endif   

#ifdef USE_PORTAUDIO
    PaError err;
    PaStreamParameters outputParameters;

    /* -- initialize PortAudio -- */
    err = Pa_Initialize();
    if( err != paNoError ) return;
    /* -- setup input and output -- */
    outputParameters.device = Pa_GetDefaultOutputDevice(); /* default output device */
    outputParameters.channelCount = NUM_CHANNELS;
    outputParameters.sampleFormat = MSBC_PA_SAMPLE_TYPE;
    outputParameters.suggestedLatency = Pa_GetDeviceInfo( outputParameters.device )->defaultHighOutputLatency;
    outputParameters.hostApiSpecificStreamInfo = NULL;
    /* -- setup stream -- */
    err = Pa_OpenStream(
           &stream,
           NULL, // &inputParameters,
           &outputParameters,
           MSBC_SAMPLE_RATE,
           MSBC_FRAMES_PER_BUFFER,
           paClipOff, /* we won't output out of range samples so don't bother clipping them */
           patestCallback,      /* no callback, use blocking API */
           NULL );    /* no callback, so no callback userData */
    if (err != paNoError){
        printf("Error initializing portaudio: \"%s\"\n",  Pa_GetErrorText(err));
        return;
    }
    memset(ring_buffer_storage, 0, sizeof(ring_buffer_storage));
    btstack_ring_buffer_init(&ring_buffer, ring_buffer_storage, sizeof(ring_buffer_storage));
    pa_stream_started = 0;
#endif  
}

static void sco_demo_receive_mSBC(uint8_t * packet, uint16_t size){
    if (num_samples_to_write){
        if (msbc_file_in){
            // log incoming mSBC data for testing
            fwrite(packet+3, size-3, 1, msbc_file_in);
        }
    }
    btstack_sbc_decoder_process_data(&decoder_state, (packet[1] >> 4) & 3, packet+3, size-3);  
}

static void sco_demo_init_CVSD(void){
    int sample_rate = 8000;
    wav_writer_open(SCO_WAV_FILENAME, 1, sample_rate);
    btstack_cvsd_plc_init(&cvsd_plc_state);
    num_samples_to_write = sample_rate * SCO_WAV_DURATION_IN_SECONDS;

#ifdef USE_PORTAUDIO
    PaError err;
    PaStreamParameters outputParameters;

    /* -- initialize PortAudio -- */
    err = Pa_Initialize();
    if( err != paNoError ) return;
    /* -- setup input and output -- */
    outputParameters.device = Pa_GetDefaultOutputDevice(); /* default output device */
    outputParameters.channelCount = NUM_CHANNELS;
    outputParameters.sampleFormat = CVSD_PA_SAMPLE_TYPE;
    outputParameters.suggestedLatency = Pa_GetDeviceInfo( outputParameters.device )->defaultHighOutputLatency;
    outputParameters.hostApiSpecificStreamInfo = NULL;
    /* -- setup stream -- */
    err = Pa_OpenStream(
           &stream,
           NULL, // &inputParameters,
           &outputParameters,
           CVSD_SAMPLE_RATE,
           CVSD_FRAMES_PER_BUFFER,
           paClipOff, /* we won't output out of range samples so don't bother clipping them */
           patestCallback,      /* no callback, use blocking API */
           NULL );    /* no callback, so no callback userData */
    if (err != paNoError){
        printf("Error initializing portaudio: \"%s\"\n",  Pa_GetErrorText(err));
        return;
    }
    memset(ring_buffer_storage, 0, sizeof(ring_buffer_storage));
    btstack_ring_buffer_init(&ring_buffer, ring_buffer_storage, sizeof(ring_buffer_storage));
    pa_stream_started = 0;
#endif  
}

static void sco_demo_receive_CVSD(uint8_t * packet, uint16_t size){
    if (!num_samples_to_write) return;

    const int num_samples = size - 3;
    const int samples_to_write = btstack_min(num_samples, num_samples_to_write);
    int8_t audio_frame_out[24];
    

    // memcpy(audio_frame_out, (int8_t*)(packet+3), 24);
    btstack_cvsd_plc_process_data(&cvsd_plc_state, (int8_t *)(packet+3), num_samples, audio_frame_out);
    // int8_t * audio_frame_out = (int8_t*)&packet[3];

    wav_writer_write_int8(samples_to_write, audio_frame_out);
    num_samples_to_write -= samples_to_write;
    if (num_samples_to_write == 0){
        sco_demo_close();
    }
#ifdef USE_PORTAUDIO
    if (!pa_stream_started && btstack_ring_buffer_bytes_available(&ring_buffer) >= CVSD_PREBUFFER_BYTES){
        /* -- start stream -- */
        PaError err = Pa_StartStream(stream);
        if (err != paNoError){
            printf("Error starting the stream: \"%s\"\n",  Pa_GetErrorText(err));
            return;
        }
        pa_stream_started = 1; 
    }
    btstack_ring_buffer_write(&ring_buffer, (uint8_t *)audio_frame_out, samples_to_write);
#endif
}

#endif
#endif

void sco_demo_close(void){    
#if SCO_DEMO_MODE == SCO_DEMO_MODE_SINE
#if defined(SCO_WAV_FILENAME) || defined(SCO_SBC_FILENAME)
    wav_writer_close();
    printf("SCO demo statistics: ");
    if (negotiated_codec == HFP_CODEC_MSBC){
        printf("Used mSBC with PLC, number of processed frames: \n - %d good frames, \n - %d zero frames, \n - %d bad frames.", decoder_state.good_frames_nr, decoder_state.zero_frames_nr, decoder_state.bad_frames_nr);
    } else {
        printf("Used CVSD with PLC, number of proccesed frames: \n - %d good frames, \n - %d bad frames.", cvsd_plc_state.good_frames_nr, cvsd_plc_state.bad_frames_nr);
    }
#endif
#endif

#ifdef HAVE_PORTAUDIO
    if (pa_stream_started){
        PaError err = Pa_StopStream(stream);
        if (err != paNoError){
            printf("Error stopping the stream: \"%s\"\n",  Pa_GetErrorText(err));
            return;
        } 
        pa_stream_started = 0;
        err = Pa_CloseStream(stream);
        if (err != paNoError){
            printf("Error closing the stream: \"%s\"\n",  Pa_GetErrorText(err));
            return;
        } 

        err = Pa_Terminate();
        if (err != paNoError){
            printf("Error terminating portaudio: \"%s\"\n",  Pa_GetErrorText(err));
            return;
        }
    } 
#endif

#if SCO_DEMO_MODE == SCO_DEMO_MODE_SINE
#ifdef SCO_WAV_FILENAME
    
#if 0
    printf("SCO Demo: closing wav file\n");
    if (negotiated_codec == HFP_CODEC_MSBC){
        wav_writer_state_t * writer_state = &wav_writer_state;
        if (!writer_state->wav_file) return;
        rewind(writer_state->wav_file);
        write_wav_header(writer_state->wav_file, writer_state->total_num_samples, btstack_sbc_decoder_num_channels(&decoder_state), btstack_sbc_decoder_sample_rate(&decoder_state),2);
        fclose(writer_state->wav_file);
        writer_state->wav_file = NULL;
    }
#endif
#endif
#endif
}

void sco_demo_set_codec(uint8_t codec){
    if (negotiated_codec == codec) return;
    negotiated_codec = codec;
#if SCO_DEMO_MODE == SCO_DEMO_MODE_SINE
#if defined(SCO_WAV_FILENAME) || defined(SCO_SBC_FILENAME)
    if (negotiated_codec == HFP_CODEC_MSBC){
        sco_demo_init_mSBC();
    } else {
        sco_demo_init_CVSD();
    }
#endif
#endif
}

void sco_demo_init(void){

	// status
#if SCO_DEMO_MODE == SCO_DEMO_MODE_SINE
#ifdef HAVE_PORTAUDIO
	printf("SCO Demo: Sending sine wave, audio output via portaudio.\n");
#else
	printf("SCO Demo: Sending sine wave, hexdump received data.\n");
#endif
#endif
#if SCO_DEMO_MODE == SCO_DEMO_MODE_ASCII
	printf("SCO Demo: Sending ASCII blocks, print received data.\n");
#endif
#if SCO_DEMO_MODE == SCO_DEMO_MODE_COUNTER
	printf("SCO Demo: Sending counter value, hexdump received data.\n");
#endif

#if SCO_DEMO_MODE != SCO_DEMO_MODE_SINE
    hci_set_sco_voice_setting(0x03);    // linear, unsigned, 8-bit, transparent
#endif

#if SCO_DEMO_MODE == SCO_DEMO_MODE_ASCII
    phase = 'a';
#endif
}

void sco_report(void);
void sco_report(void){
    printf("SCO: sent %u, received %u\n", count_sent, count_received);
}

void sco_demo_send(hci_con_handle_t sco_handle){

    if (!sco_handle) return;
    
    const int sco_packet_length = 24 + 3; // hci_get_sco_packet_length();
    const int sco_payload_length = sco_packet_length - 3;

    hci_reserve_packet_buffer();
    uint8_t * sco_packet = hci_get_outgoing_packet_buffer();
    // set handle + flags
    little_endian_store_16(sco_packet, 0, sco_handle);
    // set len
    sco_packet[2] = sco_payload_length;
    const int audio_samples_per_packet = sco_payload_length;    // for 8-bit data. for 16-bit data it's /2

#if SCO_DEMO_MODE == SCO_DEMO_MODE_SINE
    if (negotiated_codec == HFP_CODEC_MSBC){

        if (hfp_msbc_num_bytes_in_stream() < sco_payload_length){
            log_error("mSBC stream is empty.");
        }
        hfp_msbc_read_from_stream(sco_packet + 3, sco_payload_length);
        if (msbc_file_out){
            // log outgoing mSBC data for testing
            fwrite(sco_packet + 3, sco_payload_length, 1, msbc_file_out);
        }

        sco_demo_fill_audio_frame();
    } else {
        sco_demo_sine_wave_int8(audio_samples_per_packet, (int8_t *) (sco_packet+3));
    }
#endif
#if SCO_DEMO_MODE == SCO_DEMO_MODE_ASCII
    memset(&sco_packet[3], phase++, audio_samples_per_packet);
    if (phase > 'z') phase = 'a';
#endif
#if SCO_DEMO_MODE == SCO_DEMO_MODE_COUNTER
    int j;
    for (j=0;j<audio_samples_per_packet;j++){
        sco_packet[3+j] = phase++;
    }
#endif
#if SCO_DEMO_MODE == SCO_DEMO_MODE_55
    int j;
    for (j=0;j<audio_samples_per_packet;j++){
        // sco_packet[3+j] = j & 1 ? 0x35 : 0x53;
        sco_packet[3+j] = 0x55;
    }
#endif
#if SCO_DEMO_MODE == SCO_DEMO_MODE_00
    int j;
    for (j=0;j<audio_samples_per_packet;j++){
        sco_packet[3+j] = 0x00;
    }
    // additional hack
    // big_endian_store_16(sco_packet, 5, phase++);
    (void) phase;
#endif

    hci_send_sco_packet_buffer(sco_packet_length);

    // request another send event
    hci_request_sco_can_send_now_event();

    count_sent++;
#if SCO_DEMO_MODE != SCO_DEMO_MODE_55
    if ((count_sent % SCO_REPORT_PERIOD) == 0) sco_report();
#endif
}

/**
 * @brief Process received data
 */
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_GREEN   "\x1b[32m"
#define ANSI_COLOR_YELLOW  "\x1b[33m"
#define ANSI_COLOR_BLUE    "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN    "\x1b[36m"
#define ANSI_COLOR_RESET   "\x1b[0m"

void sco_demo_receive(uint8_t * packet, uint16_t size){

    dump_data = 1;

    count_received++;
    static uint32_t packets = 0;
    static uint32_t crc_errors = 0;
    static uint32_t data_received = 0;
    static uint32_t byte_errors = 0;

    data_received += size - 3;
    packets++;
    if (data_received > 100000){
        printf("Summary: data %07u, packets %04u, packet with crc errors %0u, byte errors %04u\n", data_received, packets, crc_errors, byte_errors);
        crc_errors = 0;
        byte_errors = 0;
        data_received = 0;
        packets = 0;
    }

#if SCO_DEMO_MODE == SCO_DEMO_MODE_SINE
#ifdef SCO_WAV_FILENAME
    if (negotiated_codec == HFP_CODEC_MSBC){
        sco_demo_receive_mSBC(packet, size);
    } else {
        sco_demo_receive_CVSD(packet, size);
    }
    dump_data = 0;
#endif
#endif

    if (packet[1] & 0x30){
        crc_errors++;
        // printf("SCO CRC Error: %x - data: ", (packet[1] & 0x30) >> 4);
        // printf_hexdump(&packet[3], size-3);
        return;
    }
    if (dump_data){
#if SCO_DEMO_MODE == SCO_DEMO_MODE_ASCII
        printf("data: ");
        int i;
        for (i=3;i<size;i++){
            printf("%c", packet[i]);
        }
        printf("\n");
        dump_data = 0;
#endif
#if SCO_DEMO_MODE == SCO_DEMO_MODE_COUNTER
        // colored hexdump with expected
        static uint8_t expected_byte = 0;
        int i;
        printf("data: ");
        for (i=3;i<size;i++){
            if (packet[i] != expected_byte){
                printf(ANSI_COLOR_RED "%02x " ANSI_COLOR_RESET, packet[i]);
            } else {
                printf("%02x ", packet[i]);
            }
            expected_byte = packet[i]+1;
        }
        printf("\n");
#endif
#if SCO_DEMO_MODE == SCO_DEMO_MODE_55 || SCO_DEMO_MODE_00
        int i;
        int contains_error = 0;
        for (i=3;i<size;i++){
            if (packet[i] != 0x00 && packet[i] != 0x35 && packet[i] != 0x53 && packet[i] != 0x55){
                contains_error = 1;
                byte_errors++;
            }
        }
        if (contains_error){
            printf("data: ");
            for (i=0;i<3;i++){
                printf("%02x ", packet[i]);
            }
            for (i=3;i<size;i++){
                if (packet[i] != 0x00 && packet[i] != 0x35 && packet[i] != 0x53 && packet[i] != 0x55){
                    printf(ANSI_COLOR_RED "%02x " ANSI_COLOR_RESET, packet[i]);
                } else {
                    printf("%02x ", packet[i]);
                }
            }
            printf("\n");
        }
#endif
    }
}
