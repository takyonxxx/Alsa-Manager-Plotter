#ifndef __ALSADevices_H
#define __ALSADevices_H

#include "constants.h"
#include "plotter.h"
#include "kalmanfilter.h"

class ALSAPCMDevice:public QThread
{
    Q_OBJECT

protected:
    snd_pcm_t* capture_handle;
    snd_pcm_t* playback_handle;
    std::string device_name;
    unsigned int sample_rate, channels;             // Quality of the recorded audio.
    snd_pcm_uframes_t frames_per_period;            // Latency - lower numbers will decrease latency and increase CPU usage.
    snd_pcm_format_t format;                        // Bit depth - Quality.
    //enum _snd_pcm_stream type;                      // SND_PCM_STREAM_CAPTURE | SND_PCM_STREAM_PLAYBACK
    RequestType r_type;
    bool m_stop{false};

    void set_hw_params();
public:
    ALSAPCMDevice(
            QObject* parent,
            RequestType r_type
            );

    ~ALSAPCMDevice();

    bool open_capture_device();
    bool open_playback_device();
    void close();

private:
    void findCaptureDevice(char *devname);   
    char* allocate_buffer();
    unsigned int get_frames_per_period();
    unsigned int get_bytes_per_frame();

    int init_wav_header(void);
    int init_wav_file( char *);
    int close_wav_file(void);
    void set_record_file(char *aFileName);
    int start_capture();
    int start_recognize();
    string recognize_from_microphone();
    void processRawData(char* buffer, int cap_size);

    unsigned int capture_into_buffer(char* buffer);
    unsigned int play_from_buffer(char* buffer);
    void searchAudioDevice();

    CPlotter *plotter{};
    void showPlotter();

    uint8 utt_started, in_speech;            // flags for tracking active speech - has speech started? - is speech currently happening?
    string decoded_speech;
    ps_decoder_t *mDecoder{};                  // create pocketsphinx decoder structure
    cmd_ln_t *config{};                        // create configuration structure
    ad_rec_t *mDevice{};                       // create audio recording structure - for use with ALSA functions

    struct wav_header wav_h;
    char * wav_name;
    char * fname{};
    FILE * fwav{};

    float *d_realFftData;
    float *d_iirFftData;    
    float d_fftAvg; //set by user;
    float *signalInput;
    float signal_level;
    fftw_plan  planFft{nullptr};
    fftw_complex* in{};
    fftw_complex* out{};

    KalmanFilter *plotter_filter;
    long start_time{0};
    long end_time{0};
    long dt{0};

    pthread_mutex_t lock_fftw = PTHREAD_MUTEX_INITIALIZER;

protected:
    void run() override; // reimplemented from QThread
};

#endif
