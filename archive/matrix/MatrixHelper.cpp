#ifndef MATRIX_HELPER
#include "MatrixHelper.h"
#endif

const uint16_t kMatrixWidth = 64;       // Set to the width of your display, must be a multiple of 8
const uint16_t kMatrixHeight = 32;      // Set to the height of your display
const uint8_t kRefreshDepth = 36;       // Tradeoff of color quality vs refresh rate, max brightness, and RAM usage.  36 is typically good, drop down to 24 if you need to.  On Teensy, multiples of 3, up to 48: 3, 6, 9, 12, 15, 18, 21, 24, 27, 30, 33, 36, 39, 42, 45, 48.  On ESP32: 24, 36, 48
const uint8_t kDmaBufferRows = 4;       // known working: 2-4, use 2 to save RAM, more to keep from dropping frames and automatically lowering refresh rate.  (This isn't used on ESP32, leave as default)
const uint8_t kPanelType = SM_PANELTYPE_HUB75_32ROW_MOD16SCAN;   // Choose the configuration that matches your panels.  See more details in MatrixCommonHub75.h and the docs: https://github.com/pixelmatix/SmartMatrix/wiki
const uint32_t kMatrixOptions = (SM_HUB75_OPTIONS_NONE);        // see docs for options: https://github.com/pixelmatix/SmartMatrix/wiki
const uint8_t kBackgroundLayerOptions = (SM_BACKGROUND_OPTIONS_NONE);
const uint8_t kScrollingLayerOptions = (SM_SCROLLING_OPTIONS_NONE);
const uint8_t kIndexedLayerOptions = (SM_INDEXED_OPTIONS_NONE);

SMARTMATRIX_ALLOCATE_BUFFERS(matrix, kMatrixWidth, kMatrixHeight, kRefreshDepth, kDmaBufferRows, kPanelType, kMatrixOptions);

SMARTMATRIX_ALLOCATE_BACKGROUND_LAYER(backgroundLayer, kMatrixWidth, kMatrixHeight, COLOR_DEPTH, kBackgroundLayerOptions);

#ifdef USE_ADAFRUIT_GFX_LAYERS
  // there's not enough allocated memory to hold the long strings used by this sketch by default, this increases the memory, but it may not be large enough
  SMARTMATRIX_ALLOCATE_GFX_MONO_LAYER(scrollingLayer, kMatrixWidth, kMatrixHeight, 6*1024, 1, COLOR_DEPTH, kScrollingLayerOptions);
#else
  SMARTMATRIX_ALLOCATE_SCROLLING_LAYER(scrollingLayer, kMatrixWidth, kMatrixHeight, COLOR_DEPTH, kScrollingLayerOptions);
#endif

SMARTMATRIX_ALLOCATE_INDEXED_LAYER(indexedLayer, kMatrixWidth, kMatrixHeight, COLOR_DEPTH, kIndexedLayerOptions);


const int defaultBrightness = (100*255)/100;        // full (100%) brightness
const int clockBrightness = (60*255)/100;        // full (100%) brightness
const int wifiBrightness = (20*255)/100;        // full (100%) brightness
const int barChartBrightness= (20*255)/100;        // full (100%) brightness

//const int defaultBrightness = (15*255)/100;       // dim: 15% brightness
const int defaultScrollOffset = 6;
const rgb24 defaultBackgroundColor = {0x00, 0x00, 0x00};


// we have one matrix, one background one scrollingLayer, and one indexedLayer
void setupMatrix(void)
{
  matrix.addLayer(&backgroundLayer);
  matrix.addLayer(&scrollingLayer);
  matrix.addLayer(&indexedLayer);
  matrix.begin();

  matrix.setBrightness(defaultBrightness);

  scrollingLayer.setOffsetFromTop(defaultScrollOffset);

  backgroundLayer.enableColorCorrection(true);
  
}

