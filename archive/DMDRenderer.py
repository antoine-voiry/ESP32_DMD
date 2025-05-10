#!/usr/bin/env python3
# -*- coding: utf-8 -*-
import traceback
import threading
import requests
import random
import socket
import shutil
from os import listdir, remove, mkdir, rmdir, scandir
from os.path import isdir, isfile, join, exists
from textwrap import wrap
from time import time, sleep
from datetime import datetime, timedelta
import paho.mqtt.client as mqtt
from rgbmatrix import RGBMatrix, RGBMatrixOptions, graphics
from PIL import Image, ImageDraw, ImageFont, ImageOps
import locale
from webcolors import rgb_to_name
import json

import numpy as np
from wand.image import Image as WImage

import subprocess

from config import DMDRenderer_Config, DMDRenderer_SOC
from . import DMDRenderer_SpecialsMoves
from exceptions.DMDRenderer_Exceptions import MissingArgumentsException, Raspy2DMDException
from bin.DMDRenderer_Database import DMDRenderer_Database


config = {}
dMDRendererSOC = DMDRenderer_SOC.DMDRenderer_SOC()
dMDRendererConfig = DMDRenderer_Config.DMDRenderer_Config()
dMDRendererSpecialsMoves = DMDRenderer_SpecialsMoves.DMDRenderer_SpecialsMoves()

_stopAffichage = False
_stopHDMIRequested = False
_attractModeRunning = False
_dmdAvailable = True
_IP_Available = False

