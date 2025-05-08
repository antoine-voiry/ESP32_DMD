#ifndef DMD_RENDERER_H
#define DMD_RENDERER_H

#include <string>
#include <vector>
#include <Arduino.h>

class DMDRenderer {
private:
    bool standalone;
    int brightness;
    // Add your DMD hardware interface here
    // For example: DMD dmd;

public:
    DMDRenderer();
    ~DMDRenderer();

    // Basic rendering methods
    void renderText(const std::string& text, bool val = false);
    void renderGif(const std::string& gifPath = "", bool aleatoire = false);
    void renderImage(const std::string& imagePath = "", bool aleatoire = false);
    void renderTime(const std::string& command, int only = 0);
    
    // Config and status methods
    void stop(const std::string& message);
    void renderFirstStart();
    void renderStandalone();
    void warnIsApply();
    bool scoreReceived(const std::string& score);
    void sendConfigToRaspydarts();
    void runThreading();
    void applyConfig(const std::vector<std::string>& config);
    
    // File management
    void exclude(bool isFolder, const std::string& name, const std::string& path);
    
    // Special features
    void zipPostCodeGeocoding();
    void findLatLonCityname();
    void renderMeteoInTime();
    void renderMeteoPrevisionnelle();
    void renderEDFJoursTempo();
    void renderSOC();
    void playSound();
    
    // Friend class for access to private members
    friend class MessageHandler;
};
#endif