// we have one matrix, one scrollingLayer, and one indexedLayer
void setupMatrixStartBackground(void)
{
  backgroundLayer.fillScreen(defaultBackgroundColor);
  backgroundLayer.swapBuffers();
}



  void drawRandomTransition() {
   const ulong currentMillis = millis();
   const uint transitionTime = 2000;
   // first we clean everything
   cleanLayersAndSwapBuffers();
   backgroundLayer.fillScreen(defaultBackgroundColor);
   Serial.println("drawing drawRandomTransition");
        const int testBitmapWidth = 15;
        const int testBitmapHeight = 15;
        uint8_t testBitmap[] = {
            _______X, ________,
            ______XX, X_______,
            ______XX, X_______,
            ______XX, X_______,
            _____XXX, XX______,
            XXXXXXXX, XXXXXXX_,
            _XXXXXXX, XXXXXX__,
            __XXXXXX, XXXXX___,
            ___XXXXX, XXXX____,
            ____XXXX, XXX_____,
            ___XXXXX, XXXX____,
            ___XXXX_, XXXX____,
            __XXXX__, _XXXX___,
            __XX____, ___XX___,
            __X_____, ____X___,
        };


        // currentMillis = millis();

        backgroundLayer.fillScreen({0, 0, 0x80});
        backgroundLayer.swapBuffers();

        while (millis() < currentMillis + transitionTime)
        {
          backgroundLayer.drawMonoBitmap(random(matrix.getScreenWidth() + testBitmapWidth) - testBitmapWidth,
                                         random(matrix.getScreenHeight() + testBitmapHeight) - testBitmapHeight,
                                         testBitmapWidth, testBitmapHeight, {(uint8_t)random(256), (uint8_t)random(256), 0}, testBitmap);
          backgroundLayer.swapBuffers();
          delay(100);
        }
        
        cleanLayersAndSwapBuffers();

    
  }



// we have one matrix, one scrollingLayer, and one indexedLayer
void drawClock() {
   const ulong currentMillis = millis();
   const uint transitionTime = 6000;
   //int x = kMatrixWidth/2-19;
   time_t t;
   
    // display a simple message - will stay on the screen if calls to the RTC library fail later
    matrix.setBrightness(clockBrightness);
    cleanLayersWithoutSwapBuffer();
    indexedLayer.setFont(gohufont11b);



    /* Draw Clock to SmartMatrix */
    
    while (millis() < currentMillis + transitionTime) {
        t = time(NULL);
        struct tm *tm;
        char timeBuffer[25]="";
        char dayBuffer[25]="";
        char dateBuffer[25]="";
        int x = kMatrixWidth/2-23;
        tm = localtime(&t);
        
       
        static const char* const wd[7] = {"Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"};
        
        sprintf(dateBuffer,"%04d/%02d/%02d", tm->tm_year+1900, tm->tm_mon+1, tm->tm_mday);
        sprintf(dayBuffer,"%s", wd[tm->tm_wday]);

        sprintf(timeBuffer, "%02d:%02d:%02d", tm->tm_hour, tm->tm_min,tm->tm_sec);

        backgroundLayer.setFont(gohufont11b);
        backgroundLayer.drawString(2, 2, {0x05, 0x2B, 0xE9}, dateBuffer);

        backgroundLayer.setFont(gohufont11b);
        backgroundLayer.drawString(2, 13, {0x00, 0x50, 0x00}, dayBuffer);
        


        indexedLayer.fillScreen(0);
        indexedLayer.setIndexedColor(1, {0xff, 0x33, 0x33});
        indexedLayer.setFont(gohufont11b);
        indexedLayer.drawString(x, kMatrixHeight - 8, 1, timeBuffer);

        backgroundLayer.swapBuffers();
        indexedLayer.swapBuffers();

        delay(100);
    }
    cleanLayersAndSwapBuffers();
}

  // we have one matrix, one scrollingLayer, and one indexedLayer
  void drawWIFIDetails()
  {
        const ulong currentMillis = millis();
        const uint transitionTime = 5500;
        String wifiBuffer("WIFI Details \n");
        // "Drawing Functions"
        matrix.setBrightness(wifiBrightness);
        backgroundLayer.fillScreen(defaultBackgroundColor);
        indexedLayer.setFont(gohufont11b);
        cleanLayersWithoutSwapBuffer();
        
    


        Serial.println("drawing Wifi details start");
        scrollingLayer.setColor({0xff, 0xff, 0xff});
        scrollingLayer.setMode(wrapForward);
        scrollingLayer.setSpeed(50);
        scrollingLayer.setFont(font5x7);
        wifiBuffer.concat(WiFi.localIP().toString());
        Serial.println(wifiBuffer.c_str());
        
        

        scrollingLayer.start(wifiBuffer.c_str(), 1);

        //const int delayBetweenCharacters = 1000;
        //const int leftEdgeOffset = 1;
        // currentMillis = millis();

        backgroundLayer.fillScreen({0, 0x80, 0x80});
        backgroundLayer.swapBuffers();
        while (millis() < currentMillis + transitionTime);
        backgroundLayer.fillScreen(defaultBackgroundColor);
        
        cleanLayersAndSwapBuffers();
        Serial.println("drawing Wifi details end");
  }


 // we have one matrix, one scrollingLayer, and one indexedLayer
  void drawdrawHorizonalBarchart()
  {
        const ulong currentMillis = millis();
        const uint transitionTime = 5500;
        uint8_t kRefreshRate = 50; // one graph per sec
        //level 
        const uint8_t barWith = 4;
        const uint8_t barSeparation = 1;
        const uint8_t numberOfBars = kMatrixWidth/ (barWith+barSeparation) +1;
        int8_t levels[numberOfBars]; // KMatrix / (with +1)+ 1 =
        
        // clean all the previous layers
        matrix.setBrightness(barChartBrightness);
        cleanLayersWithoutSwapBuffer();
        
        
        
        cleanLayersAndSwapBuffers();
        while (millis() < currentMillis + transitionTime)
        {
             cleanLayersWithoutSwapBuffer();
              //first we get the value to put in the level. This will be replaced by a reading of a real microphone
             uint8_t x0,y0,x1,y1; 
             for (uint8_t i = 0; i < numberOfBars; i++) {
              levels[i]= random(-128, 127);
             }
             Serial.println("------------------------");

            backgroundLayer.fillScreen(0);
            for (uint8_t i = 0; i < numberOfBars; i++) {
              
              int middle = kMatrixHeight/2;
              if(levels[i]<0) {// if not necessary
                y0 = kMatrixHeight/2;
                y1= middle-((levels[i]*middle)/128);
              }else {
                y0= middle-((levels[i]*middle)/128);
                y1 =  kMatrixHeight/2;
              }
              
              if(i== 0) {
                x0 = 0;
                x1 = barWith;
              }else {
                x0 = i*(barWith + barSeparation);
                x1 = i*(barWith + barSeparation) + barWith;
              }

              int color = abs(levels[i]) *2;
              Serial.printf("for Bar %d, the value was %d, the rectangle will be x0 : %d, x1 : %d, y0 %d, y1 : %d \n",i,levels[i],x0,x1,y0,y1);
              //SM_RGB color = CRGB(CHSV(i * 15, 255, 255));
              rgb24 fillColor = {255, color, color};
              for (int xP = x0; xP <= x1; xP++) {
                  for(int yP=y0; yP <= y1;yP++) {
                    backgroundLayer.drawPixel(xP, yP, fillColor);
                  }
              }              
              }
            backgroundLayer.swapBuffers(); // update the display
            Serial.println("------------------------");
            delay(100 / kRefreshRate); // wait for the refresh rate period
        }


  
        
        cleanLayersAndSwapBuffers();
        Serial.println("drawing drawdrawHorizonalBarchart details end");
  }


  // we have one matrix, one scrollingLayer, and one indexedLayer