class DMDRenderer:    
    
    def __init__(self):   
        
        global _stopAffichage
        global _stopHDMIRequested
        global _dmdAvailable
        _stopAffichage = False
        _stopHDMIRequested = False
        ##################################################
        locale.setlocale(locale.LC_ALL, 'fr_FR')  
        ##################################################
        self.stopAffichage = False
        _dmdAvailable = True
        ##################################################
        self.databasePath = "permissionFiles.db"
        DMDRenderer_Database.create_folder_excluded(self.databasePath)
        DMDRenderer_Database.create_file_excluded(self.databasePath)
        self.InitExcluded()
        ##################################################
        self.segments = ["SB", "DB", "X", "S1","S2","S3","S4","S5","S6","S7","S8","S9","S10","S11","S12","S13","S14","S15","S16","S17","S18","S19","S20",
          "D1","D2","D3","D4","D5","D6","D7","D8","D9","D10","D11","D12","D13","D14","D15","D16","D17","D18","D19","D20",
          "T1","T2","T3","T4","T5","T6","T7","T8","T9","T10","T11","T12","T13","T14","T15","T16","T17","T18","T19","T20"]
        ##################################################
        self.cols = 0
        self.rows = 0
        self.led_chain = 0
        self.vertical_parallel_chain = 0
        self.gpio_slowdown = 0
        self.pwm_lsb_nanoseconds = 0
        self.limit_refresh_rate_hz = 0
        self.pictureWidth = 0
        self.pictureHeight = 0
        self.hardware_mapping = 0
        self.brightness = 0
        self.pwm_bits = 0
        self.scan_mode = 0
        self.led_row_addr_type = 0
        self.center_images = 0
        ##################################################
        self.fontsTTFDirectory = ''
        self.imagesDirectory = ''
        self.videosDirectory = ''
        self.gifsDirectory = ''
        self.soundsDirectory = ''
        ##################################################
        self.defaultfont = ''
        self.defaultfontcolor = ''
        self.pictureBackgroundColor = ''
        self.pictureVerticalMargin = 0
        self.maxFontSize = 0
        self.maxCharacter = 0
        self.showing_datehours = 0
        ##################################################
        self.defaultConfig = 0
        self.standalone = 0
        self.respondToRaspydarts = 0
        self.raspydartsAdr = ''
        self.mqttPathResponse = ''
        self.attract_mode = 0
        ##################################################
        ########         Init Matrix ##############
        ##################################################
        self.matrix = RGBMatrix(options = self.GetOptions())
        ##################################################
        self.showOnlyForConfiguration = None
        self.pauseWaiterWanted = datetime.now()
        ##################################################
        self.lastMessage = ''
        ##################################################
        self._pauseevent = False  
        self.pauseevent_cond = threading.Condition(threading.Lock())
        ##################################################
        ########                              ATTRACT MODE
        ##################################################
        global _attractModeRunning
        _attractModeRunning = False
        self.threadForAttractMode = threading.Thread(target=self.ThreadForAttractMode)
        self._dateTimeAct = datetime.now() + timedelta(seconds=int(self.attract_mode))
        ##################################################
        ########                             WEB AVAILABLE
        ##################################################
        global _etatCheckIfInternetIsAvailable
        _etatCheckIfInternetIsAvailable = False
        self.threadForWebAvailable = threading.Thread()
        ##################################################
        ########                          UPDATE AVAILABLE
        ##################################################
        self._gifForWarnUpdate = '/Medias/RaspyDartsDMD/update_gif/update.gif'
        self.updateAvailable = False
        ##################################################
        ########
        ##################################################
    
    ##################################################
    def InitExcluded(self):
        try:
            self.folderExcluded = DMDRenderer_Database.select_folder_excluded(self.databasePath)
            self.fileExcluded = DMDRenderer_Database.select_file_excluded(self.databasePath)
        except Exception as e:
            print(e)
            raise Raspy2DMDException("Unhandled error in InitExcluded :\n{}".format(e))

    def Exclude(self, isDir, name, path):
        try:
            if(isDir):              
                dossierExclu = json.loads(DMDRenderer_Database.select_folder_excluded(self.databasePath))
                resultFolder = next((item for item in dossierExclu if (item['name'] == str(name) and item['path'] == str(path))), None) 
                if(resultFolder == None):
                    DMDRenderer_Database.insert_folder_excluded(self.databasePath, name, path)
                else:
                    DMDRenderer_Database.remove_folder_excluded(self.databasePath, name, path)
            else:
                fichierExclu = json.loads(DMDRenderer_Database.select_file_excluded(self.databasePath))
                resultFile = next((item for item in fichierExclu if (item['name'] == str(name) and item['path'] == str(path))), None) 
                if(resultFile == None):
                    DMDRenderer_Database.insert_file_excluded(self.databasePath, name, path)
                else:
                    DMDRenderer_Database.remove_file_excluded(self.databasePath, name, path)
        except Exception as e:
            raise Raspy2DMDException("Unhandled error in Exclude :\n{}".format(e))

    ##################################################
    def InitSelfVar(self, config):
        ##################################################
        self.fontsTTFDirectory = str(config['Directory']['fontsttf'])
        self.imagesDirectory = str(config['Directory']['images'])
        self.videosDirectory = str(config['Directory']['videos'])
        self.gifsDirectory = str(config['Directory']['gifs'])
        self.scoresDirectory = str(config['Directory']['scores'])
        self.textesDirectory = str(config['Directory']['textes'])
        self.specialsMovesDirectory = str(config['Directory']['specialsmoves'])
        self.patterns = str(config['Directory']['patterns'])
        self.meteo = str(config['Directory']['meteo'])
        self.perfvisualizer = str(config['Directory']['perfvisualizer'])
        self.soundsDirectory = str(config['Directory']['sounds'])
        self.edfjourstempo = str(config['Directory']['edfjourstempo'])
        #self.imagesHDDirectory = str(config['Directory']['imagesHD'])
        #self.gifsHDDirectory = str(config['Directory']['gifsHD'])
        #self.scoresHDDirectory = str(config['Directory']['scoresHD'])
        #self.specialsMovesHDDirectory = str(config['Directory']['specialsmovesHD'])
        #self.meteoHD = str(config['Directory']['meteoHD'])
        ##################################################
        self.standalone = int(config['Running']['standalone'])
        ##################################################
        self.cols = int(config['DMDRenderer']['cols'])
        self.rows = int(config['DMDRenderer']['rows'])
        self.led_chain = int(config['DMDRenderer']['led_chain'])
        self.vertical_parallel_chain = int(config['DMDRenderer']['vertical_parallel_chain'])
        self.gpio_slowdown = int(config['DMDRenderer']['gpio_slowdown'])
        self.pwm_lsb_nanoseconds = int(config['DMDRenderer']['pwm_lsb_nanoseconds'])
        self.limit_refresh_rate_hz = int(config['DMDRenderer']['limit_refresh_rate_hz'])
        self.pictureWidth = int(config['DMDRenderer']['picturewidth'])
        self.pictureHeight = int(config['DMDRenderer']['pictureheight'])
        self.hardware_mapping = str(config['DMDRenderer']['hardware_mapping'])
        self.brightness = int(config['DMDRenderer']['brightness'])
        self.brightnesshours = str(config['DMDRenderer']['brightnesshours']).split(',')
        self.pwm_bits = int(config['DMDRenderer']['pwm_bits'])
        self.scan_mode = int(config['DMDRenderer']['scan_mode'])
        self.rgb_mode = str(config['DMDRenderer']['rgb_mode'])
        self.led_row_addr_type = int(config['DMDRenderer']['led_row_addr_type'])
        self.center_images = int(config['DMDRenderer']['center_images'])
        ##################################################
        self.defaultfont = str(config['TextRenderer']['defaultfont'])
        self.defaultfontcolor = str(config['TextRenderer']['defaultfontcolor'])
        self.pictureVerticalMargin = int(config['TextRenderer']['pictureverticalmargin'])
        self.pictureBackgroundColor = str(config['TextRenderer']['picturebackgroundcolor'])
        self.maxFontSize = int(config['TextRenderer']['maxfontsize'])
        self.maxCharacter = int(config['TextRenderer']['maxcharacter'])
        ##################################################
        self.locale = str(config['ClockRenderer']['format_affichage']) + '.UTF8'
        locale.setlocale(locale.LC_ALL, self.locale)  
        ##################################################
        self.defaultfont_clock = str(config['ClockRenderer']['defaultfont_clock'])
        self.defaultfontcolor_clock = str(config['ClockRenderer']['defaultfontcolor_clock'])
        self.defaultfontcolor_clockshadow = str(config['ClockRenderer']['defaultfontcolor_clockshadow'])
        self.showing_datehours = int(config['ClockRenderer']['showing_datehours'])
        self.clockBackgroundImage = str(config['ClockRenderer']['clockBackgroundImage'])
        self.posX_Date = str(config['ClockRenderer']['posX_Date'])
        self.posY_Date = str(config['ClockRenderer']['posY_Date'])
        self.sizeFont_Date = str(config['ClockRenderer']['sizeFont_Date'])
        self.posX_Hours = str(config['ClockRenderer']['posX_Hours'])
        self.posY_Hours = str(config['ClockRenderer']['posY_Hours'])
        self.sizeFont_Hours = str(config['ClockRenderer']['sizeFont_Hours'])
        self.timeShow_Date = int(config['ClockRenderer']['timeShow_Date'])
        self.timeShow_Hours = int(config['ClockRenderer']['timeShow_Hours'])
        self.decal_horaire = int(config['ClockRenderer']['decal_horaire'])
        self.format_date = str(config['ClockRenderer']['format_date'])
        self.format_hours = str(config['ClockRenderer']['format_hours'])
        ##################################################
        self.deroulementCarrousel = str(config['Running']['scrollOrder'])
        ##################################################
        self.lastMessage = ''
        ##################################################
        self.checkforupdate = int(config['Running']['checkforupdate'])
        self.attract_mode = str(config['Running']['attract_mode'])
        self.defaultConfig = int(config['Running']['default'])
        self.respondToRaspydarts = int(config['Running']['resptoraspydarts'])
        self.raspydartsAdr = str(config['Running']['raspydarts'])
        self.mqttPathResponse = str(config['Running']['raspydartscanal'])
        ##################################################
        self.owmReverse = "http://api.openweathermap.org/geo/1.0/zip?zip={0},{1}&appid={2}"
        self.findLatLonCityname = "http://api.openweathermap.org/geo/1.0/zip?zip={0},{1}&appid={2}"
        self.meteoInTime = "https://api.openweathermap.org/data/2.5/weather?lat={0}&lon={1}&appid={2}&units={3}&lang={4}"
        self.meteoPrevi = "https://api.openweathermap.org/data/2.5/forecast?lat={0}&lon={1}&appid={2}&units={3}&lang={4}"
        self.edfjourstempo_api= "https://www.api-couleur-tempo.fr/api/joursTempo?dateJour%5B%5D={0}&dateJour%5B%5D={1}"
        self.tokenPath = "/Raspy2DMD/token"
        self.callevery = str(config['OpenWeatherMap']['callevery'])
        self.seeduring = str(config['OpenWeatherMap']['seeduring'])
        self.lat = str(config['OpenWeatherMap']['lat'])
        self.lon = str(config['OpenWeatherMap']['lon'])
        self.zipcode = str(config['OpenWeatherMap']['zipcode'])
        self.statecode = str(config['OpenWeatherMap']['statecode'])
        self.countrycode = str(config['OpenWeatherMap']['countrycode'])
        self.appid = str(config['OpenWeatherMap']['appid'])
        self.units = str(config['OpenWeatherMap']['units'])
        self.lang = str(config['OpenWeatherMap']['lang'])
        self.prevision = str(config['OpenWeatherMap']['prevision'])
        self.lastcall = str(config['OpenWeatherMap']['lastcall'])
        ##################################################
        self.volumeSon = str(config['Sound']['volume'])
        self.outputSon = str(config['Sound']['output'])
        ##################################################
        self.HDMIIsActive = str(config['Running']['activehdmi'])
        if(self.HDMIIsActive == "1"):
            self.pictureWidth = 640
            self.pictureHeight = 480
            self.maxFontSize = int(self.pictureHeight * 0.25)

    def GetOptions(self):
        try:            
            ##################################################
            sleep(0.01)
            ##################################################
            config = dMDRendererConfig.GetMatriceConfig()
            ##################################################
            sleep(0.01)
            ##################################################
            self.InitSelfVar(config)
            ##################################################
            sleep(0.01)
            ##################################################
            self.InitExcluded()
            ##################################################
            sleep(0.01)
            ##################################################
            self.fontDateForDateHours = None
            self.fontHoursForDateHours = None
            ##################################################
            heureEnCour = datetime.now()
            heureEnCour = heureEnCour.strftime("%H")
            ##################################################
            ########       Init RGBMatrix ##############
            ##################################################
            options = RGBMatrixOptions()
            options.disable_hardware_pulsing = 1
            options.drop_privileges = 0
            options.pwm_bits = self.pwm_bits
            options.brightness = int(self.brightnesshours[int(heureEnCour)])
            options.scan_mode = self.scan_mode 
            options.cols = self.cols
            options.rows = self.rows
            options.chain_length = self.led_chain
            options.parallel = self.vertical_parallel_chain
            options.gpio_slowdown = self.gpio_slowdown
            options.pwm_lsb_nanoseconds = self.pwm_lsb_nanoseconds
            options.limit_refresh_rate_hz = self.limit_refresh_rate_hz 
            options.hardware_mapping = self.hardware_mapping
            options.led_rgb_sequence = self.rgb_mode
            options.row_address_type  = self.led_row_addr_type
            ##################################################
            sleep(0.01)
            ##################################################
            return options

        except Raspy2DMDException as e:
            print("Unhandled error in GetOptions :\n{}".format(e))
            raise
    
    ##############################################
    #######      Run & Stop thread ###########
    ##############################################
    def RunThreading(self):
        try:
            global _etatCheckIfInternetIsAvailable
            self.StopThreading()
            if _etatCheckIfInternetIsAvailable == False:
                self.threadForWebAvailable = threading.Thread(target=self.ThreadForCheckingWebIsAvailable)
                self.threadForWebAvailable.start()
            if(self.standalone == 0):
                self.threadForAttractMode = threading.Thread(target=self.ThreadForAttractMode)
                if(int(self.attract_mode) > 0):
                    self._dateTimeAct = datetime.now() + timedelta(seconds=int(self.attract_mode))
                self.threadForAttractMode.start()
        except Exception as e:
            raise Raspy2DMDException("Unhandled error in RunThreading :\n{}".format(e))

    def StopThreading(self):
        try:
            global _attractModeRunning
            global _etatCheckIfInternetIsAvailable
            if self.threadForWebAvailable.isAlive():
                self.threadForWebAvailable.join()
                _etatCheckIfInternetIsAvailable = False
            if self.threadForAttractMode.isAlive():
                self.threadForAttractMode.join()
                _attractModeRunning = False
        except Exception as e:
            raise Raspy2DMDException("Unhandled error in StopThreading :\n{}".format(e))
        
    ##############################################
    #######   Run & Stop thread waiter ##########
    ##############################################
    def Stop(self, msg):
        global _stopAffichage
        global _stopHDMIRequested
        global _dmdAvailable
        _stopHDMIRequested = True
        _stopAffichage = True
        self.stopAffichage = True
        self.StopSound()
        if(self.HDMIIsActive == '1'):
            self.StopHDMIPlayer()
        
        sleep(0.15)

        self.IncomingMessage(msg)
        _dmdAvailable = True
                    
    def StopHDMIPlayer(self, refreshOnly=False):        
        if(refreshOnly != True):
            global _stopHDMIRequested
            _stopHDMIRequested = True
        subprocess.run(['sudo', 'sh', '/Raspy2DMD/HDMI/StopHDMIRendering.sh'])

    def IncomingMessage(self, msg):
        try:
            global _stopAffichage
            global _stopHDMIRequested
            global _attractModeRunning

            self.lastMessage = msg
            self._attractModeRunning = False
            self.stopAffichage = False
            _attractModeRunning = False  
            _stopAffichage = False
            _stopHDMIRequested = False
            if(int(self.attract_mode) > 0):
                self._dateTimeAct = datetime.now() + timedelta(seconds=int(self.attract_mode))

        except Exception as e:
            raise Raspy2DMDException("Unhandled error in IncomingMessage :\n{}".format(e))
        
    ##############################################
    ######   Checking if web is available #######
    ##############################################
    def ThreadForCheckingWebIsAvailable(self):
        try:
            _etatCheckIfInternetIsAvailable = True
            while(1 == 1):
                connected = self.is_connected()
                if(connected == True):
                    if(int(self.checkforupdate) == 1):
                        self.searchUpdate()
                    sleep(3600)
                sleep(30)
        except Exception as e:
            raise Raspy2DMDException("Unhandled error in ThreadForCheckingWebIsAvailable :\n{}".format(e))

    def is_connected(hostname):
        endpoint = "https://www.google.com/"
        try:
            response = requests.get(endpoint)
            if(response.status_code == 200):                
                return True
        except:
            pass
        return False   
        
    ##############################################
    #####   Checking if update is available #####
    ##############################################
    def searchUpdate(self):
        global _etatCheckIfInternetIsAvailable
        version = ''
        with open("/var/www/html/WebBrowser/VERSION", "r") as fichier:
            version = fichier.read()
        
        if(isfile('/Medias/beta')):
            print(':> Mode Beta')
            endpoint = "http://rde.freeboxos.fr:16843/updatesTest/" + version
        else:
            endpoint = "http://rde.freeboxos.fr:16843/updates/" + version
        try:
            response = requests.get(endpoint)
            if(response.status_code == 200):
                _etatCheckIfInternetIsAvailable = True
                response = response.json()
                for msg in response:
                    self.updateAvailable = True
                return
        except Exception as e:
            pass

    ##############################################
    #######     Gestion Attract Mode ##########
    ##############################################
    def ThreadForAttractMode(self):
        try:
            global _attractModeRunning
            global _dmdAvailable
            global _alreadyRunning
            _alreadyRunning = False
            #self._stopevent = threading.Event()
            while(True):
                if(int(self.attract_mode) > 0) :
                    if(datetime.now() > self._dateTimeAct and _attractModeRunning == False and _dmdAvailable == True and _alreadyRunning == False):
                        _attractModeRunning = True
                        self.waiter = threading.Thread(target=self.run)
                        self.RenderWaiter("start")
                if(_dmdAvailable == False):
                    sleep(5)
                else:
                    sleep(0.3)
        except Exception as e:
            raise Raspy2DMDException("Unhandled error in ThreadForAttractMode :\n{}".format(e))
            
    ##############################################
    #######         Apply is ok ##########
    ##############################################
    def WarnIsApply(self):
        try:
            if(self.HDMIIsActive == '0'):
                self.RenderImage('/Medias/RaspyDartsDMD/param_img/DMD/Parametrage_fixed.png', wait=3)
                self.RenderImage('WELK.OME')
            else:
                self.RenderImage('/Medias/RaspyDartsDMD/param_img/HDMI/Parametrage_fixed.png', wait=3)
                self.RenderImage('WELK.OME')
        except Exception as e:
            raise Raspy2DMDException("Unhandled error in WarnIsApply :\n{}".format(e))

    ##############################################
    #######         RGB Test ##########
    ##############################################
    def ShowRGBTest(self):
        try:
            if(self.HDMIIsActive == '0'):
                self.RenderImage('/Medias/RaspyDartsDMD/warn/RGBTest/DMD/rgb_test.png', wait=86400)
                self.RenderImage('WELK.OME')
            else:
                self.RenderImage('/Medias/RaspyDartsDMD/warn/RGBTest/HDMI/rgb_test.png', wait=86400)
                self.RenderImage('WELK.OME')
        except Exception as e:
            raise Raspy2DMDException("Unhandled error in ShowRGBTest :\n{}".format(e))

    ##############################################
    #######    Set new configuration ###########
    ##############################################
    def ApplyConfig(self, params):
        try:
            dMDRendererConfig.ApplyConfig(params)
            sleep(0.5)
            if('standalone' in params[0].split(':')):
                import subprocess
                subprocess.run(['sudo', 'reboot'])
        except Exception as e:
            raise Raspy2DMDException("Unhandled error in ApplyConfig :\n{}".format(e))

    ##############################################
    #######       Send to Raspydarts #####
    #######  Pour avoir un suivi temps reel #####
    ##############################################
    def SendToRaspydarts(self, text):
        try:
            if(self.respondToRaspydarts == 1):
                raspydarts = mqtt.Client()
                if(raspydarts.connect(self.raspydartsAdr, 1883, 60) == 0):
                    raspydarts.publish(self.mqttPathResponse, text + ' ' + str(datetime.now().strftime(str(self.format_hours))))
                    raspydarts.disconnect()
            else:
                print(text) 
        except Exception as e:
            raise Raspy2DMDException("Unhandled error in SendToRaspydarts :\n{}".format(e))
      
    ##############################################
    #######    Send config to raspydarts #######
    ##############################################
    def SendConfigToRaspydarts(self):
        try:
            if(self.respondToRaspydarts == 1):
                raspydarts = mqtt.Client()
                config = dMDRendererConfig.GetMatriceConfig()
                if(raspydarts.connect(self.raspydartsAdr, 1883, 60) == 0):
                    for conf in config:       
                        for val in config[conf]:     
                            raspydarts.publish(self.mqttPathResponse, conf + ':' + val + ':' + str(config[conf][val]))
                    raspydarts.disconnect()
            else:
                print(text)
        except Exception as e:
            raise Raspy2DMDException("Unhandled error in SendConfigToRaspydarts :\n{}".format(e))

    ##############################################
    #######    RENDER FIRST START ##############
    ##############################################
    def RenderFirstStart(self):
        
        global _IP_Available
        try:
            sleep(0.1)

            self.clear()
            
            if exists("/boot/rgb"):
                self.ShowRGBTest()
            
            print(":> Pas depuis la SD")
            if(self.HDMIIsActive == '0'):
                pathGifVid = '/Medias/RaspyDartsDMD/gifs_videos/DMD'
                pathImages = '/Medias/RaspyDartsDMD/images/DMD'
            else:
                pathGifVid = '/Medias/RaspyDartsDMD/gifs_videos/HDMI'
                pathImages = '/Medias/RaspyDartsDMD/images/HDMI'

            if(self.defaultConfig != 0):
                if(self.TestConnexion() == False):
                    print(":> Impossible de recuperer l'adresse IP")
                    self.RenderText("Impossible de recuperer l'adresse IP - Unable to retrieve IP address", sens="left", iterate=2)
                else:
                    self.RenderText("Web ok via",wait=1)
                    if(self.HDMIIsActive == '0'):
                        self.RenderText("http://raspy2dmd.local", sens='left')
                    else:
                        self.RenderText("http://raspy2dmd.local", sens='left',wait=3)
            else:
                self.TestConnexion()
                    
            # On cherche une possible video/gif
            imagesDisponibles = [f for f in listdir(pathGifVid) if isfile(join(pathGifVid, f))]
            if(len(imagesDisponibles) != 0):
                imgInd = random.randint(0, len(imagesDisponibles) - 1)
                imageWanted = pathGifVid + '/' + imagesDisponibles[imgInd] 
                if(imageWanted.find('.gif') != -1):
                    self.RenderGif(imageWanted, cheminPrevu=True)
                elif(imageWanted.find('.mp4') != -1):
                    self.RenderVideo(imageWanted, cheminPrevu=True)
            self.RenderImage(pathImages + '/Raspy2DMD.png')

            if(self.attract_mode != 0) :
                self._dateTimeAct = datetime.now() + timedelta(seconds=int(self.attract_mode))

            print(":> Configuration disponible")

        except Exception as e:
            raise Raspy2DMDException("Unhandled error during Raspy2DMD starting :\n{}".format(e))

    ##############################################
    #######     Test IP adresss ##############
    ##############################################
    def TestConnexion(self):

        global _IP_Available
        try:
            testIP = "8.8.8.8"
            s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            s.connect((testIP, 0))
            ipaddr = s.getsockname()[0]                    
            _IP_Available = True
            return True
                
        except socket.error:
            
            _IP_Available = False
            return False

        except Exception as e:
            raise Raspy2DMDException("Unhandled error during Raspy2DMD starting :\n{}".format(e))
            return False
    
    ##############################################
    ####Render Image or gif for no internet#######
    ##############################################
    def RenderNoWhat(self, noInternet=False):
        try:
            if(self.HDMIIsActive == '0'):
                if(noInternet == True):
                    pathGifImg = '/Medias/RaspyDartsDMD/warn/NoInternet/DMD'
                else:
                    pathGifImg = '/Medias/RaspyDartsDMD/warn/NoIP/DMD'
            else:
                if(noInternet == True):
                    pathGifImg = '/Medias/RaspyDartsDMD/warn/NoInternet/HDMI'
                else:
                    pathGifImg = '/Medias/RaspyDartsDMD/warn/NoIP/HDMI'
                                    
            # On cherche une possible video/gif
            imagesGifsDisponibles = [f for f in listdir(pathGifImg) if isfile(join(pathGifImg, f))]
            if(len(imagesGifsDisponibles) != 0):
                imgInd = random.randint(0, len(imagesGifsDisponibles) - 1)
                imageGifWanted = pathGifImg + '/' + imagesGifsDisponibles[imgInd] 
                if(imageGifWanted.find('.gif') != -1):
                    self.RenderGif(imageGifWanted, cheminPrevu=True)
                elif(imageGifWanted.find('.png') != -1):
                    self.RenderImage(imageGifWanted, wait=5)

        except Exception as e:
            raise Raspy2DMDException("Unhandled error during Raspy2DMD starting :\n{}".format(e))

    ##############################################
    #######    RENDER STANDALONE ##############
    ##############################################
    def RenderStandalone(self):
        try:
            sleep(0.1)        
            if(self.standalone == 1):
                self.RenderWaiter("start")
            self.RunThreading()
        except Exception as e:
            raise Raspy2DMDException("Unhandled error in RenderStandalone :\n{}".format(e))

    ###############################################
    #######                          ##############
    ###############################################
    def RenderWaiter(self, startOrStop=""):
        try:
            if(startOrStop == "start"):
                self.RenderWaiterStart()
            elif(startOrStop == "pause"):
                self.RenderWaiterPause()
            elif(startOrStop == "resume"):
                self.RenderWaiterResume()
            else:
                self.RenderWaiterStop() 
        except Exception as e:
            raise Raspy2DMDException("Unhandled error in RenderWaiter :\n{}".format(e))

    def RenderWaiterStart(self):
        try:
            self.waiter.start()   
        except Exception as e:
            print("Unhandled error in RenderWaiterStart :\n{}".format(e))
            raise Raspy2DMDException("Unhandled error in RenderWaiterStart :\n{}".format(e)) 
        
    def RenderWaiterStop(self):
        try:
            self.waiter.join()             
        except Exception as e:
            raise Raspy2DMDException("Unhandled error in RenderWaiterStop :\n{}".format(e)) 
        
    def RenderWaiterPause(self):
        try:
            self.pauseWaiterWanted = time() + 6
            if self._pauseevent == False:
                self._pauseevent = True
                self.pauseevent_cond.acquire() 
        except Exception as e:
            raise Raspy2DMDException("Unhandled error in RenderWaiterPause :\n{}".format(e))

    def RenderWaiterResume(self):
        try:
            self._pauseevent = False
            self.pauseevent_cond.notify()
            self.pauseevent_cond.release()
        except Exception as e:
            raise Raspy2DMDException("Unhandled error in RenderWaiterResume :\n{}".format(e))

    def run(self):   
        try:
            accesInternet = False
            global _IP_Available
            global _stopHDMIRequested
            global _stopAffichage
            global _HeureEnCoursLuminosite
            global _PrevHeureEnCoursLuminosite
            global _alreadyRunning
            _alreadyRunning = False
            _PrevHeureEnCoursLuminosite = None

            while (self.stopAffichage == False and _stopAffichage == False and _stopHDMIRequested == False):

                _alreadyRunning = True
                alreadyShowNoInternetAvailable = False
                for show in self.deroulementCarrousel.split(','): 

                    if(show in ('1','2','3','4','T','M','P','S','E')):

                        _HeureEnCoursLuminosite = datetime.now()
                        _HeureEnCoursLuminosite = _HeureEnCoursLuminosite.strftime("%-H")

                        self.TestConnexion()
                        if(show == 'M' or show == 'P' or show == 'E'):
                            accesInternet = self.is_connected()

                        if (_PrevHeureEnCoursLuminosite == None):
                            _PrevHeureEnCoursLuminosite = _HeureEnCoursLuminosite
                            self.init()

                        if(_PrevHeureEnCoursLuminosite != _HeureEnCoursLuminosite):
                            _PrevHeureEnCoursLuminosite = _HeureEnCoursLuminosite
                            self.init()

                        if(_IP_Available == True):
                            if(self.updateAvailable == True and _stopHDMIRequested == False and _stopAffichage == False):
                                if(self.HDMIIsActive == '0'):
                                    self._gifForWarnUpdate = '/Medias/RaspyDartsDMD/update_gif/DMD/update.gif'
                                else:
                                    self._gifForWarnUpdate = '/Medias/RaspyDartsDMD/update_gif/HDMI/update.gif'
                                if(self.RenderGif(self._gifForWarnUpdate, cheminPrevu=True) == False):
                                    _stopHDMIRequested = True
                                    _stopAffichage = True
                                    _alreadyRunning = False
                                    break
                        elif(_IP_Available == False):
                            if(alreadyShowNoInternetAvailable == False and _stopHDMIRequested == False and _stopAffichage == False):
                                alreadyShowNoInternetAvailable = True
                                if(self.RenderNoWhat(noInternet=False) == False):
                                    _stopHDMIRequested = True
                                    _stopAffichage = True
                                    _alreadyRunning = False
                                    break

                        if(show == '1' and self.VerifGifDispo() == True and _stopHDMIRequested == False and _stopAffichage == False):
                            if(self.RenderGif(aleatoire=True) == False):
                                #print(":> On sort des gifs !")
                                _stopHDMIRequested = True
                                _stopAffichage = True
                                _alreadyRunning = False
                                break

                        elif(show == '2' and self.VerifImageDispo() == True and _stopHDMIRequested == False and _stopAffichage == False):
                            if(self.RenderImage(aleatoire=True, wait=4) == False):
                                #print(":> On sort des images !")
                                _stopHDMIRequested = True
                                _stopAffichage = True
                                _alreadyRunning = False
                                break

                        elif(show == '4' and self.VerifCarrouselTextDispo() == True and _stopHDMIRequested == False and _stopAffichage == False):
                            if(self.RenderCarrousel() == False):
                                #print(":> On sort du carrousel de textes !")
                                _stopHDMIRequested = True
                                _stopAffichage = True
                                _alreadyRunning = False
                                break

                        elif(show == 'M' and _stopHDMIRequested == False and _stopAffichage == False):
                            if(accesInternet == False):
                                if(self.RenderNoWhat(noInternet=True) == False):
                                    _stopHDMIRequested = True
                                    _stopAffichage = True
                                    _alreadyRunning = False
                                    break
                            else:
                                if(self.RenderMeteoInTime() == False):
                                    #print(":> On sort de la meteo temps reel !")
                                    _stopHDMIRequested = True
                                    _stopAffichage = True
                                    _alreadyRunning = False
                                    break

                        elif(show == 'P' and _stopHDMIRequested == False and _stopAffichage == False):
                            if(accesInternet == False):
                                if(self.RenderNoWhat(noInternet=True) == False):
                                    _stopHDMIRequested = True
                                    _stopAffichage = True
                                    _alreadyRunning = False
                                    break
                            else:
                                if(self.RenderMeteoPrevisionnelle() == False):
                                    #print(":> On sort de la meteo previsionnelle !")
                                    _stopHDMIRequested = True
                                    _stopAffichage = True
                                    _alreadyRunning = False
                                    break

                        elif(show == 'E' and _stopHDMIRequested == False and _stopAffichage == False):
                            if(accesInternet == False):
                                if(self.RenderNoWhat(noInternet=True) == False):
                                    _stopHDMIRequested = True
                                    _stopAffichage = True
                                    _alreadyRunning = False
                                    break
                            else:
                                if(self.RenderEDFJoursTempo() == False):
                                    #print(":> On affiche les prévision d'EDF !")
                                    _stopHDMIRequested = True
                                    _stopAffichage = True
                                    _alreadyRunning = False
                                    break

                        elif(show == 'T' and _stopHDMIRequested == False and _stopAffichage == False):
                            if(self.RunTime() == False):
                                #print(":> On sort de la date et heure !")
                                _stopHDMIRequested = True
                                _stopAffichage = True
                                _alreadyRunning = False
                                break

                        elif(show == 'S' and _stopHDMIRequested == False and _stopAffichage == False):
                            if(self.RenderSOC() == False):
                                #print(":> On sort du suivi SOC !")
                                _stopHDMIRequested = True
                                _stopAffichage = True
                                _alreadyRunning = False
                                break

            #print(":> On sort de la boucle de l'attract mode")
            self.clear()
            
        except Exception as e:
            raise Raspy2DMDException("Unhandled error in run :\n{}".format(e))
            
    ###############################################
    #######        RENDER METEO ##############
    ###############################################
    def ZipPostCodeGeocoding(self):
        try:
            global _etatCheckIfInternetIsAvailable
            if _etatCheckIfInternetIsAvailable == True:
                if(self.appid == '0'):
                    return
                apiPath = str(self.owmReverse).format(self.zipcode,self.countrycode, self.appid)
                req = requests.get(str(apiPath))
                #self._stopevent.wait(0.1)
                sleep(0.1)
                if req.status_code == 200:
                    data = request.json()
                    self.zipcode = data['zip']
                    self.cityname = data['name']
                    self.countrycode = data['country']
                    self.lat = data['lat']
                    self.lon = data['lon']
                else:
                    print("Impossible d'acceder a api.openweathermap.org.\nrequest : {0}\nCode recu: {1}".format(apiPath, req.status_code))
                    self.RenderText("Impossible d'acceder a api.openweathermap.org.", sens='left')
                #self._stopevent.wait(0.5)
                sleep(0.5)
        except Exception as e:
            raise Raspy2DMDException("Unhandled error in ZipPostCodeGeocoding :\n{}".format(e))
        
    def FindLatLonCityname(self):
        try:
            isconnected = self.is_connected()
            if isconnected == True:
                if(self.appid == '0'):
                    return "appid indiqué est vide"

                apiPath = str(self.findLatLonCityname).format(self.zipcode, self.countrycode, self.appid)
                req = requests.get(str(apiPath))
                sleep(0.01)

                if req.status_code == 200:
                    data = req.json()
                    conf = []
                    conf.append("OpenWeatherMap")
                    conf.append("cityname:" + str(data['name']))
                    dMDRendererConfig.ApplyConfig(conf)
                    sleep(0.01)
                    sleep(0.01)
                    conf = []
                    conf.append("OpenWeatherMap")
                    conf.append("lat:" + str(data['lat']))
                    dMDRendererConfig.ApplyConfig(conf)
                    sleep(0.01)
                    conf = []
                    conf.append("OpenWeatherMap")
                    conf.append("lon:" + str(data['lon']))
                    dMDRendererConfig.ApplyConfig(conf)
                    sleep(0.01)
                    return "Coordonnées trouvées via OpenWeatherMap"
                else:
                    raise Raspy2DMDException("Impossible d'acceder a api.openweathermap.org.\nrequest : {0}\nCode recu: {1}".format(apiPath, req.status_code))
        except Exception as e:
            raise Raspy2DMDException("Unhandled error in FindLatLonCityname :\n{}".format(e))
            
    ###############################################
    #######     RENDER SOC ANALYSIS    ############
    ###############################################
    def RenderSOC(self, fontVoulue=None):
        try:
            global _dmdAvailable
            _dmdAvailable = False 

            i = 0
            j = 0

            if(self.HDMIIsActive == '0'):
                self.SetImage(Image.open(self.perfvisualizer + "/DMD/waitLoadingSOC.png").convert("RGB"))
            else:                
                self.SetImageOnHDMI(Image.open(self.perfvisualizer + "/HDMI/waitLoadingSOC.png").convert("RGB"), refresh=True)

            while((i < (5000)) and self.stopAffichage == False and _stopAffichage == False and _stopHDMIRequested == False):

                i = i + j

                voltage = "{0:.2f}V"
                temperature = "{0:.0f}°C"
                hexaMessage = "{0}"
                heartMessage = "{0:.2f}"
                ipIsAvailable = self.TestConnexion()
                internetAvailable = self.is_connected()
                socTemperature = dMDRendererSOC.get_temp()
                coreVoltage = dMDRendererSOC.get_volt("core")
                sdramCVoltage = dMDRendererSOC.get_volt("sdram_c")
                sdramIVoltage = dMDRendererSOC.get_volt("sdram_i")
                sdramPVoltage = dMDRendererSOC.get_volt("sdram_p")
                voltageTotal = coreVoltage + sdramCVoltage + sdramIVoltage + sdramPVoltage
                underVoltageHexaForm = dMDRendererSOC.get_undervoltage_hexaform()
                GHzValue = dMDRendererSOC.get_clock('arm')

                if(self.HDMIIsActive == '1'):
                    fontSize = 70
                    perfvisualizerPath = self.perfvisualizer + '/HDMI/'
                    width = 640
                    height = 480
                    pictureText = 210
                    pictureInfoHeight = 120
                else:
                    fontSize = self.maxFontSize
                    perfvisualizerPath = self.perfvisualizer + '/DMD/'
                    width = 128
                    height = 32
                    pictureTextWidth = 42
                    pictureThrottledWidth = 92
                    pictureInfoHeight = 16
                    pictureAvailable = 32
                    pictureThrottledheight = 28
                    pictureHeartheight = 22

                if(fontVoulue == None):
                    fontPath = self.fontsTTFDirectory + 'Quicksand-Bold.ttf'
                else:
                    fontPath = self.fontsTTFDirectory + fontVoulue

                temperature = temperature.format(float(socTemperature))
                imTemperatureValue = self.CreateImageCenterTextAndSetTransparency(temperature, fontPath, fontSize, self.defaultfontcolor.split(','), 
                                                                                pictureWidth=pictureTextWidth, pictureHeight=height)    
                voltage = voltage.format(float(voltageTotal))
                imVoltageValue = self.CreateImageCenterTextAndSetTransparency(voltage, fontPath, fontSize, self.defaultfontcolor.split(','), 
                                                                                pictureWidth=pictureTextWidth, pictureHeight=height)
            

                heartMessage = heartMessage.format(float(GHzValue))
                imHeartMessageValue = self.CreateImageCenterTextAndSetTransparency(heartMessage, fontPath, fontSize, self.defaultfontcolor.split(','), 
                                                                                pictureWidth=pictureTextWidth, pictureHeight=pictureHeartheight)

                if(ipIsAvailable == True):
                    imStatIPAvailable = Image.open(str(perfvisualizerPath + "ipAvailable.png")).convert("RGBA")
                else:
                    imStatIPAvailable = Image.open(str(perfvisualizerPath + "ipNotAvailable.png")).convert("RGBA")

                if(internetAvailable == True):
                    imStatWEBAvailable = Image.open(str(perfvisualizerPath + "webAvailable.png")).convert("RGBA")
                else:
                    imStatWEBAvailable = Image.open(str(perfvisualizerPath + "webNotAvailable.png")).convert("RGBA")
                
                imProcTemp = Image.open(str(perfvisualizerPath + "tempcpu.png")).convert("RGBA")
                imVolt = Image.open(str(perfvisualizerPath + "volt.png")).convert("RGBA")
                imHeartBeat = Image.open(str(perfvisualizerPath + "heartbeat.png")).convert("RGBA")
                imProcTemp = imProcTemp.resize((pictureInfoHeight, pictureInfoHeight), Image.BICUBIC)
                imVolt = imVolt.resize((pictureInfoHeight, pictureInfoHeight), Image.BICUBIC)
                imHeartBeat = imHeartBeat.resize((pictureInfoHeight, pictureInfoHeight), Image.BICUBIC)
                imStatIPAvailableValue = imStatIPAvailable.resize((pictureInfoHeight, pictureInfoHeight), Image.BICUBIC)
                imStatWEBAvailableValue = imStatWEBAvailable.resize((pictureInfoHeight, pictureInfoHeight), Image.BICUBIC)
                                    
                if(self.HDMIIsActive == '1'):
                    img = Image.new('RGB', (width, height), color = (0, 0, 0))
                    img.paste(imWeather, (0, 120), imWeather)
                    img.paste(imTemperature, (225, 0), imTemperature)
                    img.paste(imWindDeg, (480, 140), imWindDeg)
                    img.paste(imVitesseVent, (426, 180), imVitesseVent)
                else:
                    img = Image.new('RGB', (width, height), color = (0, 0, 0))
                    img.paste(imProcTemp, (2, 0), imProcTemp)
                    img.paste(imVolt, (2, 16), imVolt)
                    img.paste(imTemperatureValue, (20, -7), imTemperatureValue)
                    img.paste(imVoltageValue, (20, 7), imVoltageValue)
                    img.paste(imStatIPAvailableValue, (74, 0), imStatIPAvailableValue)
                    img.paste(imStatWEBAvailableValue, (100, 0), imStatWEBAvailableValue)
                    img.paste(imHeartBeat, (66, 16), imHeartBeat)
                    img.paste(imHeartMessageValue, (84, 13), imHeartMessageValue)
            
                if(self.HDMIIsActive == '0'):
                    self.SetImage(img, ((self.pictureWidth / 2) - (img.width / 2)))
                else:
                    self.SetImageOnHDMI(img)

                if (self.stopAffichage == True or _stopAffichage == True or _stopHDMIRequested == True) :
                    self.clear()
                    _dmdAvailable = True
                    return False

                j = 0

                while((j < 1500) and self.stopAffichage == False and _stopAffichage == False and _stopHDMIRequested == False):
                        
                    j = j + 1

                    if (self.stopAffichage == True or _stopAffichage == True or _stopHDMIRequested == True) :
                        self.clear()
                        _dmdAvailable = True
                        return False

                    sleep(0.001)

                if(underVoltageHexaForm != "0x0"):

                    imThrottled = Image.open(str(perfvisualizerPath + "hexa.png")).convert("RGBA")
                    imThrottled = imThrottled.resize((pictureAvailable, pictureAvailable), Image.BICUBIC)
                    imThrottledValue = self.CreateImageCenterTextAndSetTransparency(underVoltageHexaForm.replace('0x',''), fontPath, fontSize, 
                                                                                    self.defaultfontcolor.split(','), 
                                                                                    pictureWidth=pictureThrottledWidth, pictureHeight=pictureAvailable)

                    img = Image.new('RGB', (width, height), color = (0, 0, 0))
                    img.paste(imThrottled, (2, 0), imThrottled)
                    img.paste(imThrottledValue, (34, 0), imThrottledValue)

                    if(self.HDMIIsActive == '0'):
                        self.SetImage(img, ((self.pictureWidth / 2) - (img.width / 2)))
                    else:
                        self.SetImageOnHDMI(img)

                    i = i + j
                    j = 0
                    while((j < 1500) and self.stopAffichage == False and _stopAffichage == False and _stopHDMIRequested == False):
                        
                        j = j + 1

                        if (self.stopAffichage == True or _stopAffichage == True or _stopHDMIRequested == True) :
                            self.clear()
                            _dmdAvailable = True
                            return False

                        sleep(0.001)

                if (self.stopAffichage == True or _stopAffichage == True or _stopHDMIRequested == True) :
                    self.clear()
                    _dmdAvailable = True
                    return False
                
            self.clear()
            _dmdAvailable = True 
            return True

        except Exception as e:
            raise Raspy2DMDException("Unhandled error in RenderMeteoInTime :\n{}".format(e))

    ###############################################
    #######     RENDER METEO IN TIME   ############
    ###############################################
    def RenderMeteoInTime(self, fontVoulue=None):
        try:
            global _dmdAvailable
            _dmdAvailable = False
            isconnected = self.is_connected()

            if isconnected == True:

                if(self.appid == '0'):
                    self.RenderText("Impossible de recuperer la meteo, appid OpenWeatherMap manquant", sens='left')
                    _dmdAvailable = True
                    return True
                
                dateNow = datetime.now()
                lastCall = None
                lastToken = None
                titreMeteo = "{0}"
                temperature = "{0}°C"
                vitesseVent = "{0}km/h"
                
                if(self.HDMIIsActive == '1'):
                    fontSize = 70
                    meteoPath = self.meteo + '/HDMI/'
                    width = 640
                    height = 480
                    pictureFractWidth = 210
                    pictureWheather = 240
                    pictureWind = 120
                else:
                    fontSize = self.maxFontSize
                    meteoPath = self.meteo + '/DMD/'
                    width = 128
                    height = 32
                    pictureFractWidth = 42
                    pictureWheather = 32
                    pictureWind = 16


                if(fontVoulue == None):
                    fontPath = self.fontsTTFDirectory + 'Quicksand-Bold.ttf'
                else:
                    fontPath = self.fontsTTFDirectory + fontVoulue

                for files in listdir(join(self.tokenPath, 'meteo')):
                    if files.endswith('.owmtk'):
                        lastToken = str(files)
                        lastCall = datetime.fromtimestamp(int(files.replace('.owmtk','')))
                        tokenOWMFile = "{0}/{1}".format(join(self.tokenPath, 'meteo'),files)
                        break
                    else:
                        continue
                
                if lastCall == None:
                    lastToken = None
                    lastCall = datetime.fromtimestamp(int(0))

                if dateNow > lastCall:  

                    if(self.HDMIIsActive == '0'):
                        self.SetImage(Image.open(self.meteo + "/DMD/waitLoading.png").convert("RGB"))
                    else:                
                        self.SetImageOnHDMI(Image.open(self.meteo + "/HDMI/waitLoading.png").convert("RGB"), refresh=True)                  
                    
                    if lastToken != None:
                        if exists("{0}/{1}".format(join(self.tokenPath, 'meteo'),lastToken)):
                            remove("{0}/{1}".format(join(self.tokenPath, 'meteo'),lastToken))

                    lastCall = int(round(datetime.timestamp(dateNow + timedelta(minutes=(int(self.callevery))))))

                    tokenOWMFile = "{0}/{1}.owmtk".format(join(self.tokenPath, 'meteo'), lastCall)

                    open(tokenOWMFile, 'a').close()
                    
                    apiPath = str(self.meteoInTime).format(self.lat,self.lon, self.appid, self.units, self.lang)
                    req = requests.get(str(apiPath))
                
                    if req.status_code == 200:
                        
                        with open(tokenOWMFile, 'w') as fichier:
                            json.dump(req.json(), fichier)
                        sleep(0.01)

                    else:
                        print("Impossible d'acceder a api.openweathermap.org.\nrequest : {0}\nCode recu: {1}".format(apiPath, req.status_code))
                        self.RenderText("Impossible d'acceder a api.openweathermap.org.", sens='left')
                        _dmdAvailable = True
                        return True

                    if (self.stopAffichage == True or _stopAffichage == True or _stopHDMIRequested == True) :
                        self.clear()
                        _dmdAvailable = True
                        return False
                    
                if exists(tokenOWMFile):

                    with open(tokenOWMFile) as meteoweather_file: 
                        
                        meteoweather = json.load(meteoweather_file)  
                        
                        temperature = temperature.format(str(round(float(str(meteoweather['main']['temp'])))))
                        imTemperature = self.CreateImageCenterTextAndSetTransparency(temperature, fontPath, fontSize, self.defaultfontcolor.split(','), 
                                                                                     pictureWidth=pictureFractWidth, pictureHeight=height)
                                
                        imWeather = Image.open(str(meteoPath + "{0}.png".format(meteoweather['weather'][0]['icon']))).convert("RGBA")
                        imWeather = imWeather.resize((pictureWheather, pictureWheather), Image.BICUBIC)

                        imWindDeg = Image.open(str(meteoPath + "direction.png")).convert("RGBA")
                        imWindDeg = imWindDeg.resize((pictureWind, pictureWind), Image.BICUBIC)
                        imWindDeg = imWindDeg.rotate(int(round(float(str(meteoweather['wind']['deg'])))))

                        vitesseVent = vitesseVent.format(str(round(float(str(meteoweather['wind']['speed'])))))
                        imVitesseVent = self.CreateImageCenterTextAndSetTransparency(vitesseVent, fontPath, fontSize, self.defaultfontcolor.split(','), 
                                                                                     pictureWidth=pictureFractWidth, pictureHeight=int(height / 2))
                        
                        if(self.HDMIIsActive == '1'):
                            img = Image.new('RGB', (width, height), color = (0, 0, 0))
                            img.paste(imWeather, (0, 120), imWeather)
                            img.paste(imTemperature, (225, 0), imTemperature)
                            img.paste(imWindDeg, (480, 140), imWindDeg)
                            img.paste(imVitesseVent, (426, 180), imVitesseVent)
                        else:
                            img = Image.new('RGB', (width, height), color = (0, 0, 0))
                            img.paste(imWeather, (3, 0), imWeather)
                            img.paste(imTemperature, (38, 0), imTemperature)
                            img.paste(imWindDeg, (97, 2), imWindDeg)
                            img.paste(imVitesseVent, (84, 16), imVitesseVent)

                        if(self.HDMIIsActive == '0'):
                            self.SetImage(img, ((self.pictureWidth / 2) - (img.width / 2)))
                        else:
                            self.SetImageOnHDMI(img)

                    i = 0
                    sToWait = int(self.seeduring)

                    while((i < (sToWait * 1000)) and self.stopAffichage == False and _stopAffichage == False and _stopHDMIRequested == False):
                        
                        i = i + 1

                        if (self.stopAffichage == True or _stopAffichage == True or _stopHDMIRequested == True) :
                            self.clear()
                            _dmdAvailable = True
                            return False

                        sleep(0.001)

                else:
                    print("Le token '{0}' n'existe pas".format(str(tokenOWMFile)))
                    _dmdAvailable = True
                    return True                                      

            else:
                self.RenderText("Pas connecte au web")
                _dmdAvailable = True
                return True
            
            if (self.stopAffichage == True or _stopAffichage == True or _stopHDMIRequested == True) :
                self.clear()
                _dmdAvailable = True
                return False

            _dmdAvailable = True 
            return True

        except Exception as e:
            raise Raspy2DMDException("Unhandled error in RenderMeteoInTime :\n{}".format(e))
        
    # Create an image with the date now
    def CreateImageDate(self, backgroundPNG=None, fontVoulue=None):
        try:

            if(_stopHDMIRequested == True):
                return False
            
            if(fontVoulue == None):
                fontPath = self.fontsTTFDirectory + self.defaultfont_clock
            else:
                fontPath = self.fontsTTFDirectory + fontVoulue

            if(backgroundPNG == None):
                backImg = Image.open(self.patterns + self.clockBackgroundImage)
            else:
                backImg = Image.open(str(backgroundPNG))

            lastDay = ""
            for files in listdir(self.tokenPath + "/clock/date"):
                if files.endswith('.png'):                    
                    lastDay = str(files.replace('.png',''))
                    break

            if(lastDay == ""):
                lastDay = "2001-01-01"

            now = datetime.now()

            if(lastDay == now.strftime("%Y-%m-%d").strip()):
                try:
                    back_img = Image.open(self.tokenPath + "/clock/date/" + str(lastDay) + ".png")
                except:
                    try:
                        #date image not exist (??)
                        lastDay = self.CreateSaveImageDate(now, str(lastDay), fontPath, backImg)
                        back_img = Image.open(self.tokenPath + "/clock/date/" + str(lastDay) + ".png")
                    except Exception as e:
                        raise Raspy2DMDException("Unhandled error in CreateSaveImageDate :\nUnable to create date image the original not exist.\n{}".format(e))

            else:
                try:
                    lastDay = self.CreateSaveImageDate(now, str(lastDay), fontPath, backImg)
                    back_img = Image.open(self.tokenPath + "/clock/date/" + str(lastDay) + ".png")
                except:
                    try:
                        #second try if first lose
                        lastDay = self.CreateSaveImageDate(now, str(lastDay), fontPath, backImg)
                        back_img = Image.open(self.tokenPath + "/clock/date/" + str(lastDay) + ".png")
                    except Exception as e:
                        raise Raspy2DMDException("Unhandled error in CreateSaveImageDate :\nUnable to create date image on the second try.\n{}".format(e))
            sleep(0.05)

            if(self.HDMIIsActive == '0'):
                self.SetImage(back_img)
            else:
                self.SetImageOnHDMI(back_img, refresh=True)

        except Exception as e:
           raise Raspy2DMDException("Unhandled error in CreateImageDate :\n{}".format(e))

    def CreateSaveImageDate(self, now,  lastDay, fontPath, backImg):
        try:
            if exists("{0}/{1}.png".format(self.tokenPath + "/clock/date/", str(lastDay))):
                remove("{0}/{1}.png".format(self.tokenPath + "/clock/date/", str(lastDay)))
            sleep(0.05)
            lastDay = now.strftime("%Y-%m-%d").strip()
            dt = datetime.strptime(str(datetime.now()), "%Y-%m-%d %H:%M:%S.%f")
            date = dt.strftime(str(self.format_date)).strip()
            back_img = self.CreateImageCenterTextAndSetTransparencyWithSpecificBackground(date, fontPath, int(self.sizeFont_Date), backImg)
            sleep(0.05)
            back_img.save(self.tokenPath + "/clock/date/" + str(lastDay)+ ".png")
            sleep(0.05)
            return lastDay
        except Exception as e:
           raise Raspy2DMDException("Unhandled error in CreateSaveImageDate :\n{}".format(e))

    # Create an image with the hour now
    def CreateImageHour(self, hour, backgroundPNG=None, fontVoulue=None):
        try:

            if(_stopHDMIRequested == True):
                return False
            
            if(fontVoulue == None):
                fontPath = self.fontsTTFDirectory + self.defaultfont_clock
            else:
                fontPath = self.fontsTTFDirectory + fontVoulue

            if(backgroundPNG == None):
                backImg = Image.open(self.patterns + self.clockBackgroundImage)
            else:
                backImg = Image.open(str(backgroundPNG))
            
            lastImageHour = ""
            nameImgHour = str(hour.strftime(str(self.format_hours)).strip())
            for files in listdir(self.tokenPath + "/clock/hours/"):
                if files.endswith('.clck'):                    
                    lastImageHour = str(files.replace('.clck',''))
                    break
           
            if(lastImageHour == nameImgHour):
                return

            if exists("{0}/{1}/{2}.clck".format(self.tokenPath, "/clock/hours/", str(lastImageHour))):
                remove("{0}/{1}/{2}.clck".format(self.tokenPath, "/clock/hours/", str(lastImageHour)))
                
            if(str(self.format_hours).count('p') == 1):
                locale.setlocale(locale.LC_ALL, 'en_GB.utf8')

            dt = datetime.strptime(str(datetime.now()), "%Y-%m-%d %H:%M:%S.%f")
            nameImgHour = dt.strftime(str(self.format_hours)).strip()

            if(str(self.format_hours).count('p') == 1):
                locale.setlocale(locale.LC_ALL, self.locale)


            back_img = self.CreateImageCenterTextAndSetTransparencyWithSpecificBackground(nameImgHour, fontPath, int(self.sizeFont_Hours), backImg)
            sleep(0.01)

            tokenOWMFile = "{0}/{1}.clck".format(self.tokenPath + "/clock/hours/", lastImageHour)
            open(tokenOWMFile, 'a').close()
            
            if(self.HDMIIsActive == '0'):
                self.SetImage(back_img)
            else:                
                self.SetImageOnHDMI(back_img, refresh=True)

            back_img = None

        except Exception as e:
           raise Raspy2DMDException("Unhandled error in CreateImageDate :\n{}".format(e))
       
        
    ###############################################
    #######    RENDER METEO PREVISIONNAL  #########
    ###############################################
    def RenderMeteoPrevisionnelle(self, fontVoulue=None):
        try:
            global _dmdAvailable
            _dmdAvailable = False
            isconnected = self.is_connected()

            if isconnected == True:

                if(self.appid == '0'):
                    self.RenderText("Impossible de recuperer la meteo, appid OpenWeatherMap manquant", sens='left')
                    _dmdAvailable = True
                    return True
                
                dateNow = datetime.now()
                lastCall = None
                lastToken = None
                titreMeteo = "{0}"
                temperature = "{0}°"
                dayPrevi = 0
                if(fontVoulue == None):
                    fontPath = self.fontsTTFDirectory + 'Quicksand-Bold.ttf'
                else:
                    fontPath = self.fontsTTFDirectory + fontVoulue
                                        
                for files in listdir(join(self.tokenPath, 'meteo')):
                    if files.endswith('.owmtkp'):
                        dayPrevi = int(files.split('_')[0])
                        if(int(self.prevision) != int(dayPrevi)):
                            lastToken = "0.owmtkp"
                        else:
                            lastToken = str(files.split('_')[1])
                        lastCall = datetime.fromtimestamp(int(lastToken.replace('.owmtkp','')))
                        tokenOWMFile = "{0}/{1}".format(join(self.tokenPath, 'meteo'),lastToken)
                        break
                    else:
                        continue
                
                if lastCall == None:
                    lastToken = None
                    lastCall = datetime.fromtimestamp(int(0))

                if dateNow > lastCall: 
                    
                    if(self.HDMIIsActive == '0'):
                        self.SetImage(Image.open(self.meteo + "/DMD/waitLoading.png").convert("RGB"))
                    else:                
                        self.SetImageOnHDMI(Image.open(self.meteo + "/HDMI/waitLoading.png").convert("RGB"), refresh=True)
              
                    if lastToken != None:

                        if exists("{0}/{1}".format(self.tokenPath,files)):
                            remove("{0}/{1}".format(self.tokenPath,files))
                        if(exists("{0}/{1}".format(self.tokenPath,"meteo"))):
                            shutil.rmtree("{0}/{1}".format(self.tokenPath,"meteo"))
                        mkdir("{0}/{1}".format(self.tokenPath,"meteo"))

                    lastCall = int(round(datetime.timestamp(dateNow + timedelta(minutes=(int(self.callevery))))))

                    tokenOWMFile = "{0}/{1}_{2}.owmtkp".format(join(self.tokenPath, 'meteo'), self.prevision, lastCall)
                    open(tokenOWMFile, 'a').close()
                    
                    apiPath = str(self.meteoPrevi).format(self.lat,self.lon, self.appid, self.units, self.lang)
                    req = requests.get(str(apiPath))
                
                    if req.status_code == 200:
                        
                        with open(tokenOWMFile, 'w') as fichier:
                            json.dump(req.json(), fichier)

                    else:
                        print("Impossible d'acceder a api.openweathermap.org.\nrequest : {0}\nCode recu: {1}".format(apiPath, req.status_code))
                        self.RenderText("Impossible d'acceder a api.openweathermap.org.", sens='left')
                        _dmdAvailable = True
                        return True

                    if (self.stopAffichage == True or _stopAffichage == True or _stopHDMIRequested == True) :
                        self.clear()
                        _dmdAvailable = True
                        return False
                    
                    if exists(tokenOWMFile):

                        datePrevisions = []
                        datePrevisions.append(dateNow.strftime("%Y-%m-%d"))

                        if(int(self.prevision) > 1):
                            for i in range(1, int(self.prevision)):
                                if (self.stopAffichage == True or _stopAffichage == True or _stopHDMIRequested == True) :
                                    self.clear()
                                    _dmdAvailable = True
                                    return False
                                datePrev = datetime.now() + timedelta(days=(i))
                                datePrevisions.append(datePrev.strftime("%Y-%m-%d"))
                        
                        with open(tokenOWMFile) as meteoweather_file:   
                            
                            meteoweather = json.load(meteoweather_file)  
                            items = meteoweather['list']  
                            weather = dict()
                            prevision = dict()
                            weathers = dict()
                            for datePrevision in datePrevisions:

                                if (self.stopAffichage == True or _stopAffichage == True or _stopHDMIRequested == True) :
                                    self.clear()
                                    _dmdAvailable = True
                                    return False

                                if not datePrevision in weathers.keys():
                                    weathers[str(datePrevision)] = dict()

                                for item in items:

                                    if (self.stopAffichage == True or _stopAffichage == True or _stopHDMIRequested == True) :
                                        self.clear()
                                        _dmdAvailable = True
                                        return False

                                    dat = datetime.strptime(item['dt_txt'],"%Y-%m-%d %H:%M:%S")
                                    itemDatMinuit = dat.strftime("%H:%M")
                                    if(itemDatMinuit == "00:00"):   
                                        dat = dat + timedelta(days=(-1))
                                        itemDat = dat.strftime("%Y-%m-%d") 
                                    else:
                                        itemDat = dat.strftime("%Y-%m-%d")    
                                
                                    if (itemDat != datePrevision):  
                                        weather = dict()

                                    else:     
                                        prevision = dict() 
                                        prevision["weather_icon"] = str("{0}".format(item['weather'][0]['icon']))
                                        prevision["temp"] = str(item['main']['temp'])                                  
                                        weather[str(datetime.strptime(item['dt_txt'],"%Y-%m-%d %H:%M:%S").strftime("%H:%M"))] = prevision
                                    
                                        weathers[str(datePrevision)] = weather

                                    sleep(0.01)
                        
                            images = []
                            widthPrevi = 0
                            imPreviWidth = 0 
                            heure = "{0}"
                            temperature = "{0}°C"
                            imTemperaturePrevi = None
                            imHeurePrevi = None
                            cntImg = 0
                            cnt = 0
                            fontDate = ImageFont.truetype(fontPath, 44)
                           
                            if(self.HDMIIsActive == '1'):
                                fontSize = 60
                                meteoPath = self.meteo + '/HDMI/'
                                width = 640
                                height = 480
                                imagPictPreviHeight = 480
                                imagPictPreviWidth = 640
                                imagTimeSize = 60
                                imagWeatherSize = 160
                                imagTemperatureSize = 70
                            else:
                                fontSize = self.maxFontSize
                                meteoPath = self.meteo + '/DMD/'
                                width = 128
                                height = 32
                                imagPictPreviHeight = 51
                                imagPictPreviWidth = 192
                                imagTimeSize = 10
                                imagWeatherSize = 20
                                imagTemperatureSize = 12


                            for key, values in weathers.items():

                                if (self.stopAffichage == True or _stopAffichage == True or _stopHDMIRequested == True) :
                                    self.clear()
                                    _dmdAvailable = True
                                    return False

                                widthPrevi = 0
                                cntImg = 0
                                img = Image.new('RGB', (self.pictureWidth, self.pictureHeight), color = (0, 0, 0))
                                datePrevi = str(datetime.strptime(key,"%Y-%m-%d").strftime("%d/%m"))
                                imDatePrevi = self.CreateImageCenterTextAndSetTransparency(datePrevi, fontPath, fontSize, '60,60,60'.split(','), 
                                                                                                    pictureHeight=imagPictPreviHeight, pictureWidth=imagPictPreviWidth)
                                if(self.HDMIIsActive == '1'):
                                    img.paste(imDatePrevi, (0, -200), imDatePrevi)
                                else:
                                    img.paste(imDatePrevi, (-33, -10), imDatePrevi)

                                if  len(values) > 4 :
                                    imPreviWidth = int(round(self.pictureWidth / 4))
                                else:
                                    imPreviWidth = int(round(self.pictureWidth / len(values)))

                                for keySon, valueSon in values.items():

                                    if (self.stopAffichage == True or _stopAffichage == True or _stopHDMIRequested == True) :
                                        self.clear()
                                        _dmdAvailable = True
                                        return False

                                    if cntImg == 4 :
                                        img.save("{0}/meteo/{1}_{2}.png".format(self.tokenPath, key, cnt))
                                        sleep(0.1)
                                        images.append(img)
                                        cnt += 1
                                        img = Image.new('RGB', (self.pictureWidth, self.pictureHeight), color = (0, 0, 0))
                                        datePrevi = str(datetime.strptime(key,"%Y-%m-%d").strftime("%d/%m"))
                                        imDatePrevi = self.CreateImageCenterTextAndSetTransparency(datePrevi, fontPath,  fontSize, '60,60,60'.split(','), 
                                                                                                            pictureHeight=imagPictPreviHeight, pictureWidth=imagPictPreviWidth)
                                        img.paste(imDatePrevi, (-33, -10), imDatePrevi)
                                        cntImg = -1
                                        widthPrevi = 0
                                        sleep(0.01)
                                                 
                                    heurePrevi = heure.format(str(keySon))
                                    if(self.HDMIIsActive == '0'):
                                        imHeurePrevi = self.CreateImageCenterTextAndSetTransparencyNOSPACE(heurePrevi, fontPath, fontSize, self.defaultfontcolor.split(','), 
                                                                                                          pictureHeight=imagTimeSize, pictureWidth=int(imPreviWidth))
                                    else:
                                        imHeurePrevi = self.CreateImageCenterTextAndSetTransparency(heurePrevi, fontPath, fontSize, self.defaultfontcolor.split(','), 
                                                                                                          pictureHeight=imagTimeSize, pictureWidth=int(imPreviWidth))

                                    imWeatherPrevi = Image.open(str(meteoPath + "{0}.png".format(valueSon['weather_icon']))).convert("RGBA")
                                    imWeatherPrevi = imWeatherPrevi.resize((imagWeatherSize, imagWeatherSize), Image.BICUBIC)
                                
                                    temperaturePrevi = temperature.format(str(round(float(str(valueSon['temp'])))))
                                    if(self.HDMIIsActive == '0'):
                                        imTemperaturePrevi = self.CreateImageCenterTextAndSetTransparencyNOSPACE(temperaturePrevi, fontPath, fontSize, self.defaultfontcolor.split(','), 
                                                                                                          pictureHeight=imagTemperatureSize, pictureWidth=int(imPreviWidth))
                                    else:
                                        imTemperaturePrevi = self.CreateImageCenterTextAndSetTransparency(temperaturePrevi, fontPath, fontSize, self.defaultfontcolor.split(','), 
                                                                                                        pictureHeight=imagTemperatureSize, pictureWidth=int(imPreviWidth))
                                
                                    if(self.HDMIIsActive == '1'):
                                        if(len(values) < 4):
                                            img.paste(imHeurePrevi, (widthPrevi, 95), imHeurePrevi)
                                            img.paste(imWeatherPrevi, (widthPrevi + 25, 150), imWeatherPrevi)
                                            img.paste(imTemperaturePrevi, (widthPrevi, 315), imTemperaturePrevi)
                                        else:
                                            img.paste(imHeurePrevi, (widthPrevi, 95), imHeurePrevi)
                                            img.paste(imWeatherPrevi, (widthPrevi, 150), imWeatherPrevi)
                                            img.paste(imTemperaturePrevi, (widthPrevi, 315), imTemperaturePrevi)
                                    else:
                                        img.paste(imHeurePrevi, (widthPrevi, -3), imHeurePrevi)
                                        img.paste(imWeatherPrevi, (int(widthPrevi + ((imPreviWidth / 2) - 10)), 5), imWeatherPrevi)
                                        img.paste(imTemperaturePrevi, (widthPrevi, 19), imTemperaturePrevi)

                                    widthPrevi += imPreviWidth
                                    cntImg += 1
                                    sleep(0.01)
                                    
                                img.save("{0}/meteo/{1}_{2}.png".format(self.tokenPath, key, cnt))
                                sleep(0.1)
                                images.append(img)
                                cnt += 1
                else:
                    images = []
                    listOfFile = sorted(filter(lambda x: isfile(join(self.tokenPath + "/meteo", x)), listdir(self.tokenPath + "/meteo")))
                    for files in listOfFile:
                        if files.endswith('.png'):
                            images.append(Image.open(self.tokenPath + str("/meteo/{0}").format(files)))
                        
                for value in images:

                    if (self.stopAffichage == True or _stopAffichage == True or _stopHDMIRequested == True) :
                        self.clear()
                        _dmdAvailable = True
                        return False

                    if(self.HDMIIsActive == '0'):
                        self.SetImage(value, ((self.pictureWidth / 2) - (value.size[0] / 2)))
                    else:
                        self.SetImageOnHDMI(value)
                    
                    i = 0
                    sToWait = int(self.seeduring)

                    while((i < (2000)) and (self.stopAffichage == False and _stopAffichage == False and _stopHDMIRequested == False)):
                        
                        i = i + 1

                        if (self.stopAffichage == True or _stopAffichage == True or _stopHDMIRequested == True) :
                            self.clear()
                            _dmdAvailable = True
                            return False

                        sleep(0.001)                        

            else:
                self.RenderText("Pas connecte au web")
                _dmdAvailable = True
                return True
            
            if (self.stopAffichage == True or _stopAffichage == True or _stopHDMIRequested == True) :
                self.clear()
                _dmdAvailable = True
                return False

            _dmdAvailable = True
            return True
                            
        except Exception as e:
            _dmdAvailable = True
            self.clear()
            raise Raspy2DMDException("Unhandled error in RenderMeteoPrevisionnelle :\n{}".format(e))
            
    ###############################################
    #######     RENDER EDF JOURS TEMPO    #########
    ###############################################
    def RenderEDFJoursTempo(self, fontVoulue=None):
        try:   
            global _dmdAvailable
            _dmdAvailable = False
            isconnected = self.is_connected()

            if isconnected == True:
                
                dateNow = datetime.now()
                lastCall = None
                lastToken = None

                if(self.HDMIIsActive == '1'):
                    edfJTPath = self.edfjourstempo + '/HDMI/'
                    fontSize = 70
                    width = 640
                    height = 480
                    pictureDayEDFTempo = 240
                    pictureColor = 120
                else:
                    edfJTPath = self.edfjourstempo + '/DMD/'
                    fontSize = 10
                    width = 128
                    height = 32
                    pictureDayEDFTempo = 64
                    pictureColor = 32

                if(fontVoulue == None):
                    fontPath = self.fontsTTFDirectory + 'Quicksand-Bold.ttf'
                else:
                    fontPath = self.fontsTTFDirectory + fontVoulue

                for files in listdir(join(self.tokenPath, 'edfjt')):
                    if files.endswith('.edfjttk'):
                        lastToken = str(files)
                        lastCall = datetime.fromtimestamp(int(files.replace('.edfjttk','')))
                        tokenEDFJTFile = "{0}/{1}".format(join(self.tokenPath, 'edfjt'),files)
                        break
                    else:
                        continue

                if lastCall == None:
                    lastToken = None
                    lastCall = datetime.fromtimestamp(0)

                if (dateNow.date() - lastCall.date()).days >= 0:  

                    if(self.HDMIIsActive == '0'):
                        self.SetImage(Image.open(self.edfjourstempo + "/DMD/waitLoading.png").convert("RGB"))
                    else:                
                        self.SetImageOnHDMI(Image.open(self.edfjourstempo + "/HDMI/waitLoading.png").convert("RGB"), refresh=True)                      
                
                    if lastToken != None:
                        if exists("{0}/{1}".format(join(self.tokenPath, 'edfjt'),lastToken)):
                            remove("{0}/{1}".format(join(self.tokenPath, 'edfjt'),lastToken))    
                    
                    dateNow = dateNow.date()
                    lastCall = int(round(datetime.timestamp(datetime.now() + timedelta(days=(1)))))

                    tokenEDFJTFile = "{0}/{1}.edfjttk".format(join(self.tokenPath, 'edfjt'), lastCall)

                    open(tokenEDFJTFile, 'a').close()
                
                    apiPath = str(self.edfjourstempo_api).format(dateNow, (dateNow + timedelta(days=(1))))
                    req = requests.get(str(apiPath))
            
                    if req.status_code == 200:
                    
                        with open(tokenEDFJTFile, 'w') as fichier:
                            json.dump(req.json(), fichier)
                        sleep(0.01)

                    else:
                        print("Impossible d'acceder a api-couleur-tempo.fr.\nrequest : {0}\nCode recu: {1}".format(apiPath, req.status_code))
                        return

                    if (self.stopAffichage == True or _stopAffichage == True or _stopHDMIRequested == True) :
                        self.clear()
                        _dmdAvailable = True
                        return False     
                    
                if exists(tokenEDFJTFile):    

                    with open(tokenEDFJTFile) as edfJoursTempo_file: 
                    
                        edfJoursTempoInfos = json.load(edfJoursTempo_file) 

                        imTitle = self.CreateImageCenterTextAndSetTransparency("EDF Jours Tempo", fontPath, fontSize, self.defaultfontcolor.split(','), 
                                                                                        pictureWidth=width, pictureHeight=height)

                        dateJourToday = datetime.strptime(edfJoursTempoInfos[0]["dateJour"],"%Y-%m-%d")
                        jourToday = dateJourToday.strftime("%d/%m/%y")
                        imJourToday = self.CreateImageCenterTextAndSetTransparency(jourToday, fontPath, fontSize, self.defaultfontcolor.split(','), 
                                                                                        pictureWidth=pictureDayEDFTempo, pictureHeight=height)
                        imCodeJourToday = Image.open(str(edfJTPath + "{0}.png".format(edfJoursTempoInfos[0]["codeJour"]))).convert("RGBA")
                        imCodeJourToday = imCodeJourToday.resize((pictureColor, pictureColor), Image.BICUBIC)
                        
                        dateJourTomorrow = datetime.strptime(edfJoursTempoInfos[1]["dateJour"],"%Y-%m-%d")
                        jourTomorrow = dateJourTomorrow.strftime("%d/%m/%y")
                        imJourTomorrow = self.CreateImageCenterTextAndSetTransparency(jourTomorrow, fontPath, fontSize, self.defaultfontcolor.split(','), 
                                                                                        pictureWidth=pictureDayEDFTempo, pictureHeight=height)
                        imCodeJourTomorrow = Image.open(str(edfJTPath + "{0}.png".format(edfJoursTempoInfos[1]["codeJour"]))).convert("RGBA")
                        imCodeJourTomorrow = imCodeJourTomorrow.resize((pictureColor, pictureColor), Image.BICUBIC)

                        if(self.HDMIIsActive == '1'):
                            img = Image.new('RGB', (width, height), color = (0, 0, 0))
                            img.paste(imCodeJourToday, (225, 0), imCodeJourToday)
                            img.paste(imCodeJourTomorrow, (480, 0), imCodeJourTomorrow)
                            img.paste(imJourToday, (0, 120), imJourToday)
                            img.paste(imJourTomorrow, (480, 120), imJourTomorrow)
                            img.paste(imTitle, (0, -10), imTitle)
                        else:
                            img = Image.new('RGB', (width, height), color = (0, 0, 0))
                            img.paste(imCodeJourToday, (16, 0), imCodeJourToday)
                            img.paste(imCodeJourTomorrow, (80, 0), imCodeJourTomorrow)
                            img.paste(imJourToday, (0, 11), imJourToday)
                            img.paste(imJourTomorrow, (64, 11), imJourTomorrow)
                            img.paste(imTitle, (0, -12), imTitle)

                        if(self.HDMIIsActive == '0'):
                            self.SetImage(img, ((self.pictureWidth / 2) - (img.width / 2)))
                        else:
                            self.SetImageOnHDMI(img)

                    i = 0
                    sToWait = int(self.seeduring)

                    while((i < (sToWait * 1000)) and self.stopAffichage == False and _stopAffichage == False and _stopHDMIRequested == False):
                    
                        i = i + 1

                        if (self.stopAffichage == True or _stopAffichage == True or _stopHDMIRequested == True) :
                            self.clear()
                            _dmdAvailable = True
                            return False

                        sleep(0.001)

                else:
                    print("Le token '{0}' n'existe pas".format(str(tokenEDFJTFile)))
                    _dmdAvailable = True
                    return False

            if (self.stopAffichage == True or _stopAffichage == True or _stopHDMIRequested == True) :
                self.clear()
                _dmdAvailable = True
                return False

            _dmdAvailable = True 
            return True

        except Exception as e:
            _dmdAvailable = True
            self.clear()
            raise Raspy2DMDException("Unhandled error in RenderEDFJoursTempo :\n{}".format(e))
            
    ###############################################
    ############     VALID SEGMENT #############
    ###############################################
    def ScoreReceived(self, messageForScore): 
        messageForScore = str(messageForScore).upper()
        valeurs = messageForScore.split(' - ')
        for val in valeurs:
            if(val not in self.segments):  
                return False
        return True
    ###############################################
    #######        RENDER TEXT ##############
    ###############################################
    def RenderText(self, textWanted, sens=None, iterate=None, val=False, fontVoulue=None, patternVoulue=None, wait=None, fontcolor=None, 
                   fontbackcolor=None, backImagePNG=None, sound=None, cheminPrevu=False):	    
        
        try:
            global _dmdAvailable
            _dmdAvailable = False
            self.clear()

            djpris = False
            boucle = 0
            start = 0
            x = -1
            y = -1
            if(fontVoulue == None):
                fontPath = self.fontsTTFDirectory + self.defaultfont
            else:
                fontPath = self.fontsTTFDirectory + fontVoulue
            
            if(self.HDMIIsActive == '0'):
                fontSize = self.maxFontSize
                degIncr = 1
            else:
                fontSize = int(((self.pictureWidth * 0.032) * (self.pictureHeight * 0.032)) / 3)
                degIncr = 10

            charMax = self.maxCharacter
            width = 0
            line_width = 0
            
            if (fontcolor == None):
                rgb = self.defaultfontcolor.split(',')
            else:
                rgb = fontcolor.split(';')

            if (fontbackcolor == None):
                rgbBackground = self.pictureBackgroundColor.split(',')
            else:
                rgbBackground = fontbackcolor.split(';')
                                

            if(val == True):
                #################################################################################
                ###### Recherche de la derniere valeur obtenu (donc pas X)
                textWanted = str(textWanted).upper()
                valeurs = textWanted.split(' - ')
                valSearch = ''
                for val in valeurs:
                    if(val not in self.segments):
                        return
                    if(val != 'X'):
                        valSearch = val
                #################################################################################
                ###### Recherche d'une possible animation pour le score demande
                scorePatch = self.scoresDirectory + str(valSearch)
                #print("scorePatch : " + str(scorePatch))
                # On cherche un possible gif / video
                gifsvideosDisponibles = [f for f in listdir(scorePatch) if isfile(join(scorePatch, f))]
                #print("len(gifsvideosDisponibles) : " +
                #str(len(gifsvideosDisponibles)))
                if(len(gifsvideosDisponibles) != 0):
                    gifvideoInd = random.randint(0, len(gifsvideosDisponibles) - 1)
                    gifvideoFind = scorePatch + '/' + gifsvideosDisponibles[gifvideoInd] 
                    #print("gifvideoFind : " + str(gifvideoFind))
                    if(gifvideoFind.find('.gif') != -1):
                        self.RenderGif(gifvideoFind, cheminPrevu=True)
                    elif(gifvideoFind.find('.mp4') != -1):
                        self.RenderVideo(gifvideoFind, cheminPrevu=True)
                #################################################################################
                ###### Recherche d'une possible animation en fonction d'un coup
                ###### special
                if('X' not in valeurs):
                    #print('Recherche coup special')
                    specialMoves = dMDRendererSpecialsMoves.SearchSpecialsMoves(valeurs)
                    #print('>> ' + str(specialMoves))
                    if(specialMoves != ""):
                        movePath = self.specialsMovesDirectory + str(specialMoves)
                        gifsMovesDisponibles = [f for f in listdir(movePath) if isfile(join(movePath, f))]
                        if(len(gifsMovesDisponibles) != 0):
                            gifMovesInd = random.randint(0, len(gifsMovesDisponibles) - 1)
                            gifMovesFind = movePath + '/' + gifsMovesDisponibles[gifMovesInd] 
                            if(gifMovesFind.find('.gif') != -1):
                                self.RenderGif(gifMovesFind, cheminPrevu=True)
                            elif(gifMovesFind.find('.mp4') != -1):
                                self.RenderVideo(gifMovesFind, cheminPrevu=True)
                        else:
                            self.RenderText(specialMoves.replace('_',' '))
                            time.sleep(2)

            if(sens != None and (sens == "left" or sens == "right")):
                charMax = 9999  

            if(sens != None and sens == "right"):
                textWanted = ' ' + textWanted
            elif(sens != None and sens == "left"):
                textWanted = textWanted + ' '
            
            while(0 > x or y > self.pictureHeight):

                # Create the font
                font = ImageFont.truetype(fontPath, fontSize)
            
                if(sens != None and (sens == "left" or sens == "right")):
                    width = line_width
                else:
                    width = self.pictureWidth

                # New image based on the settings defined above
                imag = Image.new("RGB", (width, self.pictureHeight), color=(int(rgbBackground[0]),int(rgbBackground[1]), int(rgbBackground[2])))

                # Interface to draw on the image
                draw_interface = ImageDraw.Draw(imag)

                # Wrap the `text` string into a list of `CHAR_LIMIT`-character
                # strings
                text_lines = wrap(textWanted, charMax)

                # Get the first vertical coordinate at which to draw text and
                # the
                # height of each line of text
                y, line_heights = self.get_y_and_heights(text_lines,
                        (width, self.pictureHeight),
                        self.pictureVerticalMargin,
                        font,
                        textWanted)

                # Draw each line of text
                for i, line in enumerate(text_lines):
                
                    # Calculate the horizontally-centered position at which to
                    # draw
                    # this line
                    line_width = font.getmask(line).getbbox()[2]
                    if(self.HDMIIsActive == '0'):
                        if(sens != None and (sens == "left" or sens == "right")):
                            x = 0
                        else:
                            x = ((int(width) - line_width) // 2)
                    else:
                        x = ((int(width) - line_width) // 2)

                    #draw_interface.rectangle((0, 0, self.pictureWidth,
                    #self.pictureHeight), fill="grey")
                    #draw_interface.rectangle((x, y, line_width,
                    #line_heights[0]), fill="green")
                    # Draw this line
                    draw_interface.text((x, y), line, font=font, fill=(int(rgb[0]),int(rgb[1]), int(rgb[2])))

                    if(self.HDMIIsActive == '1'):
                        if(x < 0):
                            break

                    # Move on to the height at which the next line should be
                    # drawn
                    # at
                    y += line_heights[i]
                    
                if(self.HDMIIsActive == '0'):
                    if(sens != None and (sens == "left" or sens == "right")):
                        if(y <= self.pictureHeight):
                            break

                if(0 > x):
                    fontSize = fontSize - degIncr

                if(y > self.pictureHeight):
                    fontSize = fontSize - degIncr

            if(backImagePNG != None):
                
                backImg = Image.open(str(backImagePNG))
                back_img = self.CreateImageCenterTextAndSetTransparencyWithSpecificBackground(textWanted, fontPath, int(fontSize), backImg)
                back_img = back_img.save("/Raspy2DMD/FontSizer/TextWithFontPNG.png")
                sleep(0.01)
                back_img = Image.open("/Raspy2DMD/FontSizer/TextWithFontPNG.png")
                sleep(0.01)
            
                if(self.HDMIIsActive == '0'):
                    self.SetImage(back_img)
                else:                
                    self.SetImageOnHDMI(back_img, refresh=True)
                    
                if (self.stopAffichage == True or _stopAffichage == True or _stopHDMIRequested == True):
                    _dmdAvailable = True
                    self.clear()
                    return False

                return True        

            if(sens != None):  
                
                if (sound != None and sound != ''):
                    self.RenderSound(sound, cheminPrevu=cheminPrevu)

                if(iterate == None):
                    boucle = 1
                else:
                    boucle = iterate
            
                if(sens == 'up' or sens == 'down'):
                    start = -(self.pictureHeight)
                elif(sens == 'right' or sens == 'left'):
                    start = -(imag.size[0])

                for t in range(0, int(boucle)):  
                    if(sens == 'up'):
                        # Bas vers Haut
                        if(self.HDMIIsActive == '0'):
                            for y in range(self.pictureHeight, start, -1):
                                #if self._stopeventcarrousel.isSet() :
                                #    return
                                if self.stopAffichage == True or _stopAffichage == True or _stopHDMIRequested == True :
                                    self.clear()
                                    _dmdAvailable = True
                                    return False
                                self.SetImage(imag, 0, y) 
                                sleep(0.04)
                        else:
                            self.SetImageOnHDMI(imag, arg="--up")
                    elif(sens == 'down'):
                        # Haut vers Bas
                        if(self.HDMIIsActive == '0'):
                            for y in range(start, self.pictureHeight, 1):
                                #if self._stopeventcarrousel.isSet() :
                                #    return
                                if self.stopAffichage == True or _stopAffichage == True or _stopHDMIRequested == True :
                                    self.clear()
                                    _dmdAvailable = True
                                    return False
                                self.SetImage(imag, 0, y)
                                sleep(0.04)
                        else:
                            self.SetImageOnHDMI(imag, arg="--down")
                    elif(sens == 'right'):
                        # Gauche vers Droite
                        if(self.HDMIIsActive == '0'):
                            for x in range(start, self.pictureWidth, 1):
                                #if self._stopeventcarrousel.isSet() :
                                #    return
                                if self.stopAffichage == True or _stopAffichage == True or _stopHDMIRequested == True :
                                    self.clear()
                                    _dmdAvailable = True
                                    return False
                                self.SetImage(imag, x, 0)
                                sleep(0.01)
                        else:
                            self.SetImageOnHDMI(imag, arg="--right")
                    elif(sens == 'left'):
                        # Droite vers Gauche
                        if(self.HDMIIsActive == '0'):
                            for x in range(self.pictureWidth, start, -1):
                                #if self._stopeventcarrousel.isSet() :
                                #    return
                                if self.stopAffichage == True or _stopAffichage == True or _stopHDMIRequested == True :
                                    self.clear()
                                    _dmdAvailable = True
                                    return False
                                self.SetImage(imag, x, 0)
                                sleep(0.01)
                        else:
                            self.SetImageOnHDMI(imag, arg="--left")
                    elif(sens == 'antirotate'):
                        # rotation anti horaire
                        if(self.HDMIIsActive == '0'):
                            y = 0
                            for x in range(0, 9, 1):
                                #if self._stopeventcarrousel.isSet() :
                                #    return
                                if self.stopAffichage == True or _stopAffichage == True or _stopHDMIRequested == True :
                                    self.clear()
                                    _dmdAvailable = True
                                    return False
                                imagRot = imag.rotate(y, resample=Image.BICUBIC)
                                y = y + 45
                                if(self.HDMIIsActive == '0'):
                                    self.SetImage(imagRot)
                                sleep(0.1)
                        else: 
                            self.SetImageOnHDMI(imag, arg="--antirotate")
                    elif(sens == 'rotate'):
                        # rotation horaire
                        if(self.HDMIIsActive == '0'):
                            y = 360
                            for x in range(0, 9, 1):
                                #if self._stopeventcarrousel.isSet() :
                                #    return
                                if self.stopAffichage == True or _stopAffichage == True or _stopHDMIRequested == True :
                                    self.clear()
                                    _dmdAvailable = True
                                    return False
                                imagRot = imag.rotate(y, resample=Image.BICUBIC)
                                y = y - 45
                                self.SetImage(imagRot)
                                sleep(0.1)                        
                        else: 
                            self.SetImageOnHDMI(imag, arg="--rotate")
                    elif(sens == 'flip'):
                        if(self.HDMIIsActive == '0'):
                            # flip
                            img1 = imag
                            if(self.HDMIIsActive == '0'):
                                self.SetImage(img1)
                            else: 
                                self.SetImageOnHDMI(img1)
                            sleep(0.009)

                            # On reduit
                            for height in range(self.pictureHeight, 0 , -1):
                                #if self._stopeventcarrousel.isSet() :
                                #    return
                                if self.stopAffichage == True or _stopAffichage == True or _stopHDMIRequested == True :
                                    self.clear()
                                    _dmdAvailable = True
                                    return False
                                img1 = imag.resize((self.pictureWidth, height))
                                if(self.HDMIIsActive == '0'):
                                    self.SetImage(img1)
                                else: 
                                    self.SetImageOnHDMI(img1)
                                sleep(0.009)

                            # On retourne
                            imag = imag.transpose(method=Image.FLIP_TOP_BOTTOM)
                            sleep(0.009)

                            # On deplie
                            for height in range(1, self.pictureHeight , 1):
                                #if self._stopeventcarrousel.isSet() :
                                #    return
                                if self.stopAffichage == True or _stopAffichage == True or _stopHDMIRequested == True :
                                    self.clear()
                                    _dmdAvailable = True
                                    return False
                                img1 = imag.resize((self.pictureWidth, height))
                                if(self.HDMIIsActive == '0'):
                                    self.SetImage(img1)
                                else: 
                                    self.SetImageOnHDMI(img1)
                                sleep(0.009)

                            # On reduit
                            yPos = 1    
                            for height in range(self.pictureHeight, 0 , -1):
                                #if self._stopeventcarrousel.isSet() :
                                #    return
                                if self.stopAffichage == True or _stopAffichage == True or _stopHDMIRequested == True :
                                    self.clear()
                                    _dmdAvailable = True
                                    return False
                                img1 = imag.resize((self.pictureWidth, height))
                                self.SetImage(img1, 0, yPos)
                                yPos = yPos + 1
                                sleep(0.009)  

                            # On retourne
                            imag = imag.transpose(method=Image.FLIP_TOP_BOTTOM)
                            sleep(0.009)

                            ## On deplie
                            yPos = self.pictureHeight    
                            for height in range(1, self.pictureHeight , 1):
                                #if self._stopeventcarrousel.isSet() :
                                #    return
                                if self.stopAffichage == True or _stopAffichage == True or _stopHDMIRequested == True :
                                    self.clear()
                                    _dmdAvailable = True
                                    return False
                                img1 = imag.resize((self.pictureWidth, height))
                                self.SetImage(img1, 0, yPos)
                                yPos = yPos - 1
                                sleep(0.009)
                            
                            if(self.HDMIIsActive == '0'):
                                self.SetImage(img1)
                            else: 
                                self.SetImageOnHDMI(img1)
                            sleep(0.009)
                        else: 
                            self.SetImageOnHDMI(imag, arg="--flip")

                    elif(sens == 'twirl'):
                        if(self.HDMIIsActive == '0'):
                            # twirl
                            img1 = imag
                            if(self.HDMIIsActive == '0'):
                                self.SetImage(img1)
                            else: 
                                self.SetImageOnHDMI(img1)
                            sleep(0.0045)

                            # On reduit
                            for width in range(self.pictureWidth, 0 , -1):
                                #if self._stopeventcarrousel.isSet() :
                                #    return
                                if self.stopAffichage == True or _stopAffichage == True or _stopHDMIRequested == True :
                                    self.clear()
                                    _dmdAvailable = True
                                    return False
                                img1 = imag.resize((width, self.pictureHeight))
                                if(self.HDMIIsActive == '0'):
                                    self.SetImage(img1)
                                else: 
                                    self.SetImageOnHDMI(img1)
                                sleep(0.0045)

                            # On retourne
                            imag = imag.transpose(method=Image.FLIP_LEFT_RIGHT)
                            sleep(0.0045)

                            # On deplie
                            for width in range(1, self.pictureWidth , 1):
                                #if self._stopeventcarrousel.isSet() :
                                #    return
                                if self.stopAffichage == True or _stopAffichage == True or _stopHDMIRequested == True :
                                    self.clear()
                                    _dmdAvailable = True
                                    return False
                                img1 = imag.resize((width, self.pictureHeight))
                                if(self.HDMIIsActive == '0'):
                                    self.SetImage(img1)
                                else: 
                                    self.SetImageOnHDMI(img1)
                                sleep(0.0045)
                        
                            # On reduit
                            xPos = 1
                            for width in range(self.pictureWidth, 0 , -1):
                                #if self._stopeventcarrousel.isSet() :
                                #    return
                                if self.stopAffichage == True or _stopAffichage == True or _stopHDMIRequested == True :
                                    self.clear()
                                    _dmdAvailable = True
                                    return False
                                img1 = imag.resize((width, self.pictureHeight))
                                self.SetImage(img1, xPos, 0)
                                xPos = xPos + 1
                                sleep(0.0045)

                            # On retourne
                            imag = imag.transpose(method=Image.FLIP_LEFT_RIGHT)
                            sleep(0.0045)

                            # On deplie
                            xPos = self.pictureWidth
                            for width in range(1, self.pictureWidth , 1):
                                #if self._stopeventcarrousel.isSet() :
                                #    return
                                if self.stopAffichage == True or _stopAffichage == True or _stopHDMIRequested == True :
                                    self.clear()
                                    _dmdAvailable = True
                                    return False
                                img1 = imag.resize((width, self.pictureHeight))
                                self.SetImage(img1, xPos, 0)
                                xPos = xPos - 1
                                sleep(0.0045)
                            
                            if(self.HDMIIsActive == '0'):
                                self.SetImage(img1)
                            else: 
                                self.SetImageOnHDMI(img1)
                            sleep(0.0045)
                        else: 
                            self.SetImageOnHDMI(imag, arg="--twirl")

            else:
                if (sound != None and sound != ''):
                    self.RenderSound(sound, cheminPrevu=cheminPrevu)

                if(self.HDMIIsActive == '0'):
                    self.SetImage(imag)
                else: 
                    self.SetImageOnHDMI(imag)


                img1 = None
                imag = None
                if (self.stopAffichage == True or _stopAffichage == True or _stopHDMIRequested == True):
                    _dmdAvailable = True
                    self.clear()
                    return False

            if(wait != None):
                #sleep(int(wait))
                i = 0
                while(i < (int(wait) * 1000)):
                    i = i + 1
                    if (self.stopAffichage == True or _stopAffichage == True or _stopHDMIRequested == True):
                        _dmdAvailable = True
                        self.clear()
                        return False
                    sleep(0.001)
                    
            if (self.stopAffichage == True or _stopAffichage == True or _stopHDMIRequested == True):
                _dmdAvailable = True
                self.clear()
                return False

            _dmdAvailable = True
            return True
                
        except Exception as e:
            raise Raspy2DMDException("Unhandled error in RenderText :\n{}".format(e))

    ###############################################
    #######    RENDER CAROUSSEL TEXT #############
    ###############################################
    def RenderCarrousel(self):

        try:
            global _dmdAvailable
            _dmdAvailable = False
            carouPath = self.textesDirectory

            dirlist = [ item for item in listdir(carouPath) if isdir(join(carouPath, item)) ]
            if(len(dirlist) != 0):
                dirInd = random.randint(0, len(dirlist) - 1)
                carouPath = carouPath + '/' + dirlist[dirInd]

            carouDisponibles = [f for f in listdir(carouPath) if isfile(join(carouPath, f))]
            if(len(carouDisponibles) != 0):
                carrouInd = random.randint(0, len(carouDisponibles) - 1)
                carrouWanted = carouPath + '/' + carouDisponibles[carrouInd] 
            else:
                return False


            if(isfile(carrouWanted)):
                sens = None
                iterate = None
                sensTexte = ['left','right','up','down','rotate','antirotate','flip','twirl']
                msg = ''
                opts = ''
                gifBackground = ''
                readIn = open(carrouWanted, "r", encoding = 'utf-8')
                lignes = readIn.readlines()
                if(len(lignes) != 0):
                    # La 1ere ligne est toujours la ligne des options
                    opts = lignes[0].strip()
                    lignes.pop(0)
                    # On s'assure avoir toujours du texte
                    if(len(lignes) != 0):
                        for ligne in lignes:
                            msg = msg + " " + ligne
                        lignes = readIn.close()

                        if (opts != ""):
                            for opt in opts.split(';'):
                                if opt == 'GD':
                                    sens = sensTexte[1]
                                elif opt == 'A':
                                    sens = random.choice(sensTexte)
                                elif opt == 'DG':
                                    sens = sensTexte[0]
                                elif opt == 'HB':
                                    sens = sensTexte[3]
                                elif opt == 'BH':
                                    sens = sensTexte[2]
                                elif opt == 'ROT':
                                    sens = sensTexte[4]
                                elif opt == 'ARO':
                                    sens = sensTexte[5]
                                elif opt == 'FLI':
                                    sens = sensTexte[6]
                                elif opt == 'TWI':
                                    sens = sensTexte[7]
                                elif opt.find('IT') != -1:
                                    iterate = opt.replace('IT','')
                                elif opt.find('♠') != -1:
                                    gifBackground = opt.replace('♠','')

                        if(gifBackground == ''):
                            retour = self.RenderText(msg, sens=sens, iterate=iterate)
                        else:
                            retour = self.RenderGifWithText(gifBackground, msg)

                        if(retour != True):
                            _dmdAvailable = True
                            return False

            else:
                _dmdAvailable = True
                return False
        
            if(opts == ""):
                i = 0
                while(i < (4000)):
                    i = i + 1
                    if (self.stopAffichage == True or _stopAffichage == True or _stopHDMIRequested == True):
                        self.clear()
                        _dmdAvailable = True
                        return False
                    sleep(0.001)

            else:        
                if (self.stopAffichage == True or _stopAffichage == True or _stopHDMIRequested == True):
                    self.clear()
                    _dmdAvailable = True
                    return False
            
            _dmdAvailable = True
            return True
        
        except Exception as e:
            raise Raspy2DMDException("Unhandled error in RenderCarrousel :\n{}".format(e))
    
    ###############################################
    #######    RENDER CAROUSSEL TEXT #############
    ###############################################
    def RenderCarrouselText(self, startOrStopCarrousel):
        try:
            if(startOrStopCarrousel == "start"):
                self.RenderCarrouselStart()
            else:
                self.RenderCarrouselStop()
        except Exception as e:
            raise Raspy2DMDException("Unhandled error in RenderCarrouselText :\n{}".format(e))

    def runCarrousel(self):
        try:
            carouPath = self.textesDirectory

            while (self.stopAffichage == False and _stopAffichage == False and _stopHDMIRequested == False):
                                    
                self.RenderCarrousel()

        except Exception as e:
            raise Raspy2DMDException("Unhandled error in runCarrousel :\n{}".format(e))

    def RenderCarrouselStart(self):
        try:
            self.runCarrou = threading.Thread(target=self.runCarrousel)
            self.runCarrou.start()
        except Exception as e:
            raise Raspy2DMDException("Unhandled error in RenderCarrouselStart :\n{}".format(e))
        
    def RenderCarrouselStop(self):
        try:
            self.clear()
        except Exception as e:
            raise Raspy2DMDException("Unhandled error in RenderCarrouselStop :\n{}".format(e))
    
    #######################################
    ####  Verif Carrousel Text dispo #####
    #######################################
    def VerifCarrouselTextDispo(self):
        try:
            carouselPath = self.textesDirectory
            # On cherche un possible sous dossier
            dirlist = [ item for item in listdir(carouselPath) if isdir(join(carouselPath, item)) ]
            if(len(dirlist) != 0):
                dirInd = random.randint(0, len(dirlist) - 1)
                carouselPath = carouselPath + '/' + dirlist[dirInd]
            # On cherche un possible texte
            carouDisponibles = [f for f in listdir(carouselPath) if isfile(join(carouselPath, f))]
            if(len(carouDisponibles) != 0):
                return True
            else:
                return False
        except Exception as e:
            raise Raspy2DMDException("Unhandled error in VerifCarrouselTextDispo :\n{}".format(e))

    #######################################
    #######    RENDER IMAGE ##############
    #######################################
    def RenderImage(self, imageWanted='', aleatoire=False, wait=None):

        try:
            global _dmdAvailable
            _dmdAvailable = False
            self.clear()

            if(imageWanted == 'WELK.OME'):
                if(self.HDMIIsActive == '0'):
                    imageWanted = '/Medias/RaspyDartsDMD/images/DMD/Raspy2DMD.png'
                else:
                    imageWanted = '/Medias/RaspyDartsDMD/images/HDMI/Raspy2DMD.png'


            if(aleatoire == False and imageWanted == ''):
                self.RenderText('Veuillez faire un choix pour votre image')
                _dmdAvailable = True
                return False

            if (aleatoire == True):    
                ##################################################################################
                ## On cherche un fichier dans les dossiers et sous dossier
                listDisponible = self.RandomFile(self.imagesDirectory)
                indRand = random.randint(0, len(listDisponible) - 1)
                sleep(0.01)
                imageWanted = listDisponible[indRand]     

            image = Image.open(imageWanted)

            image.thumbnail((self.pictureWidth, self.pictureHeight), Image.BICUBIC)

            #print(":> On affiche l'image : " + str(imageWanted))
            xPos = 0
            yPos = 0
            
            if(self.center_images == 1):
                if (image.size[0] < self.pictureWidth):                        
                    #print(f"Image size x ({image.size[0]}) plus petit que pictureWidth ({self.pictureWidth})")
                    middlePictureWidth = int(self.pictureWidth/2)
                    middleImageWidth = int(image.size[0]/2)
                    xPos = int(middlePictureWidth - middleImageWidth)

                if (image.size[1] < self.pictureHeight):                        
                    #print(f"Image size y ({image.size[1]}) plus petit que pictureHeight ({self.pictureHeight})")
                    middlePictureHeight = int(self.pictureHeight/2)
                    middleImageHeight = int(image.size[1]/2)
                    yPos = int(middlePictureHeight - middleImageHeight)

            if(self.HDMIIsActive == '0'):
                #self.SetImage(image.convert('RGB'), ((self.pictureWidth / 2) - (image.width / 2)))
                self.SetImage(image.convert('RGB'), xPos, yPos)
            else:
                self.SetImagePathOnHDMI(imageWanted)

            if(wait != None):
                i = 0
                while(i < (int(wait) * 1000)):
                    i = i + 1 
                    if (self.stopAffichage == True or _stopAffichage == True or _stopHDMIRequested == True):
                        self.clear()
                        _dmdAvailable = True
                        return False
                    sleep(0.001)
            
            if (self.stopAffichage == True or _stopAffichage == True or _stopHDMIRequested == True):
                self.clear()
                _dmdAvailable = True
                return False

            image = None
            
            _dmdAvailable = True
            return True 
        
        except Exception as e:
            raise Raspy2DMDException("Unhandled error in RenderImage :\n{}".format(e))
    
    #######################################
    #######   Verif Image dispo ##########
    #######################################
    def VerifImageDispo(self):
        try:
            imgPath = self.imagesDirectory
            # On cherche un possible sous dossier
            dirlist = [ item for item in listdir(imgPath) if isdir(join(imgPath, item)) ]
            if(len(dirlist) != 0):
                dirInd = random.randint(0, len(dirlist) - 1)
                imgPath = imgPath + '/' + dirlist[dirInd]
            # On cherche une possible image
            imagesDisponibles = [f for f in listdir(imgPath) if isfile(join(imgPath, f))]
            if(len(imagesDisponibles) != 0):
                return True
            else:
                return False
        except Exception as e:
            raise Raspy2DMDException("Unhandled error in VerifImageDispo :\n{}".format(e))
        
    ####################################################
    #######     RANDOM DIRECTORY AND FILE ##############
    ####################################################
    def GetAllDirectories(self, path, directoryListFind=[]): 
        try:
            if(directoryListFind == None):
                directoryList = []
            else:
                directoryList = directoryListFind
            obj = scandir(path)
            for enter in obj:
                if enter.is_dir():
                    directoryList.append({ "name" : str(enter.name), "path" : str(enter.path)})
                    directoryList = self.GetAllDirectories(enter.path, directoryList)
            obj.close()
        except Exception as e:
            raise Raspy2DMDException("Unhandled error in GetAllDirectories :\n{}".format(e))
        return directoryList

    def RandomFile(self, masterDirectory):

        dirList = []
        filesList = []
        ligne = []
        try:
            mesDossiers = self.GetAllDirectories(masterDirectory, [{ "name" : str(masterDirectory), "path" : str(masterDirectory)}])
            dossierExclu = json.loads(DMDRenderer_Database.select_folder_excluded(self.databasePath))
            fichierExclu = json.loads(DMDRenderer_Database.select_file_excluded(self.databasePath))
                    
            for dossier in mesDossiers:
                sleep(0.001)
                resultFolder = next((item for item in dossierExclu if (item['name'] == str(dossier["name"]) and item['path'] == str(dossier["path"]))), None)
                if resultFolder is None: 
                    obj = scandir(dossier["path"])
                    for enter in obj:
                        sleep(0.001)
                        if enter.is_file():                          
                            resultFile = next((item for item in fichierExclu if (item['name'] == str(enter.name) and item['path'] == str(enter.path))), None)
                            if resultFile is None:
                                ligne.append(str(enter.path))
                    obj.close()
        except Exception as e:
            raise Raspy2DMDException("Unhandled error in GetAllDirectories :\n{}".format(e))
        finally:
            resultFolder = None
            resultFile = None
            dossierExclu = None
            fichierExclu = None
            dirList = None
            filesList = None
            return ligne
        
    #######################################
    #######     RENDER GIF ##############
    #######################################
    def RenderGif(self, gifWanted='', aleatoire=False, cheminPrevu=False, sound=None):
        
        try:
            global _dmdAvailable
            _dmdAvailable = False
            self.clear()
            
            gifPath = self.gifsDirectory
            
            if(aleatoire == False and gifWanted == ''):
                self.RenderText('Veuillez faire un choix pour votre gif')
                _dmdAvailable = True
                return

            if(aleatoire == True):
                ##################################################################################
                ## On cherche un fichier dans les dossiers et sous dossier
                gifsDisponibles = self.RandomFile(gifPath)
                gifsInd = random.randint(0, len(gifsDisponibles) - 1)
                gifWanted = gifsDisponibles[gifsInd]

            if(cheminPrevu == True):
                if(not isfile(gifWanted)):
                    print(str(gifWanted) + ' non trouvé par chemin prevu')
            else:
                if(isfile(join(gifPath + '/' + gifWanted))):
                    gifWanted = gifPath + '/' + gifWanted
                else:
                    if(not isfile(gifWanted)):
                        print(str(gifWanted) + ' non trouvé')
                        _dmdAvailable = True
                        return True            

            if (self.HDMIIsActive == "0"):

                with WImage(filename=gifWanted) as img:

                    self.OptimizeGif(img, gifWanted)

                    if (sound != None and sound != ''):
                        self.RenderSound(sound, cheminPrevu=cheminPrevu)
                        
                    xPos = 0
                    yPos = 0

                    if(self.center_images == 1):
                        if (img.size[0] < self.pictureWidth):                        
                            middlePictureWidth = int(self.pictureWidth/2)
                            middleImageWidth = int(img.size[0]/2)
                            xPos = int(middlePictureWidth - middleImageWidth)

                        if (img.size[1] < self.pictureHeight):                        
                            middlePictureHeight = int(self.pictureHeight/2)
                            middleImageHeight = int(img.size[1]/2)
                            yPos = int(middlePictureHeight - middleImageHeight)

                    for i in range(len(img.sequence)):  

                        # Si on a stoppe le waiter
                        if (self.stopAffichage == True or _stopAffichage == True or _stopHDMIRequested == True):
                            img.sequence[i].destroy()
                            self.clear()
                            img.clear()
                            img.close()
                            _dmdAvailable = True
                            return False

                        ig = Image.fromarray(np.array(img.sequence[i])).convert('RGB')
                        self.SetImage(ig, xPos, yPos)
                        sleep(int(img.sequence[i].delay) / 100)
                        ig = None
                        img.sequence[i].destroy()

                    img.clear()
                    img.close()
            
                ##print(":> on finit l'affichage du Gif")
                self.clear()
                    
            else :

                if (self.stopAffichage == True or _stopAffichage == True or _stopHDMIRequested == True):
                    _dmdAvailable = True
                    self.clear()
                    return False

                self.SetGifOnHDMI(gifWanted)
                
            _dmdAvailable = True
            return True
        
        except Exception as e:
            raise Raspy2DMDException("Unhandled error in RenderGif :\n{}".format(e))

    def OptimizeGif(self, gif=None, gifName=None):
        try: 
            oldsize = None
            oldpage = None
            ind = 0
            for index, frame in enumerate(gif.sequence):
                if(oldsize == None):
                    oldsize = frame.size
                elif(oldsize != None):
                    if(frame.size != oldsize):
                        print(":> Le Gif '{0}' aurait besoin d'etre optimise! Les tailles d'images ne sont pas identique.".format(gifName))
                        gif.sequence[ind].destroy()
                        gif.coalesce()
                        return
                    else:
                        oldsize = frame.size
                if(oldpage == None):
                    oldpage = frame.page
                elif(oldpage != None):
                    if(frame.page != oldpage):
                        print(":> Le Gif '{0}' aurait besoin d'etre optimise! Les pages ne sont pas identique.".format(gifName))
                        gif.sequence[ind].destroy()
                        gif.coalesce()
                        return
                    else:
                        oldpage = frame.page
                gif.sequence[ind].destroy()
                ind = ind + 1
        except Exception as e:
            raise Raspy2DMDException("Unhandled error in OptimizeGif :\n{}".format(e))
    #######################################
    ######     RENDER GIF WITH TEXT ######
    #######################################
    def RenderGifWithText(self, gifWanted='', textWanted='', cheminPrevu=False, fontVoulue=None, sound=None):
        
        try:
            global _dmdAvailable
            _dmdAvailable = False
            self.clear()

            gifPath = self.gifsDirectory

            if(gifWanted == ''):
                print('Veuillez faire un choix pour votre gif')
                self.RenderText('Veuillez faire un choix pour votre gif')
                _dmdAvailable = True
                return

            if(textWanted == ''):
                print('Veuillez faire un choix pour votre gif')
                self.RenderText('Veuillez indiquer un texte a afficher')
                _dmdAvailable = True
                return

            if(cheminPrevu == True):
                if(not isfile(gifWanted)):
                    print(str(gifWanted) + ' non trouvé par chemin prevu')
                    _dmdAvailable = True
                    return
            else:
                if(isfile(join(gifPath + '/' + gifWanted))):
                    gifWanted = gifPath + '/' + gifWanted
                else:
                    if(not isfile(gifWanted)):
                        print(str(gifWanted) + ' non trouvé')
                        _dmdAvailable = True
                        return

            ###################################################################
            # Pour ecrire notre texte
            if(fontVoulue == None):
                fontPath = self.fontsTTFDirectory + self.defaultfont
            else:
                fontPath = self.fontsTTFDirectory + fontVoulue
            fontSize = self.maxFontSize
            charMax = self.maxCharacter
            width = 0
            rgb = self.defaultfontcolor.split(',')
            rgbBackground = self.pictureBackgroundColor.split(',')             

            soundPlayed = False
            with WImage(filename=gifWanted) as img:
                #img.coalesce()
                self.OptimizeGif(img, gifWanted)

                if (sound != None and sound != '' and soundPlayed == False):
                    self.RenderSound(sound, cheminPrevu=cheminPrevu)
                    soundPlayed = True

                for i in range(len(img.sequence)): 

                    # Si on a stoppe le waiter
                    if (self.stopAffichage == True or _stopAffichage == True or _stopHDMIRequested == True):
                        img.sequence[i].destroy()
                        img.destroy()
                        _dmdAvailable = True
                        self.clear()
                        imgFinal = None
                        im = None
                        imag = None
                        clip = None
                        frame = None
                        return False

                    arrayframe = np.array(img.sequence[i])
                    im = Image.fromarray(arrayframe).convert("RGBA")
                    im.thumbnail((self.pictureWidth, self.pictureHeight),Image.BICUBIC)

                    ###################################################################
                    x = -1
                    y = -1

                    while(0 > x or y > self.pictureHeight):

		                # Create the font
                        font = ImageFont.truetype(fontPath, fontSize)
                    
                        imag = Image.new("RGB", (width, self.pictureHeight), color=(255,255, 255)).convert("RGBA")
                        datas = imag.getdata()                    
                        newData = []  
                        for items in datas:
                            if items[0] == 255 and items[1] == 255 and items[2] == 255:
                                newData.append((255, 255, 255, 0))
                            else:
                                newData.append(item)
                        imag.putdata(newData)

                        width = self.pictureWidth

		                # Interface to draw on the image
                        draw_interface = ImageDraw.Draw(imag)

		                # Wrap the `text` string into a list of
		                # `CHAR_LIMIT`-character
		                # strings
                        text_lines = wrap(textWanted, charMax)

		                # Get the first vertical coordinate at which to draw text and
		                # the
		                # height of each line of text
                        y, line_heights = self.get_y_and_heights(text_lines,
			                (width, self.pictureHeight),
			                self.pictureVerticalMargin,
			                font,
			                textWanted)

		                # Draw each line of text
                        for i, line in enumerate(text_lines):

			                # Calculate the horizontally-centered position at which to
			                # draw
			                # this line
                            line_width = font.getmask(line).getbbox()[2]
                            x = ((int(width) - line_width) // 2)

                            # Draw this line
                            draw_interface.text((x, y), line, font=font, fill=(int(rgb[0]),int(rgb[1]), int(rgb[2])))

                            # Move on to the height at which the next line
                            # should
                            # be drawn at
                            y += line_heights[i]

                        if(0 > x):
                            fontSize = fontSize - 1

                        if(y > self.pictureHeight):
                            fontSize = fontSize - 1
                    ###################################################################
                
                    imgFinal = Image.new("RGBA", im.size)
                    imgFinal = Image.alpha_composite(imgFinal, im)
                    imgFinal = Image.alpha_composite(imgFinal, imag)
                    
                    self.SetImage(imgFinal.convert('RGB'), ((self.pictureWidth / 2) - (imgFinal.width / 2)))
                    sleep(int(img.sequence[i].delay) / 100)
                    img.sequence[i].destroy()

                    im = None
                    arrayframe = None
            
            _dmdAvailable = True
            img.destroy()
            self.clear()
            imgFinal = None
            im = None
            imag = None
            clip = None
            frame = None
            return True
        
        except Exception as e:
            raise Raspy2DMDException("Unhandled error in RenderGifWithText :\n{}".format(e))
    
    #######################################
    #######    Verif Gif dispo ##########
    #######################################
    def VerifGifDispo(self):
        try:
            gifPath = self.gifsDirectory
            dirlist = [ item for item in listdir(gifPath) if isdir(join(gifPath, item)) ]
            if(len(dirlist) != 0):
                dirInd = random.randint(0, len(dirlist) - 1)
                gifPath = gifPath + '/' + dirlist[dirInd]
            # On cherche un possible gif
            gifsDisponibles = [f for f in listdir(gifPath) if isfile(join(gifPath, f))]
            if(len(gifsDisponibles) != 0):
                return True
            else:
                return False
        except Exception as e:
            raise Raspy2DMDException("Unhandled error in VerifGifDispo :\n{}".format(e))

    # TODO On le fait disparaitre tranquillement
    ########################################
    ########    RENDER VIDEO ##############
    ########################################
    #def RenderVideo(self, videoWanted='', aleatoire=False, cheminPrevu=False):
    #    try:
    #        global _dmdAvailable
    #        _dmdAvailable = False
    #        self.clear()
        
    #        vidPath = self.videosDirectory

    #        if (videoWanted == '' and aleatoire == False):
    #            self.RenderText('Veuillez faire un choix pour votre video')
    #            _dmdAvailable = True
    #            return

    #        if (aleatoire == True):
    #            ##################################################################################
    #            ## On cherche un fichier dans les dossiers et sous dossier
    #            listDisponible = self.RandomFile(vidPath)
    #            indRand = random.randint(0, len(listDisponible) - 1)
    #            sleep(0.01)
    #            videoWanted = listDisponible[indRand]

    #        else:
    #            if(cheminPrevu == True):
    #                if(not isfile(videoWanted)):
    #                    print(str(videoWanted) + ' non trouvé')
    #                    _dmdAvailable = True
    #                    return True
    #            else:
    #                if(isfile(join(vidPath + '/' + videoWanted))):
    #                    videoWanted = vidPath + '/' + videoWanted
    #                    return True
    #                else :
    #                    if(not isfile(join(videoWanted))):
    #                        print(str(videoWanted) + ' non trouvé')
    #                        _dmdAvailable = True
    #                        return True
        
    #        if(self.HDMIIsActive == '0'):
    #            clip = VideoFileClip(videoWanted)
    #            FPS = clip.fps
    #            FRAMETIME = 1.0 / FPS

    #            for frame in clip.iter_frames():
    #                # Si on a stoppe le waiter
    #                if (self.stopAffichage == True or _stopAffichage == True
    #                or _stopHDMIRequested == True):
    #                    _dmdAvailable = True
    #                    self.clear()
    #                    im = None
    #                    clip = None
    #                    frame = None
    #                    return False
    #                while self._pauseevent:
    #                    self.waitLock()

    #                im = Image.fromarray(frame)
    #                im.thumbnail((self.pictureWidth,
    #                self.pictureHeight),Image.BICUBIC)
    #                self.SetImage(im.convert('RGB'), ((self.pictureWidth / 2)
    #                - (im.width / 2)))
    #                sleep(FRAMETIME)
        
    #            self.clear()
    #            im = None
    #            clip = None
    #            frame = None
    #            #self._stopevent.clear()

    #        else:
    #            self.SetVideoOnHDMI(videoWanted)
                
    #        _dmdAvailable = True
            
    #    except Exception as e:
    #        raise Raspy2DMDException("Unhandled error in RenderVideo
    #        :\n{}".format(e))

    #######################################
    #######   Verif Video dispo ##########
    #######################################
    def VerifVideoDispo(self):
        try:
            vidPath = self.videosDirectory
            dirlist = [ item for item in listdir(vidPath) if isdir(join(vidPath, item)) ]
            if(len(dirlist) != 0):
                dirInd = random.randint(0, len(dirlist) - 1)
                vidPath = vidPath + '/' + dirlist[dirInd]
            # On cherche une possible video
            videosDisponibles = [f for f in listdir(vidPath) if isfile(join(vidPath, f))]
            if(len(videosDisponibles) != 0):
                return True
            else:
                return False
        except Exception as e:
            raise Raspy2DMDException("Unhandled error in VerifVideoDispo :\n{}".format(e))
                
    #######################################
    #######    RENDER EFFET ##############
    #######################################
    def RenderEffet(self, id):
        try:
            effet = json.loads(DMDRenderer_Database.get_effet(self.databasePath, id))
            if(effet != []):
                texte = effet[0]["text"]
                gif = effet[0]["gif"]
                sound = effet[0]["sound"]
                self.RenderSoundEffet(texte, gif, sound)
        except Exception as e:
            raise Raspy2DMDException("Unhandled error in RenderEffet :\n{}".format(e))

    #######################################
    #######    RENDER SOUND EFFET ########
    #######################################
    def RenderSoundEffet(self, texte, gif, sound):
        try:
            if(gif != '' and texte != ''):
                self.RenderGifWithText(gif, texte, sound=sound)
            elif(str(texte) != ''):
                self.RenderText(texte, sound=sound)
            elif(str(gif) != ''):
                self.RenderGif(gif, sound=sound)
            elif(str(sound) != ''):
                self.RenderSound(sound)
        except Exception as e:
            raise Raspy2DMDException("Unhandled error in RenderSoundEffet :\n{}".format(e))

    #######################################
    #######   RENDER DEMO GIF ############
    #######################################
    def RenderDemoGif(self):
        try:
            self.clear()

            gifPath = self.gifsDirectory

            #################################################################################
            # On cherche un le ou les dossiers
            dirlist = [ item for item in listdir(gifPath) if isdir(join(gifPath, item)) ]
            for dir in dirlist:
                gifPath = gifPath + '/' + dir
                # On cherche un possible gif
                gifsDisponibles = [f for f in listdir(gifPath) if isfile(join(gifPath, f))]
                for gif in gifsDisponibles:

                    self.RenderText(gif)

                    sleep(0.5)

                    gifWanted = gifPath + '/' + gif

                    self.RenderGif(gifWanted,cheminPrevu=True)

                    sleep(0.5)

                    self.clear()
        except Exception as e:
            raise Raspy2DMDException("Unhandled error in RenderDemoGif :\n{}".format(e))
    
    #######################################
    #######    RENDER TIME ##############
    #######################################
    def RenderTime(self, startOrStopTime, only=None, testPattern=None):
        try:
            if(testPattern != None):
                self.RenderTimeDemo(only=only, testPattern=testPattern)
            else:
                if(only != None):
                    self.showOnlyForConfiguration = int(only)
                    self.RunTime(self.showOnlyForConfiguration, testPattern)
                else:
                    self.showOnlyForConfiguration = only

                    if(self.showing_datehours != 0):
                        if(startOrStopTime == "start"):
                            self.RenderTimeStart()
                        else:
                            self.RenderTimeStop()
        except Exception as e:
            raise Raspy2DMDException("Unhandled error in RenderTime :\n{}".format(e))

    def RenderTimeSec(self, testPattern=None):
        try:
            return self.RunTime()
        except Exception as e:
            raise Raspy2DMDException("Unhandled error in RenderTimeSec :\n{}".format(e))

    def RenderTimeStart(self, only=None):
        try:
            self.runTimer = threading.Thread(target=self.RunTime)
            self.runTimer.start()
        except Exception as e:
            raise Raspy2DMDException("Unhandled error in RenderTimeStart :\n{}".format(e))
        
    def RenderTimeStop(self):
        try:
            global _stopHDMIRequested
            _stopHDMIRequested = True
            self.clear()
        except Exception as e:
            raise Raspy2DMDException("Unhandled error in RenderTimeStop :\n{}".format(e))

    def RunTime(self, only=None, testPattern=None):
        try:
            global _dmdAvailable
            _dmdAvailable = False
            self.clear()

            flipflop = True          
            secondToWait = (int(self.timeShow_Date) + int(self.timeShow_Hours))

            if(testPattern == None):
                backgroundClockColor = str(self.patterns) + str(self.clockBackgroundImage)
            else:
                backgroundClockColor = str(self.patterns) + str(testPattern)

            i = 0
            j = 0

            while((i < (secondToWait * 1000)) and self.stopAffichage == False and _stopAffichage == False and _stopHDMIRequested == False):

                if(self.showing_datehours == 2):
                    if flipflop == True:
                        flipflop = False
                    else:
                        flipflop = True   

                    if flipflop == False:
                        self.CreateImageDate(backgroundClockColor) 
                        j = 0
                        while(j < (int(self.timeShow_Date) * 1000)):
                            j = j + 1
                            if (self.stopAffichage == True or _stopAffichage == True or _stopHDMIRequested == True):
                                self.clear()
                                _dmdAvailable = True
                                return False
                            sleep(0.001)
                        i = i + j
                    else:
                        now = datetime.now()
                        self.CreateImageHour(now, backgroundClockColor) 
                        j = 0
                        while(j < (int(self.timeShow_Hours) * 1000)):
                            j = j + 1
                            if(int(datetime.now().second) != int(now.second)):
                                now = datetime.now()
                                self.CreateImageHour(now, backgroundClockColor)
                            if (self.stopAffichage == True or _stopAffichage == True or _stopHDMIRequested == True):
                                self.clear()
                                _dmdAvailable = True
                                return False
                            sleep(0.001)
                        i = i + j

                elif(self.showing_datehours == 3):
                    self.CreateImageDate(backgroundClockColor)
                    j = 0
                    while(j < (int(self.timeShow_Date) * 1000)):
                        j = j + 1
                        if (self.stopAffichage == True or _stopAffichage == True or _stopHDMIRequested == True):
                            self.clear()
                            _dmdAvailable = True
                            return False
                        sleep(0.001)
                    i = i + j

                elif(self.showing_datehours == 4):
                    now = datetime.now()
                    self.CreateImageHour(now, backgroundClockColor) 
                    j = 0
                    while(j < (int(self.timeShow_Hours) * 1000)):
                        j = j + 1
                        if(int(datetime.now().second) != int(now.second)):
                            now = datetime.now()
                            self.CreateImageHour(now, backgroundClockColor)
                        if (self.stopAffichage == True or _stopAffichage == True or _stopHDMIRequested == True):
                            self.clear()
                            _dmdAvailable = True
                            return False
                        sleep(0.001)
                    i = i + j

                # Si on a stoppe le waiter
                if (self.stopAffichage == True or _stopAffichage == True or _stopHDMIRequested == True):
                    self.clear()
                    _dmdAvailable = True
                    return False

            self.StopHDMIPlayer(refreshOnly=True)
            _dmdAvailable = True
            return True
            
        except Exception as e:
            raise Raspy2DMDException("Unhandled error in RunTime :\n{}".format(e))
        
    #######################################
    #######    RENDER SOUND ##############
    #######################################
    def RenderSound(self, soundWanted='', aleatoire=False, cheminPrevu=False):
        try:
        
            soundPath = self.soundsDirectory

            if (soundWanted == '' and aleatoire == False):
                self.RenderText('Veuillez faire un choix pour votre son')
                return

            if (aleatoire == True):       
                ##################################################################################
                ## On cherche un fichier dans les dossiers et sous dossier
                listDisponible = self.RandomFile(soundPath)
                indRand = random.randint(0, len(listDisponible) - 1)
                sleep(0.01)
                soundWanted = listDisponible[indRand]

            else:
                if(cheminPrevu == True):
                    if(not isfile(soundWanted)):
                        print(str(soundWanted) + ' non trouvé')
                        self.RenderText(str(soundWanted) + ' non trouvé')
                        return
                else:
                    if(isfile(join(soundPath + '/' + soundWanted))):
                        soundWanted = soundPath + '/' + soundWanted
                    else :
                        if(not isfile(join(soundWanted))):
                            print(str(soundWanted) + ' non trouvé')
                            self.RenderText(str(soundWanted) + ' non trouvé')
                            return

            self.PlaySound(sound=soundWanted)
            
        except Exception as e:
            raise Raspy2DMDException("Unhandled error in RenderSound :\n{}".format(e))

    #######################################
    def SetImage(self, image, x=0, y=0):
        try:
            #self.matrix.SetImage(image, x, y, unsafe=False) 
            self.matrix.SetImage(image, x, y)
        except Exception as e:
            raise Raspy2DMDException("Unhandled error in SetImage :\n{}".format(e))

    #######################################
    def SetImageOnHDMI(self, image, refresh=False, arg=""):
        try:
            imageName = 'monimage.png'
            if(image != None):
                if(refresh != False):
                    imageName = 'dateheure.png'
                image.save('/Raspy2DMD/HDMI/MonImage/' + imageName, optimize=True)
                subprocess.run(['sudo', 'sh', '/Raspy2DMD/HDMI/ShowImageOnHDMI.sh', '/Raspy2DMD/HDMI/MonImage/' + imageName, arg])
        except Exception as e:
            raise Raspy2DMDException("Unhandled error in SetImageOnHDMI :\n{}".format(e))

    #######################################
    def SetImagePathOnHDMI(self, image):
        try:
            subprocess.run(['sudo', 'sh', '/Raspy2DMD/HDMI/ShowImageOnHDMI.sh', image])
        except Exception as e:
            raise Raspy2DMDException("Unhandled error in SetImagePathOnHDMI :\n{}".format(e))

    #######################################
    def SetGifOnHDMI(self, image):
        try:
            subprocess.run(['sudo', 'sh', '/Raspy2DMD/HDMI/ShowOnHDMI.sh', image])
        except Exception as e:
            raise Raspy2DMDException("Unhandled error in SetGifOnHDMI :\n{}".format(e))

    #######################################
    def SetVideoOnHDMI(self, image):
        try:
            subprocess.run(['sudo', 'sh', '/Raspy2DMD/HDMI/ShowVideoOnHDMI.sh', image])
        except Exception as e:
            raise Raspy2DMDException("Unhandled error in SetVideoOnHDMI :\n{}".format(e))

    #######################################
    def PlaySound(self, sound=None, output=None):
        try:
            if(output == None):
                output = self.outputSon
            if(sound == None):
                sound = '/Raspy2DMD/sound/sound.mp3'
            #subprocess.Popen(['sudo', 'sh', '/Raspy2DMD/sound/PlaySound.sh', str(output), str(sound)])
            subprocess.run(['sudo', 'sh', '/Raspy2DMD/sound/PlaySound.sh', str(output), str(sound)])
        except Exception as e:
            raise Raspy2DMDException("Unhandled error in PlaySound :\n{}".format(e))
        
    #######################################
    def StopSound(self):
        try:
            subprocess.run(['sudo', 'sh', '/Raspy2DMD/sound/StopSound.sh'])
        except Exception as e:
            raise Raspy2DMDException("Unhandled error in PlaySound :\n{}".format(e))

    #######################################
    def init(self): 
        try:
            self.matrix = RGBMatrix(options = self.GetOptions())

        except Exception as e:
            raise Raspy2DMDException("Unhandled error in init :\n{}".format(e))

    #######################################
    def clear(self): 
        try:

            # On efface l'image presente
            image = Image.new("RGB", (self.pictureWidth, self.pictureHeight), color=(0,0,0))
            if(self.HDMIIsActive == '0'):
                self.SetImage(image)             
            else:
                self.SetImageOnHDMI(None)

        except Exception as e:
            raise Raspy2DMDException("Unhandled error in clear :\n{}".format(e))

    #######################################
    def get_y_and_heights(self, text_wrapped, dimensions, margin, font, text):
        try:
            ascent, descent = font.getmetrics()
            (width, baseline), (offset_x, offset_y) = font.font.getsize(text)
        
            # Calculate the height needed to draw each line of text (including
            # its
            # bottom margin)
            line_heights = [
                (ascent - offset_y) + descent + offset_y
                for text_line in text_wrapped
            ]

            # Total height needed
            height_text = sum(line_heights)

            # Calculate the Y coordinate at which to draw the first line of
            # text
            y = (dimensions[1] - height_text) // 2

            # Return the first Y coordinate and a list with the height of each
            # line
            return (y, line_heights)
        except Exception as e:
            raise Raspy2DMDException("Unhandled error in get_y_and_heights :\n{}".format(e))

    #######################################
    def get_y_and_heightsNOSPACE(self, text_wrapped, dimensions, margin, font, text):
        try:
            ascent, descent = font.getmetrics()
            (width, baseline), (offset_x, offset_y) = font.font.getsize(text)
        
            # Calculate the height needed to draw each line of text (including
            # its
            # bottom margin)
            line_heights = [
                (ascent - offset_y) + descent + 1
                for text_line in text_wrapped
            ]

            # Total height needed
            height_text = sum(line_heights)

            # Calculate the Y coordinate at which to draw the first line of
            # text
            y = (dimensions[1] - height_text) // 2

            # Return the first Y coordinate and a list with the height of each
            # line
            return (y, line_heights)
        except Exception as e:
            raise Raspy2DMDException("Unhandled error in get_y_and_heightsNOSPACE :\n{}".format(e))
    
    #######################################
    # Create image with text and background transparent
    def CreateImageCenterTextAndSetTransparencyWithSpecificBackground(self, textVoulue, fontPath, fontSize, imageBackground, pictureWidth=None, pictureHeight=None):

        try:
            x = -1
            y = -1
            
            if(self.HDMIIsActive == '1'):
                fontSize = int((self.pictureWidth * 0.032) * (self.pictureHeight * 0.032) / 4)

            if pictureWidth == None :
                pictureWidth = self.pictureWidth
            else:
                pictureWidth = int(pictureWidth)

            if pictureHeight == None :
                pictureHeight = self.pictureHeight
            else:
                pictureHeight = int(pictureHeight)

            while(0 > x or y > pictureHeight):

                backgroundImage = imageBackground
                backgroundImage = backgroundImage.resize((pictureWidth, pictureHeight))
                mask = Image.new('RGBA', (pictureWidth, pictureHeight),(0, 0, 0, 255))
                
			    # Create the font
                font = ImageFont.truetype(fontPath, fontSize)

			    # Interface to draw on the image
                draw_interface = ImageDraw.Draw(mask)

			    # Wrap the `text` string into a list of `CHAR_LIMIT`-character strings
                text_lines = wrap(textVoulue, self.maxCharacter)

			    # Get the first vertical coordinate at which to draw text and the height
			    # of
			    # each line of text
                y, line_heights = self.get_y_and_heights(text_lines,
				    (pictureWidth, pictureHeight),
				    self.pictureVerticalMargin,
				    font,
				    textVoulue)

			    # Draw each line of text
                for i, line in enumerate(text_lines):

				    # Calculate the horizontally-centered position at which to draw this
				    # line
                    line_width = font.getmask(line).getbbox()[2]
                    x = ((int(pictureWidth) - line_width) // 2)

				    # Draw this line
                    draw_interface.text((x, y), line, font=font, fill=(0, 0, 0, 0))

				    # Move on to the height at which the next line should be drawn at
                    y += line_heights[i]

                if(0 > x):
                    fontSize = fontSize - 1

                if(y > pictureHeight):
                    fontSize = fontSize - 1

                sleep(0.01)

            backgroundImage.paste(mask, (0, 0), mask)
            return backgroundImage
        
        except Exception as e:
            raise Raspy2DMDException("Unhandled error in CreateImageCenterTextAndSetTransparencyWithSpecificBackground :\n{}".format(e))
    
    #######################################
    # Create image with text and background transparent
    def CreateImageCenterTextAndSetTransparency(self, textVoulue, fontPath, fontSize, rgb, pictureWidth=None, pictureHeight=None):

        try:
            x = -1
            y = -1
            
            #if(self.HDMIIsActive == '1'):
            #    fontSize = int((self.pictureWidth * 0.032) *
            #    (self.pictureHeight * 0.032) / 4)

            if pictureWidth == None :
                pictureWidth = self.pictureWidth
            else:
                pictureWidth = int(pictureWidth)

            if pictureHeight == None :
                pictureHeight = self.pictureHeight
            else:
                pictureHeight = int(pictureHeight)

            while(0 > x or y > pictureHeight):

                imag = Image.new("RGB", size=(pictureWidth, pictureHeight), color=(255,255,255)).convert("RGBA")
                # Set transparency
                datas = imag.getdata() 
                newData = []  
                for items in datas:
                    if items[0] == 255 and items[1] == 255 and items[2] == 255:
                        newData.append((255, 255, 255, 0))
                    else:
                        newData.append(items)
                imag.putdata(newData)

			    # Create the font
                font = ImageFont.truetype(fontPath, fontSize)

			    # Interface to draw on the image
                draw_interface = ImageDraw.Draw(imag)

			    # Wrap the `text` string into a list of `CHAR_LIMIT`-character strings
                text_lines = wrap(textVoulue, self.maxCharacter)

			    # Get the first vertical coordinate at which to draw text and the height
			    # of
			    # each line of text
                y, line_heights = self.get_y_and_heights(text_lines,
				    (pictureWidth, pictureHeight),
				    self.pictureVerticalMargin,
				    font,
				    textVoulue)

			    # Draw each line of text
                for i, line in enumerate(text_lines):

				    # Calculate the horizontally-centered position at which to draw this
				    # line
                    line_width = font.getmask(line).getbbox()[2]
                    x = ((int(pictureWidth) - line_width) // 2)

				    # Draw this line
                    draw_interface.text((x, y), line, font=font, fill=(int(rgb[0]),int(rgb[1]), int(rgb[2])))

				    # Move on to the height at which the next line should be drawn at
                    y += line_heights[i]

                if(0 > x):
                    fontSize = fontSize - 1

                if(y > pictureHeight):
                    fontSize = fontSize - 1

                sleep(0.01)

            return imag
        
        except Exception as e:
            raise Raspy2DMDException("Unhandled error in CreateImageCenterTextAndSetTransparency :\n{}".format(e))
    
    #######################################
    # Create image with text and background transparent
    def CreateImageCenterTextAndSetTransparencyNOSPACE(self, textVoulue, fontPath, fontSize, rgb, pictureWidth=None, pictureHeight=None):

        try:
            x = -1
            y = -1
            
            #if(self.HDMIIsActive == '1'):
            #    fontSize = int((self.pictureWidth * 0.032) *
            #    (self.pictureHeight * 0.032))

            if pictureWidth == None :
                pictureWidth = self.pictureWidth
            else:
                pictureWidth = int(pictureWidth)

            if pictureHeight == None :
                pictureHeight = self.pictureHeight
            else:
                pictureHeight = int(pictureHeight)

            while(0 > x or y > pictureHeight):

                imag = Image.new("RGB", size=(pictureWidth, pictureHeight), color=(255,255,255)).convert("RGBA")
                # Set transparency
                datas = imag.getdata() 
                newData = []  
                for items in datas:
                    if items[0] == 255 and items[1] == 255 and items[2] == 255:
                        newData.append((255, 255, 255, 0))
                    else:
                        newData.append(items)
                imag.putdata(newData)

			    # Create the font
                font = ImageFont.truetype(fontPath, fontSize)

			    # Interface to draw on the image
                draw_interface = ImageDraw.Draw(imag)

			    # Wrap the `text` string into a list of `CHAR_LIMIT`-character strings
                text_lines = wrap(textVoulue, self.maxCharacter)

			    # Get the first vertical coordinate at which to draw text and the height
			    # of
			    # each line of text
                y, line_heights = self.get_y_and_heightsNOSPACE(text_lines,
				    (pictureWidth, pictureHeight),
				    self.pictureVerticalMargin,
				    font,
				    textVoulue)

			    # Draw each line of text
                for i, line in enumerate(text_lines):

				    # Calculate the horizontally-centered position at which to draw this
				    # line
                    line_width = font.getmask(line).getbbox()[2]
                    x = ((int(pictureWidth) - line_width) // 2)

				    # Draw this line
                    draw_interface.text((x, y), line, font=font, fill=(int(rgb[0]),int(rgb[1]), int(rgb[2])))

				    # Move on to the height at which the next line should be drawn at
                    y += line_heights[i]

                if(0 > x):
                    fontSize = fontSize - 1

                if(y > pictureHeight):
                    fontSize = fontSize - 1

                sleep(0.01)

            return imag
        
        except Exception as e:
            raise Raspy2DMDException("Unhandled error in CreateImageCenterTextAndSetTransparencyNOSPACE :\n{}".format(e))

