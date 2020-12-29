#include "alsadevices.h"

ALSAPCMDevice::ALSAPCMDevice(QObject *parent, RequestType r_type)
    : QThread(parent),     
      r_type(r_type)
{
    sample_rate = SAMPLING_RATE;
    frames_per_period = (int)(sample_rate/60.0); ;
    format = FORMAT;

    char devname[10] = {0};
    findCaptureDevice(devname);
    device_name = devname;
    channels = 2;

    QString fileName = ":/robot.dic";
    createFile(fileName);
    fileName = ":/robot.lm";
    createFile(fileName);

    plotter_filter = new KalmanFilter(KF_VAR_ACCEL);
    plotter_filter->Reset(0.0);

    printf("\n");

    if(r_type == RequestType::Record)
    {
        printf("Request type: Record\n");
        open_capture_device();
    }
    else if(r_type == RequestType::Play)
    {
        printf("Request type: Play\n");
        open_capture_device();
        open_playback_device();
    }
    else if(r_type == RequestType::Data)
    {
        printf("Request type: Data\n");
        open_capture_device();
    }
    else if(r_type == RequestType::Speech)
    {
        printf("Request type: Speech\n");
    }

    if(r_type != RequestType::Speech)
        showPlotter();
}

ALSAPCMDevice::~ALSAPCMDevice()
{
    close();
}

void ALSAPCMDevice::findCaptureDevice(char *devname)
{
    int idx, dev, err;
    snd_ctl_t *handle;
    snd_ctl_card_info_t *info;
    snd_pcm_info_t *pcminfo;
    char str[128];
    bool found = false;

    snd_ctl_card_info_alloca(&info);
    snd_pcm_info_alloca(&pcminfo);
    printf("\n");

    idx = -1;
    while (!found)
    {
        if ((err = snd_card_next(&idx)) < 0) {
            printf("Card next error: %s\n", snd_strerror(err));
            break;
        }
        if (idx < 0)
            break;
        sprintf(str, "hw:CARD=%i", idx);
        if ((err = snd_ctl_open(&handle, str, 0)) < 0) {
            printf("Open error: %s\n", snd_strerror(err));
            continue;
        }
        if ((err = snd_ctl_card_info(handle, info)) < 0) {
            printf("HW info error: %s\n", snd_strerror(err));
            continue;
        }

        dev = -1;
        while (1) {
            snd_pcm_sync_id_t sync;
            if ((err = snd_ctl_pcm_next_device(handle, &dev)) < 0) {
                printf("  PCM next device error: %s\n", snd_strerror(err));
                break;
            }
            if (dev < 0)
                break;
            snd_pcm_info_set_device(pcminfo, dev);
            snd_pcm_info_set_subdevice(pcminfo, 0);
            snd_pcm_info_set_stream(pcminfo, SND_PCM_STREAM_CAPTURE);
            if ((err = snd_ctl_pcm_info(handle, pcminfo)) < 0) {
                printf("Sound card - %i - '%s' has no capture device.\n",
                       snd_ctl_card_info_get_card(info), snd_ctl_card_info_get_name(info));
                continue;
            }
            printf("Sound card - %i - '%s' has capture device.\n", snd_ctl_card_info_get_card(info), snd_ctl_card_info_get_name(info));
            sprintf(devname, "plughw:%d,0", snd_ctl_card_info_get_card(info));
            found = true;
            break;
        }
        snd_ctl_close(handle);
    }

    snd_config_update_free_global();
}