void drawRandomPixelMatrix(){
  const ulong currentMillis = millis();
  const uint transitionTime = 10000;
  uint8_t kRefreshRate = 250;
  int color  =  CRGB::Green;
  matrix.setBrightness(clockBrightness);
  cleanLayersAndSwapBuffers();
  
  indexedLayer.setIndexedColor(1, {0x00, 0xff, 0x00});

  while (millis() < currentMillis + transitionTime) {
      uint8_t x = random(kMatrixWidth);
      uint8_t y = random(kMatrixHeight);
      indexedLayer.drawPixel(x, y,color); // draw a green pixel at a random location
      indexedLayer.swapBuffers(); // update the display
      delay(1000 / kRefreshRate); // wait for the refresh rate period
  }
  cleanLayersAndSwapBuffers();
  

  
}
/// @brief 
void cleanLayersAndSwapBuffers()
{
  backgroundLayer.fillScreen(defaultBackgroundColor);
  scrollingLayer.fillScreen(0);
  indexedLayer.fillScreen(0);

  indexedLayer.swapBuffers(true);
  backgroundLayer.swapBuffers(true);
  scrollingLayer.swapBuffers(true);
  Serial.println("filling screen with 0 and swapping buffer done");
}

void cleanLayersWithoutSwapBuffer()
{
  backgroundLayer.fillScreen(defaultBackgroundColor);
  scrollingLayer.fillScreen(0);
  indexedLayer.fillScreen(0);
  indexedLayer.swapBuffers(false);
  backgroundLayer.swapBuffers(false);
  scrollingLayer.swapBuffers(false);
  Serial.println("filling screen with 0 without swapping buffer done");
}



