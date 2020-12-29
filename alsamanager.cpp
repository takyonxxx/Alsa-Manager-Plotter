#include "alsamanager.h"

AlsaManager::AlsaManager(QObject *parent) : QObject(parent)
{
}

AlsaManager::~AlsaManager()
{    
    if(device)
    {
        device->close();
        device->quit();
        device->wait();
    }

    if(google_speech)
    {
        google_speech->close();
        google_speech->quit();
        google_speech->wait();
    }
}

void AlsaManager::init(int argc, char *argv[])
{
    Q_UNUSED(argc);

    QString argType{"-d"};    
    RequestType type{};
    int channel;

    int i=0;
    while(argv[i]!=NULL)
    {
        if(i==1)
            argType = argv[i];
        i++;
    }

    if(argType.contains("-r"))
        type = RequestType::Record;
    else if(argType.contains("-p"))
        type = RequestType::Play;
    else if(argType.contains("-d"))
        type = RequestType::Data;
    else if(argType.contains("-s"))
        type = RequestType::Speech;

    if (type == RequestType::Speech)
    {
        google_speech = new VoiceTranslator(this);

        connect(google_speech, &VoiceTranslator::speechChanged, [](auto speech)
        {            
            qDebug() << speech;
        });

        google_speech->start();
    }
    else
    {        
        device = new ALSAPCMDevice(this, type);
        device->start();
    }
}