void ALSAPCMDevice::showPlotter()
{
    in= (fftw_complex*)fftw_malloc(sizeof(fftw_complex)*frames_per_period);
    out= (fftw_complex*)fftw_malloc(sizeof(fftw_complex)*frames_per_period);

    d_realFftData = (float*) malloc(frames_per_period * sizeof(float));
    d_iirFftData  = (float*) malloc(frames_per_period * sizeof(float));
    signalInput   = (float*) malloc(frames_per_period * sizeof(float));

    for (int i = 0; i < frames_per_period; i++)
        d_iirFftData[i] = -70.0f;  // dBFS

    for (int i = 0; i < frames_per_period; i++)
        d_realFftData[i] = -70.0f;

    d_fftAvg = 1.0 - 1.0e-2 * ((float)75);
    d_realFftData = (float*) malloc(frames_per_period * sizeof(float));
    d_iirFftData  = (float*) malloc(frames_per_period * sizeof(float));

    for (int i = 0; i < frames_per_period; i++)
        d_iirFftData[i] = -70.0f;  // dBFS

    for (int i = 0; i < frames_per_period; i++)
        d_realFftData[i] = -70.0f;

    plotter = new CPlotter();
    plotter->setGeometry(0,0,800, 600);

    plotter->setTooltipsEnabled(true);

    plotter->setTooltipsEnabled(true);
    plotter->setSampleRate((float)sample_rate/(float)2.0);
    plotter->setSpanFreq(static_cast<quint32>((float)sample_rate/(float)2.0));
    plotter->setCenterFreq(static_cast<quint64>((float)sample_rate/(float)(4.0)));

    plotter->setFftCenterFreq(0);
    plotter->setFftRate(sample_rate/frames_per_period);
    plotter->setFftRange(-140.0f, 20.0f);
    plotter->setPandapterRange(-140.f, 20.f);
    plotter->setHiLowCutFrequencies(KHZ(1), KHZ(1));
    plotter->setDemodRanges(KHZ(1), -KHZ(1), KHZ(1),KHZ(1), true);

    plotter->setFreqDigits(1);
    plotter->setFreqUnits(1000);
    plotter->setPercent2DScreen(50);
    plotter->setFilterBoxEnabled(true);
    plotter->setCenterLineEnabled(true);
    plotter->setBookmarksEnabled(true);
    plotter->setClickResolution(1);

    plotter->setFftPlotColor(QColor("#CEECF5"));

    //plotter->setPeakDetection(true ,2);
    plotter->setFftFill(true);
    d_fftAvg = 1.0 - 1.0e-2 * ((float)75);
    plotter->show();
}

