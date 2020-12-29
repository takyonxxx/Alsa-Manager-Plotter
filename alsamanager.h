#ifndef AI_H
#define AI_H

#include "alsadevices.h"
#include "voicetranslator.h"

class AlsaManager: public QObject
{
    Q_OBJECT
public:
    AlsaManager(QObject* parent = nullptr);
    ~AlsaManager();
    void init(int argc, char *argv[]);

private:   
    ALSAPCMDevice *device{};
    VoiceTranslator *google_speech{};
};

#endif // AI_H
