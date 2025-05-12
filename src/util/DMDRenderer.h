#ifndef DMD_RENDERER_H
#define DMD_RENDERER_H

#include <string>
#include <vector>
#include <Arduino.h>
#include "matrix/Hub75_Matrix.h"
class DMDRenderer {
private:
    bool standalone;
    int brightness;
    // Add your DMD hardware interface here
    // For example: DMD dmd;
    Hub75_Matrix* _dmd = nullptr; // Pointer to the matrix object
    DMDRenderer() = delete; // Prevent default constructor

public:
    DMDRenderer(Hub75_Matrix* matrix);
    ~DMDRenderer();

    // Basic rendering methods
    void renderText(const std::string& text);
    void renderText(const std::string& text, bool val);
    void renderText(const std::string& text, const std::string& sens, int iterate, bool val , const std::string& fontName );
    void renderGif(const std::string& gifPath , bool aleatoire );
    void renderImage(const std::string& imagePath , bool aleatoire);
    void renderTime(const std::string& command, int only = 0);
    void renderCarrousel();
    void renderTime(bool startOrStopTime);
    void update();
    
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