bool ALSAPCMDevice::open_capture_device() {

    snd_pcm_hw_params_t *params;

    int err = 0;

    if ((err = snd_pcm_open(&capture_handle, device_name.c_str(), SND_PCM_STREAM_CAPTURE, 0)) < 0)

    {
        std::cerr << "cannot open audio device " << capture_handle << "(" << snd_strerror(err) << ", " << err << ")" << "\n";
        return OPEN_ERROR;
    }

    if ((err = snd_pcm_hw_params_malloc(&params)) < 0)
    {
        std::cerr << "cannot allocate hardware parameter structure " << "(" << snd_strerror(err) << ", " << err << ")" << "\n";
        return MALLOC_ERROR;
    }

    if ((err = snd_pcm_hw_params_any(capture_handle, params)) < 0)
    {
        std::cerr << "cannot initialize hardware parameter structure " << "(" << snd_strerror(err) << ", " << err << ")" << "\n";
        return ANY_ERROR;
    }

    if ((err = snd_pcm_hw_params_set_access(capture_handle, params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0)
    {
        std::cerr << "cannot set access type " << "(" << snd_strerror(err) << ", " << err << ")" << "\n";
        return ACCESS_ERROR;
    }

    if ((err = snd_pcm_hw_params_set_format(capture_handle, params, SND_PCM_FORMAT_S16_LE)) < 0)
    {
        std::cerr << "cannot set sample format " << "(" << snd_strerror(err) << ", " << err << ")" << "\n";
        return FORMAT_ERROR;
    }

    if ((err = snd_pcm_hw_params_set_rate_near(capture_handle, params, &sample_rate, 0)) < 0)
    {
        std::cerr << "cannot set sample rate " << "(" << snd_strerror(err) << ", " << err << ")" << "\n";
        return RATE_ERROR;
    }

    if ((err = snd_pcm_hw_params_set_channels(capture_handle, params, channels))< 0)
    {
        std::cerr << "cannot set channel count " << "(" << snd_strerror(err) << ", " << err << ")" << "\n";
        return CHANNELS_ERROR;
    }

    if ((err =  snd_pcm_hw_params_set_period_size_near(capture_handle, params, &frames_per_period, 0)) < 0)
    {
        std::cerr << "cannot set period_size " << "(" << snd_strerror(err) << ", " << err << ")" << "\n";
        return RATE_ERROR;
    }

    if ((err = snd_pcm_hw_params(capture_handle, params)) < 0)
    {
        std::cerr << "cannot set parameters " << "(" << snd_strerror(err) << ", " << err << ")" << "\n";
        return PARAMS_ERROR;
    }

    if ((err = snd_pcm_prepare(capture_handle)) < 0)
    {
        std::cerr << "cannot prepare audio interface for use " << "(" << snd_strerror(err) << ", " << err << ")" << "\n";
        return PREPARE_ERROR;
    }

    if ((err = snd_pcm_start(capture_handle)) < 0)
    {
        std::cerr << "cannot start soundcard " << "(" << snd_strerror(err) << ", " << err << ")" << "\n";
        return START_ERROR;
    }


    unsigned int val;
    int dir;

    snd_pcm_hw_params_get_period_size(params, &frames_per_period, &dir);
    int size = frames_per_period * 4; /* 2 bytes/sample, 2 channels */

    printf("\n");
    //printf("ALSA library version: %s\n", SND_LIB_VERSION_STR);
    printf("Device type: capture\n");
    printf("Device name = '%s'\n", snd_pcm_name(capture_handle));
    snd_pcm_hw_params_get_channels(params, &val);
    printf("Channels = %d\n", val);
    snd_pcm_hw_params_get_rate(params, &val, &dir);
    printf("Rate = %d bps\n", val);
    printf("Size_of_one_frame = %d frames\n", get_frames_per_period());
    val = snd_pcm_hw_params_get_sbits(params);
    printf("Significant bits = %d\n", val);
    printf("Size = %d\n", size);
    printf("\n");
    return true;
}

bool ALSAPCMDevice::open_playback_device()
{
    snd_pcm_hw_params_t *params;
    int err = 0;

    if ((err = snd_pcm_open(&playback_handle, device_name.c_str(), SND_PCM_STREAM_PLAYBACK, 0)) < 0)

    {
        std::cerr << "cannot open audio device " << playback_handle << "(" << snd_strerror(err) << ", " << err << ")" << "\n";
        return OPEN_ERROR;
    }

    if ((err = snd_pcm_hw_params_malloc(&params)) < 0)
    {
        std::cerr << "cannot allocate hardware parameter structure " << "(" << snd_strerror(err) << ", " << err << ")" << "\n";
        return MALLOC_ERROR;
    }

    if ((err = snd_pcm_hw_params_any(playback_handle, params)) < 0)
    {
        std::cerr << "cannot initialize hardware parameter structure " << "(" << snd_strerror(err) << ", " << err << ")" << "\n";
        return ANY_ERROR;
    }

    if ((err = snd_pcm_hw_params_set_access(playback_handle, params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0)
    {
        std::cerr << "cannot set access type " << "(" << snd_strerror(err) << ", " << err << ")" << "\n";
        return ACCESS_ERROR;
    }

    if ((err = snd_pcm_hw_params_set_format(playback_handle, params, SND_PCM_FORMAT_S16_LE)) < 0)
    {
        std::cerr << "cannot set sample format " << "(" << snd_strerror(err) << ", " << err << ")" << "\n";
        return FORMAT_ERROR;
    }

    if ((err = snd_pcm_hw_params_set_rate_near(playback_handle, params, &sample_rate, 0)) < 0)
    {
        std::cerr << "cannot set sample rate " << "(" << snd_strerror(err) << ", " << err << ")" << "\n";
        return RATE_ERROR;
    }

    if ((err = snd_pcm_hw_params_set_channels(playback_handle, params, channels))< 0)
    {
        std::cerr << "cannot set channel count " << "(" << snd_strerror(err) << ", " << err << ")" << "\n";
        return CHANNELS_ERROR;
    }

    if ((err =  snd_pcm_hw_params_set_period_size_near(playback_handle, params, &frames_per_period, 0)) < 0)
    {
        std::cerr << "cannot set period_size " << "(" << snd_strerror(err) << ", " << err << ")" << "\n";
        return RATE_ERROR;
    }

    if ((err = snd_pcm_hw_params(playback_handle, params)) < 0)
    {
        std::cerr << "cannot set parameters " << "(" << snd_strerror(err) << ", " << err << ")" << "\n";
        return PARAMS_ERROR;
    }

    if ((err = snd_pcm_prepare(playback_handle)) < 0)
    {
        std::cerr << "cannot prepare audio interface for use " << "(" << snd_strerror(err) << ", " << err << ")" << "\n";
        return PREPARE_ERROR;
    }

    if ((err = snd_pcm_start(playback_handle)) < 0)
    {
        std::cerr << "cannot start soundcard " << "(" << snd_strerror(err) << ", " << err << ")" << "\n";
        return START_ERROR;
    }


    unsigned int val;
    int dir;

    snd_pcm_hw_params_get_period_size(params, &frames_per_period, &dir);
    int size = frames_per_period * 4; /* 2 bytes/sample, 2 channels */

    printf("\n");
    //printf("ALSA library version: %s\n", SND_LIB_VERSION_STR);
    printf("Device type: playback\n");
    printf("Device name = '%s'\n", snd_pcm_name(playback_handle));
    snd_pcm_hw_params_get_channels(params, &val);
    printf("Channels = %d\n", val);
    snd_pcm_hw_params_get_rate(params, &val, &dir);
    printf("Rate = %d bps\n", val);
    printf("Size_of_one_frame = %d frames\n", get_frames_per_period());
    val = snd_pcm_hw_params_get_sbits(params);
    printf("Significant bits = %d\n", val);
    printf("Size = %d\n", size);
    printf("\n");
    return true;
}

void ALSAPCMDevice::close() {

    m_stop = true;

    if(playback_handle)
    {
        snd_pcm_drain(playback_handle);
        snd_pcm_close(playback_handle);
    }

    if(capture_handle)
    {
        snd_pcm_drain(capture_handle);
        snd_pcm_close(capture_handle);
    }

}

char* ALSAPCMDevice::allocate_buffer() {
    unsigned int size_of_one_frame = (snd_pcm_format_width(format)/8) * channels;
    return (char*) calloc(frames_per_period, size_of_one_frame);
}

unsigned int ALSAPCMDevice::get_frames_per_period() {
    return frames_per_period;
}

unsigned int ALSAPCMDevice::get_bytes_per_frame() {
    unsigned int size_of_one_frame = (snd_pcm_format_width(format)/8) * channels;
    return size_of_one_frame;
}


int ALSAPCMDevice::init_wav_header()
{
    wav_h.ChunkID[0]     = 'R';
    wav_h.ChunkID[1]     = 'I';
    wav_h.ChunkID[2]     = 'F';
    wav_h.ChunkID[3]     = 'F';

    wav_h.Format[0]      = 'W';
    wav_h.Format[1]      = 'A';
    wav_h.Format[2]      = 'V';
    wav_h.Format[3]      = 'E';

    wav_h.Subchunk1ID[0] = 'f';
    wav_h.Subchunk1ID[1] = 'm';
    wav_h.Subchunk1ID[2] = 't';
    wav_h.Subchunk1ID[3] = ' ';

    wav_h.Subchunk2ID[0] = 'd';
    wav_h.Subchunk2ID[1] = 'a';
    wav_h.Subchunk2ID[2] = 't';
    wav_h.Subchunk2ID[3] = 'a';

    wav_h.NumChannels = channels;
    wav_h.BitsPerSample = 16;
    wav_h.Subchunk2Size = 300 * MAX_SAMPLES * (uint32_t) wav_h.NumChannels * (uint32_t) wav_h.BitsPerSample / 8;
    //wav_h.Subchunk2Size = 0xFFFFFFFF;
    wav_h.ChunkSize = (uint32_t) wav_h.Subchunk2Size + 36;
    wav_h.Subchunk1Size = 16;
    wav_h.AudioFormat = 1;
    wav_h.SampleRate = sample_rate;
    wav_h.ByteRate = (uint32_t) wav_h.SampleRate
            * (uint32_t) wav_h.NumChannels
            * (uint32_t) wav_h.BitsPerSample / 8;
    wav_h.BlockAlign = (uint32_t) wav_h.NumChannels * (uint32_t) wav_h.BitsPerSample / 8;

    return EXIT_SUCCESS;
}

int ALSAPCMDevice::init_wav_file(char *fname)
{
    if(FileExists(fname))
    {
        std::remove(fname); // delete file
    }

    fwav = fopen(fname, "wb");

    if (fwav != NULL)
        fwrite(&wav_h, 1, sizeof(wav_h), fwav);
    else
    {
        std::cerr << "cannot open wav file to write data" << "\n";
        return FOPEN_ERROR;
    }

    return EXIT_SUCCESS;
}

int ALSAPCMDevice::close_wav_file()
{
    if (fwav != nullptr)
        fclose(fwav);
    else
    {
        std::cerr << "cannot close wav file" << "\n";
        return FCLOSE_ERROR;
    }

    std::cout << "Recording stopped.\n";

    return EXIT_SUCCESS;
}

void ALSAPCMDevice::set_record_file(char *aFileName)
{
    fname = aFileName;
    if(fname)
    {
        init_wav_header();
        init_wav_file(fname);
    }
}

unsigned int ALSAPCMDevice::capture_into_buffer(char* buffer) {

    snd_pcm_sframes_t frames_read = snd_pcm_readi(capture_handle, buffer, get_frames_per_period());

    if(frames_read == 0) {
        fprintf(stderr, "End of file.\n");
        return 0;
    }

    // A -ve return value means an error.
    if(frames_read < 0)
    {
        fprintf(stderr, "error from readi: %s\n", snd_strerror(frames_read));
        return 0;
    }
    return frames_read;
}

unsigned int ALSAPCMDevice::play_from_buffer(char* buffer) {

    snd_pcm_sframes_t frames_written = snd_pcm_writei(playback_handle, buffer, get_frames_per_period());

    if (frames_written == -EPIPE) {
        /* EPIPE means underrun */
        snd_pcm_prepare(playback_handle);
    } else if (frames_written < 0) {
        fprintf(stderr, "error from writei: %s\n", snd_strerror(frames_written));
    }
    return frames_written;
}

void ALSAPCMDevice::processRawData(char* buffer, int cap_size)
{      
    int fftSize = cap_size;
    float pwr;
    float pwr_scale = 1.0 / ((float)fftSize * (float)fftSize);
    d_fftAvg = static_cast<float>(1.0 - 1.0e-2 * 70);
    signalInput = (float*)buffer;

    pthread_mutex_lock(&lock_fftw);

    if(fftSize > 0)
    {
        int nCount = 0;
        for (int dw = 0; dw < fftSize; dw++)
        {
            //copy audio signal to fft real component for left channel
            in[nCount][0] = (double)((short*)signalInput)[dw++] / 127.5f - 1.f;
            //copy audio signal to fft real component for right channel
            in[nCount++][0] = (double)((short*)signalInput)[dw] / 127.5f - 1.f;
        }

        planFft = fftw_plan_dft_1d(fftSize, in, out, FFTW_FORWARD, FFTW_ESTIMATE);

        fftw_execute(planFft);

        for (int i = 0; i < fftSize/2; ++i)
        {
            //printf("%3d %9.5f %9.5f\n", i, in[i][REAL], out[i][REAL]/(2*(fftSize - 1)));
            end_time =  clock();
            dt = float( clock () - start_time );

            pwr  = pwr_scale * mag_sqrd(out[i][REAL], out[i][IMAG]);
            signal_level = 15.f * log10(pwr + 1.0e-20f);

            if(dt > 0)
            {
                plotter_filter->Update(signal_level, KF_VAR_MEASUREMENT, dt);
                signal_level = plotter_filter->GetXAbs();
                fprintf(stderr, "\033[1GSignal Level : \033[1;33m %0.1f dB \033[0m\033[30G", signal_level);
            }

            d_realFftData[i] = signal_level;
            d_iirFftData[i] += d_fftAvg * (d_realFftData[i] - d_iirFftData[i]);
            start_time = end_time;
        }

        if(plotter)
        {
            plotter->setNewFttData(d_iirFftData, d_realFftData, static_cast<int>(fftSize/2));
        }
    }

    if(planFft)
    {
        fftw_destroy_plan(planFft);
    }

    pthread_mutex_unlock(&lock_fftw);
}

int ALSAPCMDevice::start_capture()
{
    if(!capture_handle)
        return SNDREAD_ERROR;

    if (r_type == RequestType::Record)
        set_record_file((char*)"record.wav");

    char* buffer = allocate_buffer();

    do
    {
        auto cap_size = capture_into_buffer(buffer);

        if(r_type == RequestType::Play)
            play_from_buffer(buffer);

        processRawData(buffer, cap_size);

        if (fwav != nullptr)
        {
            fwrite(buffer, 1, get_frames_per_period() * 4, fwav);
        }

    } while (!m_stop); /*esc */

    if (fwav != nullptr)
    {
        fwrite(&wav_h, 1, sizeof(wav_h), fwav);
        close_wav_file();
    }

    return EXIT_SUCCESS;
}

int ALSAPCMDevice::start_recognize()
{
    QString logPath = QDir().currentPath() + "/errors.log";
    QString lmPath = QDir().currentPath() + "/robot.lm";
    QString dicPath = QDir().currentPath() + "/robot.dic";
    config = cmd_ln_init(NULL, ps_args(), TRUE,                                         // Load the configuration structure - ps_args() passes the default values
                         "-hmm", MODELDIR "/en-us/en-us",     // path to the standard english language model
                         "-lm", lmPath.toStdString().c_str(),                                             // custom language model (file must be present)
                         "-dict", dicPath.toStdString().c_str(),                                          // custom dictionary (file must be present)
                         "-remove_noise", "yes",
                         "-logfn", logPath.toStdString().c_str(),
                         nullptr);

    mDecoder = ps_init(config);                                                        // initialize the pocketsphinx decoder
    mDevice = ad_open_dev(device_name.c_str(), (int) cmd_ln_float32_r(config, "-samprate")); // open default microphone at default samplerate

    do{
        decoded_speech = recognize_from_microphone();                 // call the function to capture and decode speech
        std::cout << "Decoded : "<< decoded_speech << std::endl;  // send decoded speech to screen
        if (decoded_speech.compare(KEY_GOODBYE) == 0)        {
            cout << "Key phrase " << KEY_GOODBYE << " detected;" << endl;
        }
        QThread::msleep(1);
    } while (!m_stop);

    ad_close(mDevice);

    return EXIT_SUCCESS;
}

string ALSAPCMDevice::recognize_from_microphone()
{
    bool uttStarted = false;
    const char* data = nullptr;
    int16 buffer[frames_per_period * 4];

    if(ad_start_rec(mDevice) >= 0 && ps_start_utt(mDecoder) >= 0)
    {
        while(true)
        {
            int32 numberSamples = ad_read(mDevice, buffer, frames_per_period * 4);
            if(ps_process_raw(mDecoder, buffer, numberSamples, 0, 0) < 0) break;
            bool inSpeech = (ps_get_in_speech(mDecoder) > 0) ? true : false;
            if(inSpeech && !uttStarted) uttStarted = true;
            if(!inSpeech && uttStarted)
            {
                ps_end_utt(mDecoder);
                ad_stop_rec(mDevice);
                data = ps_get_hyp(mDecoder, nullptr);
                break;
            }
            QThread::msleep(10);
        }

        if(data) return string(data);
    }
    ps_end_utt(mDecoder);
    ad_stop_rec(mDevice);

    return empty_string;
}

void ALSAPCMDevice::run()
{
    if(r_type == RequestType::Speech)
    {
        std::cout << "Recognizing voice..."<< std::endl;
        start_recognize();
    }
    else
    {
        std::cout << "Capturing voice..."<< std::endl;
        start_capture();
    }
}

void ALSAPCMDevice::searchAudioDevice()
{
    void **hints;
    const char *ifaces[] = {"pcm", 0};
    int index = 0;
    void **str;
    char *name;
    char *desc;
    int devIdx = 0;
    size_t tPos;

    snd_config_update();

    while (ifaces[index]) {

        printf("Querying interface %s \n", ifaces[index]);
        if (snd_device_name_hint(-1, ifaces[index], &hints) < 0)
        {
            printf("Querying devices failed for %s.\n", ifaces[index]);
            index++;
            continue;
        }
        str = hints;
        while (*str)
        {
            name = snd_device_name_get_hint(*str, "NAME");
            desc = snd_device_name_get_hint(*str, "DESC");

            string tNameStr = "";
            if (name != NULL)
                tNameStr = string(desc);            

            // search for "default:", if negative result then go on searching for next device
            if ((tNameStr != "") && ((tPos = tNameStr.find("USB")) != string::npos))
            {
                printf("Deafult Sound Card : %d : %s\n%s\n",devIdx, name,desc);
                //snd_device_name_free_hint(hints);

                // return;
            }

            free(name);
            free(desc);
            devIdx++;
            str++;
        }
        index++;
        snd_device_name_free_hint(hints);
    }
    return;
}
