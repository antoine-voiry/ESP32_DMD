#ifndef DMD_RENDERER_H
#define DMD_RENDERER_H

#include <string>
#include <vector>
#include <Arduino.h>
#include "matrix/Hub75_Matrix.h"
#define DELAY_CRITICAL_MSG 8000
class DMDRenderer {
private:
    bool standalone;
    int brightness;
    // Add your DMD hardware interface here
    // For example: DMD dmd;
    Hub75_Matrix* _dmd = nullptr; // Pointer to the matrix object
    DMDRenderer() = delete; // Prevent default constructor
    std::string last_action_text;
    std::string last_action_name;

public:
    DMDRenderer(Hub75_Matrix* matrix);
    ~DMDRenderer();

    // Basic rendering methods
    void renderText(const std::string& text);
    void renderText(const std::string& text, bool isDartScore);
    void renderText(const std::string& text, const std::string& sens, int iterate, bool isDartScore , const std::string& fontName );
    void renderGif(const std::string& gifPath , bool aleatoire );
    void renderImage(const std::string& imagePath , bool aleatoire);
    void renderTime(const std::string& command, int only = 0);
    void renderScore(const std::string& score);
    void renderCarrousel();
    void renderTime(bool startOrStopTime);
    void swapBuffers(bool copy = false);
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